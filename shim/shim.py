#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import logging
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Any, Dict, List, Optional
from zipfile import BadZipFile

from pyre_extensions import none_throws, safe_json

from .exit_codes import ClientError, ConfigurationError, ExitCode

try:
    from ..facebook.shim import configuration
    from ..facebook.shim.third_party_utils import start_third_party_analysis
except Exception:
    # pyre-ignore
    from . import configuration

import pyredex


LOG: logging.Logger = logging.getLogger(__name__)


def _path_exists(path: str) -> str:
    path = os.path.expanduser(path)
    if not os.path.exists(path):
        raise argparse.ArgumentTypeError(f"Path `{path}` does not exist.")
    return os.path.realpath(path)


def _directory_exists(path: str) -> str:
    path = os.path.expanduser(path)
    if not os.path.isdir(path):
        raise argparse.ArgumentTypeError(f"Path `{path}` is not a directory.")
    path = os.path.realpath(path)
    if path and path[-1] != "/":
        path = path + "/"
    return path


def _separated_paths_exist(paths: Optional[str]) -> Optional[str]:
    if paths is None:
        return None

    elements = paths.split(";")
    return ";".join([_path_exists(element) for element in elements])


def _check_executable(path: Path) -> Path:
    if not (path.exists() and os.access(path, os.X_OK)):
        raise ConfigurationError(message=f"Invalid binary `{path}`.")
    return path


def _system_jar_configuration_path(input: str) -> str:
    if input.endswith(".json"):
        path = _path_exists(input)
        with open(path) as file:
            try:
                paths = safe_json.load(file, List[str])
                return ";".join(paths)
            except safe_json.InvalidJson:
                raise argparse.ArgumentTypeError(
                    f"`{path}` must contain a list of strings"
                )

    # Validation deferred to backend if we pass `;` separated list of paths
    # because they are allowed to not exist.
    return input


def _heuristics_json_config_exists(input: str) -> str:
    path = _path_exists(input)
    with open(path) as file:
        try:
            # Heuristics are a list of key-value pairs.
            safe_json.load(file, Dict[str, Any])
            return path
        except safe_json.InvalidJson:
            raise argparse.ArgumentTypeError(
                f"`{path}` must be a valid JSON file with key-value pairs"
            )


class ExtractJexException(ClientError):
    pass


def _extract_jex_file_if_exists(path: Path, target: str, build_directory: Path) -> Path:
    jex_extract_directory: Path = Path(build_directory) / "jex"

    def run_unzip_command(command: List[str]) -> Path:
        output = subprocess.run(command, stderr=subprocess.PIPE)
        if output.returncode != 0:
            stderr = output.stderr.decode()
            if (
                command[0] == "unzip"
                and output.returncode == 1
                and "extra bytes at beginning or within zipfile" in stderr
            ):
                # This warning is fine to ignore, because *.jar files have been properly extracted
                LOG.warning(f"`unzip` warning: {stderr}")
            else:
                command_string = " ".join(command)
                raise ExtractJexException(
                    f"Unable to extract binary file `{path}` with command `{command_string}`:\n{stderr}"
                )

        jar_file_path = jex_extract_directory / (
            target.rsplit(":", maxsplit=1)[1] + ".jar"
        )
        if jar_file_path.exists():
            return jar_file_path
        else:
            raise ConfigurationError(
                message=f"Could not find jar file `{jar_file_path}` in `{jex_extract_directory}`.",
            )

    # If the target is java_binary, then the output is a JEX file
    if path.suffix != ".jex":
        return path

    # Try to extract *.jex files with various unzip tools, until one of them succeeds. See D22579374 for why doing this
    try:
        return run_unzip_command(
            ["unsquashfs", "-d", str(jex_extract_directory), "-o", "4096", str(path)]
        )
    except ExtractJexException:
        LOG.warning(f"Running `unsquashfs` on file `{path}` failed.")
        LOG.warning(f"Trying to extract file `{path}` with `unzip`.")
        return run_unzip_command(["unzip", "-d", str(jex_extract_directory), str(path)])


def _build_target(target: str, *, mode: Optional[str] = None) -> Path:
    LOG.info(f"Building `{target}`%s...", f" with `{mode}`" if mode else "")

    # If a target starts with fbcode, then it needs to be built from fbcode instead of from fbsource
    if ":" not in target:
        LOG.warning(
            f"Target `{target}` is an alias. Please expand it if it is a fbcode target. Otherwise buck build will fail."
        )
    fbcode_target_prefix = "fbcode//"
    is_fbcode_target = target.startswith(fbcode_target_prefix)
    if is_fbcode_target:
        target = target[len(fbcode_target_prefix) :]

    command = ["buck2", "build", "--show-json-output"]
    if mode:
        command.append(str(mode))
    command.append(target)
    current_working_directory = Path(os.getcwd())
    working_directory = (
        current_working_directory / "fbcode"
        if is_fbcode_target
        else current_working_directory
    )
    output = subprocess.run(command, capture_output=True, cwd=working_directory)
    if output.returncode != 0:
        raise ClientError(
            message=f"Error while building buck target `{target}`, aborting.\nstderr:{output.stderr.decode()}",
            exit_code=ExitCode.BUCK_ERROR,
        )

    try:
        response = json.loads(output.stdout)
    except json.JSONDecodeError:
        response = {}

    if len(response) != 1 or len(next(iter(list(response.values())))) == 0:
        raise ClientError(
            message=f"Unexpected buck output:\n{output.stdout.decode()}",
            exit_code=ExitCode.BUCK_ERROR,
        )

    # Use current_working_directory instead of working_directory.
    # Path should be relative to fbsource/ rather than fbcode/
    return current_working_directory / next(iter(list(response.values())))


def _build_executable_target(target: str, *, mode: Optional[str] = None) -> Path:
    return _check_executable(_build_target(target, mode=mode))


def _get_analysis_binary(arguments: argparse.Namespace) -> Path:
    from_arguments = arguments.binary
    if from_arguments:
        # Use the user-provided binary.
        return _check_executable(Path(from_arguments))

    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
    buck_target = configuration.BINARY_BUCK_TARGET
    if buck_target:
        # Build the mariana-trench binary from buck (facebook-only).
        return _build_executable_target(
            buck_target,
            mode=arguments.build,
        )

    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
    path_command = configuration.BINARY_PATH_COMMAND
    if path_command:
        # Find the mariana-trench binary in the path (open-source).
        command = shutil.which(path_command)
        if command is None:
            raise ConfigurationError(
                message=f"Could not find `{path_command}` in PATH.",
            )
        return Path(command)

    raise ConfigurationError(
        message="Could not find the analyzer binary.",
    )


def _desugar_jar_file(jar_path: Path) -> Path:
    LOG.info(f"Desugaring `{jar_path}`...")
    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
    desugar_tool = _build_target(none_throws(configuration.DESUGAR_BUCK_TARGET))
    desugared_jar_file = jar_path.parent / (jar_path.stem + "-desugared.jar")

    with tempfile.NamedTemporaryFile() as temp_file:
        # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
        for skipped_class in configuration.get_skipped_classes():
            temp_file.write(f"{skipped_class}\n".encode())
        temp_file.flush()
        output = subprocess.run(
            [
                "java",
                "-jar",
                desugar_tool,
                os.fspath(jar_path),
                os.fspath(desugared_jar_file),
                os.fspath(temp_file.name),
            ],
            stderr=subprocess.PIPE,
        )
        if output.returncode != 0:
            raise ClientError(
                message=f"Error while desugaring jar file, aborting.\nstderr: {output.stderr.decode()}",
                exit_code=ExitCode.JAVA_TARGET_ERROR,
            )

    LOG.info(f"Desugared jar file: `{desugared_jar_file}`.")
    return desugared_jar_file


def _build_apk_from_jar(jar_path: Path) -> Path:
    _, dex_file = tempfile.mkstemp(suffix=".jar")

    LOG.info(f"Running d8 on `{jar_path}`...")
    output = subprocess.run(
        [
            "buck2",
            "run",
            # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
            configuration.get_d8_target(),
            "--",
            "--output",
            dex_file,
            "--min-api",
            "25",  # mininum api 25 corresponds to dex 37
            jar_path,
        ],
        stderr=subprocess.PIPE,
    )
    if output.returncode != 0:
        raise ClientError(
            message=f"Error while running d8, aborting.\nstderr:{output.stderr.decode()}",
            exit_code=ExitCode.JAVA_TARGET_ERROR,
        )

    return Path(dex_file)


class VersionAction(argparse.Action):
    def __call__(self, parser: argparse.ArgumentParser, *_: Any) -> None:
        from . import package

        print(f"{package.name} {package.version}")
        print("Copyright (c) Meta Platforms, Inc. and affiliates.")
        parser.exit()


def _add_target_arguments(parser: argparse.ArgumentParser) -> None:
    target_arguments = parser.add_argument_group("Target arguments")
    target_arguments.add_argument(
        "--apk-path",
        type=_path_exists,
        help="The APK to analyze.",
    )
    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
    if configuration.FACEBOOK_SHIM:
        target_arguments.add_argument(
            "--java-target",
            type=str,
            help="The java buck target to analyze. If the target is `java_library`, append `-javaX` were X is the java version (e.g. 11).",
        )
        target_arguments.add_argument(
            "--java-mode",
            type=str,
            help="The buck mode for building the java target.",
        )


def _add_output_arguments(parser: argparse.ArgumentParser) -> None:
    output_arguments = parser.add_argument_group("Output arguments")
    output_arguments.add_argument(
        "--output-directory",
        type=_directory_exists,
        default=".",
        help="The directory to store results in.",
    )
    output_arguments.add_argument(
        "--generated-models-directory",
        type=_directory_exists,
        help="Save generated models to this directory.",
    )


def _add_binary_arguments(parser: argparse.ArgumentParser) -> None:
    binary_arguments = parser.add_argument_group("Analysis binary arguments")
    binary_arguments.add_argument(
        "--binary", type=str, help="The Mariana Trench binary."
    )
    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
    if configuration.FACEBOOK_SHIM:
        binary_arguments.add_argument(
            "--build",
            type=str,
            # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
            default=none_throws(configuration.BINARY_BUCK_BUILD_MODE),
            metavar="BUILD_MODE",
            help="The Mariana Trench binary buck build mode.",
        )


def _add_configuration_arguments(parser: argparse.ArgumentParser) -> None:
    configuration_arguments = parser.add_argument_group("Configuration arguments")
    configuration_arguments.add_argument(
        "--system-jar-configuration-path",
        type=_system_jar_configuration_path,
        help="A JSON configuration file with a list of paths to the system jars "
        + "or a `;` separated list of jars.",
    )
    configuration_arguments.add_argument(
        "--rules-paths",
        type=str,
        help="A `;`-separated list of rules files and directories containing rules files.",
    )
    configuration_arguments.add_argument(
        "--repository-root-directory",
        type=_directory_exists,
        default=".",
        help="The root of the repository. Resulting paths will be relative to this.",
    )
    configuration_arguments.add_argument(
        "--source-root-directory",
        type=_directory_exists,
        default=".",
        help="The root where source files for the APK can be found.",
    )
    configuration_arguments.add_argument(
        "--source-exclude-directories",
        type=str,
        default=None,
        help="A `;`-separated list of directories that should be excluded from indexed source files.",
    )
    configuration_arguments.add_argument(
        "--grepo-metadata-path",
        type=str,
        default=None,
        help="A json file containing grepo metadata for source file indexing.",
    )
    configuration_arguments.add_argument(
        "--proguard-configuration-paths",
        type=_separated_paths_exist,
        default=None,
        help="A `;`-separated list of ProGuard configurations, which can be used for global inference and to ignore unreachable objects.",
    )
    configuration_arguments.add_argument(
        "--disable-global-type-analysis",
        action="store_true",
        help="Disables global type analysis. If a proguard configuration path is passed in, it will be ignored.",
    )
    configuration_arguments.add_argument(
        "--verify-expected-output",
        action="store_true",
        help="Verifies special @Expected* annotations against the analysis output.",
    )
    configuration_arguments.add_argument(
        "--remove-unreachable-code",
        action="store_true",
        help="Prune unreachable code based on entry points specified in proguard configuration.",
    )
    configuration_arguments.add_argument(
        "--lifecycles-paths",
        type=_separated_paths_exist,
        default=None,
        help="A `;`-separated list of files and directories containing lifecycle definitions.",
    )
    configuration_arguments.add_argument(
        "--graphql-metadata-paths",
        type=str,
        default=None,
        help="A json file containing graphql metadata mapping information.",
    )
    configuration_arguments.add_argument(
        "--third-party-library-package-ids-path",
        type=str,
        default=None,
        help="A json file containing list of third party library package ids.",
    )
    configuration_arguments.add_argument(
        "--shims-paths",
        type=_separated_paths_exist,
        default=None,
        help="A `;`-separated list of files and directories containing shim definitions.",
    )
    configuration_arguments.add_argument(
        "--model-generator-configuration-paths",
        type=_separated_paths_exist,
        help="""A `;`-separated list of paths specifying JSON configuration files. Each file is a list of paths
        to JSON model generators relative to the configuration file or names of CPP model generators.""",
    )
    configuration_arguments.add_argument(
        "--model-generator-search-paths",
        type=_separated_paths_exist,
        help="A `;`-separated list of paths where we look up JSON model generators.",
    )
    configuration_arguments.add_argument(
        "--models-paths",
        type=_separated_paths_exist,
        help="A `;` separated list of models files and directories containing models files.",
    )
    configuration_arguments.add_argument(
        "--field-models-paths",
        type=_separated_paths_exist,
        help="A `;` separated list of field models files and directories containing field models files.",
    )
    configuration_arguments.add_argument(
        "--literal-models-paths",
        type=_separated_paths_exist,
        help="A `;` separated list of literal models files and directories containing literal models files.",
    )
    configuration_arguments.add_argument(
        "--sharded-models-directory",
        type=_directory_exists,
        help="Cached models from a separate analysis run, to be pre-loaded into the analysis.",
    )
    configuration_arguments.add_argument(
        "--maximum-source-sink-distance",
        type=int,
        # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
        default=configuration.DEFAULT_MAXIMUM_SOURCE_SINK_DISTANCE,
        help="Limits the distance of sources and sinks from a trace entry point.",
    )
    configuration_arguments.add_argument(
        "--emit-all-via-cast-features",
        action="store_true",
        help=(
            "Compute and emit all via-cast features. There can be many such "
            "features which slows down the analysis so it is disabled by "
            "default. Use this to enable it."
        ),
    )
    configuration_arguments.add_argument(
        "--allow-via-cast-feature",
        type=str,
        action="append",
        help=(
            "Compute only these via-cast features. Specified as the full "
            "type name, e.g. Ljava/lang/Object;. Multiple inputs allowed. "
            "Use --emit-all-via-cast-features to allow everything."
        ),
    )
    configuration_arguments.add_argument(
        "--propagate-across-arguments",
        action="store_true",
        help=(
            "Enable taint propagation across object type arguments. By "
            "default, taint propagation is only tracked for return values and "
            "the `this` argument. This enables taint propagation across "
            "method invocations for all other object type arguments as well."
        ),
    )
    configuration_arguments.add_argument(
        "--heuristics",
        type=_heuristics_json_config_exists,
        help=(
            "Path to JSON configuration file which specifies heuristics "
            "parameters to use during the analysis. See the documentation for "
            "available heuristics parameters."
        ),
    )


def _add_analysis_arguments(parser: argparse.ArgumentParser) -> None:
    analysis_arguments = parser.add_argument_group("Analysis arguments")
    analysis_arguments.add_argument(
        "--sequential",
        action="store_true",
        help="Run the analysis sequentially, one a single thread.",
    )
    analysis_arguments.add_argument(
        "--skip-source-indexing",
        action="store_true",
        help="Skip indexing source files.",
    )
    analysis_arguments.add_argument(
        "--skip-analysis",
        action="store_true",
        help="Skip taint analysis.",
    )
    analysis_arguments.add_argument(
        "--disable-parameter-type-overrides",
        action="store_true",
        help="Disable analyzing methods with specific parameter type information.",
    )
    analysis_arguments.add_argument(
        "--maximum-method-analysis-time",
        type=int,
        help="Specify number of seconds as a bound. If the analysis of a method takes longer than this then make the method obscure (default taint-in-taint-out).",
    )
    analysis_arguments.add_argument(
        "--enable-cross-component-analysis",
        action="store_true",
        help="Compute taint flows across Android components.",
    )
    analysis_arguments.add_argument(
        "--extra-analysis-arguments",
        type=str,
        help=(
            "Additional arguments to be passed to the analysis that the "
            "shim does not currently wrap. For convenience of testing "
            "only."
        ),
    )


def _add_metadata_arguments(parser: argparse.ArgumentParser) -> None:
    metadata_arguments = parser.add_argument_group("Metadata arguments")
    metadata_arguments.add_argument(
        "--job-id",
        type=str,
        help="Specify identifier for the current analysis run.",
    )
    metadata_arguments.add_argument(
        "--metarun-id",
        type=str,
        help="Specify identifier for a group of analysis runs.",
    )


def _add_debug_arguments(parser: argparse.ArgumentParser) -> None:
    debug_arguments = parser.add_argument_group("Debugging arguments")
    debug_arguments.add_argument(
        "--verbosity",
        type=int,
        default=1,
        metavar="[1-5]",
        help="The logging verbosity.",
    )
    debug_arguments.add_argument(
        "--gta-verbosity",
        type=int,
        default=0,
        metavar="[0-10]",
        help="The logging verbosity for global type analysis (GTA). Disabled by default (value=0)",
    )
    debug_arguments.add_argument(
        "--gdb", action="store_true", help="Run the analyzer inside gdb."
    )
    debug_arguments.add_argument(
        "--lldb", action="store_true", help="Run the analyzer inside lldb."
    )
    debug_arguments.add_argument(
        "--log-method",
        action="append",
        metavar="PATTERN",
        help="Enable logging for the given methods.",
    )
    debug_arguments.add_argument(
        "--log-method-types",
        action="append",
        metavar="PATTERN",
        help="Enable logging of type inference (from local and global type analysis) for the given methods.",
    )
    debug_arguments.add_argument(
        "--dump-class-hierarchies",
        action="store_true",
        help="Dump the class hierarchies in `class_hierarchies.json`.",
    )
    debug_arguments.add_argument(
        "--dump-class-intervals",
        action="store_true",
        help="Dump the class intervals in `class_intervals.json`.",
    )
    debug_arguments.add_argument(
        "--dump-overrides",
        action="store_true",
        help="Dump the override graph in `overrides.json`.",
    )
    debug_arguments.add_argument(
        "--dump-call-graph",
        action="store_true",
        help="Dump the call graph in `call_graph.json`.",
    )
    debug_arguments.add_argument(
        "--dump-gta-call-graph",
        action="store_true",
        help="Dump the GTA (global type analysis) call graph in `gta_call_graph.json`.",
    )
    debug_arguments.add_argument(
        "--dump-dependencies",
        action="store_true",
        help="Dump the dependency graph in `dependencies.json`.",
    )
    debug_arguments.add_argument(
        "--dump-methods",
        action="store_true",
        help="Dump a list of the method signatures in `methods.json`.",
    )
    debug_arguments.add_argument(
        "--dump-coverage-info",
        action="store_true",
        help="Dumps file coverage info into `file_coverage.txt` and rule coverage info into `rule_coverage.json`.",
    )
    debug_arguments.add_argument(
        "--dump-replay-output",
        action="store_true",
        help="Dumps the output for replaying an analysis (--analysis-mode=replay) into the output directory.",
    )
    debug_arguments.add_argument(
        "--always-export-origins",
        action="store_true",
        help="Export the origins for every trace frame into the output JSON instead of only on origins.",
    )
    debug_arguments.add_argument(
        "--analysis-mode",
        default="normal",
        choices=["normal", "cached_models", "replay"],
        help=(
            "Analysis modes: cached_models - loads cached models from --sharded-models-directory; "
            "replay - replays a previous analysis with input from --sharded-models-directory; "
            "normal - the default type of analysis, no input models are loaded"
        ),
    )


def _set_environment_variables(arguments: argparse.Namespace) -> None:
    trace_settings = [
        f"MARIANA_TRENCH:{arguments.verbosity}",
        f"TYPE:{arguments.gta_verbosity}",
    ]
    if "TRACE" in os.environ:
        trace_settings.insert(0, os.environ["TRACE"])
    os.environ["TRACE"] = ",".join(trace_settings)


def _get_command_options_json(
    arguments: argparse.Namespace, apk_directory: str, dex_directory: str
) -> Dict[str, Any]:
    options = {}
    options["system-jar-paths"] = arguments.system_jar_configuration_path
    options["apk-directory"] = apk_directory
    options["dex-directory"] = dex_directory
    options["rules-paths"] = arguments.rules_paths
    options["repository-root-directory"] = arguments.repository_root_directory
    options["source-root-directory"] = arguments.source_root_directory
    options["apk-path"] = arguments.apk_path
    options["output-directory"] = arguments.output_directory
    options["maximum-source-sink-distance"] = arguments.maximum_source_sink_distance
    options["model-generator-configuration-paths"] = (
        arguments.model_generator_configuration_paths
    )
    options["analysis-mode"] = arguments.analysis_mode

    if arguments.grepo_metadata_path:
        options["grepo-metadata-path"] = arguments.grepo_metadata_path

    if arguments.model_generator_search_paths:
        options["model-generator-search-paths"] = arguments.model_generator_search_paths

    if arguments.models_paths:
        options["models-paths"] = arguments.models_paths

    if arguments.field_models_paths:
        options["field-models-paths"] = arguments.field_models_paths

    if arguments.literal_models_paths:
        options["literal-models-paths"] = arguments.literal_models_paths

    if arguments.proguard_configuration_paths:
        options["proguard-configuration-paths"] = arguments.proguard_configuration_paths

    if arguments.lifecycles_paths:
        options["lifecycles-paths"] = arguments.lifecycles_paths

    if arguments.shims_paths:
        options["shims-paths"] = arguments.shims_paths

    if arguments.graphql_metadata_paths:
        options["graphql-metadata-paths"] = arguments.graphql_metadata_paths

    if arguments.third_party_library_package_ids_path:
        options["third-party-library-package-ids-path"] = (
            arguments.third_party_library_package_ids_path
        )

    if arguments.source_exclude_directories:
        options["source-exclude-directories"] = arguments.source_exclude_directories

    if arguments.generated_models_directory:
        options["generated-models-directory"] = arguments.generated_models_directory

    if arguments.sharded_models_directory:
        options["sharded-models-directory"] = arguments.sharded_models_directory
        # TODO: Update caching jobs to provide the analysis_mode argument then
        # remove this hack.
        if arguments.analysis_mode == "normal":
            LOG.info(
                "Overriding --analysis-mode to cached_models since --sharded-models-directory is provided"
            )
            options["analysis-mode"] = "cached_models"

    if arguments.emit_all_via_cast_features:
        options["emit-all-via-cast-features"] = True

    if arguments.propagate_across_arguments:
        options["propagate-across-arguments"] = True

    if arguments.allow_via_cast_feature:
        options["allow-via-cast-feature"] = []
        for feature in arguments.allow_via_cast_feature:
            options["allow-via-cast-feature"].append(feature.strip())

    if arguments.heuristics:
        options["heuristics"] = arguments.heuristics

    if arguments.sequential:
        options["sequential"] = True

    if arguments.skip_source_indexing:
        options["skip-source-indexing"] = True

    if arguments.skip_analysis:
        options["skip-analysis"] = True

    if arguments.disable_parameter_type_overrides:
        options["disable-parameter-type-overrides"] = True

    if arguments.disable_global_type_analysis:
        options["disable-global-type-analysis"] = True

    if arguments.verify_expected_output:
        options["verify-expected-output"] = True

    if arguments.remove_unreachable_code:
        options["remove-unreachable-code"] = True

    if arguments.maximum_method_analysis_time is not None:
        options["maximum-method-analysis-time"] = arguments.maximum_method_analysis_time

    if arguments.enable_cross_component_analysis:
        options["enable-cross-component-analysis"] = True

    if arguments.extra_analysis_arguments:
        extra_arguments = json.loads(arguments.extra_analysis_arguments)
        for key, value in extra_arguments.items():
            if (
                key in options
                and isinstance(options[key], list)
                and isinstance(value, list)
            ):
                # Append the values to the existing list
                options[key].extend(value)
            else:
                # Override the value
                options[key] = value

    if arguments.job_id:
        options["job-id"] = arguments.job_id

    if arguments.metarun_id:
        options["metarun-id"] = arguments.metarun_id

    if arguments.log_method:
        options["log-method"] = []
        for method in arguments.log_method:
            options["log-method"].append(method.strip())

    if arguments.log_method_types:
        options["log-method-types"] = []
        for method in arguments.log_method_types:
            options["log-method-types"].append(method.strip())

    if arguments.dump_class_hierarchies:
        options["dump-class-hierarchies"] = True

    if arguments.dump_class_intervals:
        options["dump-class-intervals"] = True

    if arguments.dump_overrides:
        options["dump-overrides"] = True

    if arguments.dump_call_graph:
        options["dump-call-graph"] = True

    if arguments.dump_gta_call_graph:
        options["dump-gta-call-graph"] = True

    if arguments.dump_dependencies:
        options["dump-dependencies"] = True

    if arguments.dump_methods:
        options["dump-methods"] = True

    if arguments.dump_coverage_info:
        options["dump-coverage-info"] = True

    if arguments.dump_replay_output:
        options["dump-class-hierarchies"] = True
        options["dump-class-intervals"] = True
        options["dump-overrides"] = True
        options["always-export-origins"] = True

    if arguments.always_export_origins:
        options["always-export-origins"] = True

    return options


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    build_directory = Path(tempfile.mkdtemp())
    try:
        parser = argparse.ArgumentParser(
            description="A security-focused static analyzer targeting Android."
        )
        parser.add_argument(
            "--version",
            action=VersionAction,
            nargs=0,
            help="Print the version and exit",
        )
        _add_target_arguments(parser)
        _add_output_arguments(parser)
        _add_binary_arguments(parser)
        _add_configuration_arguments(parser)
        _add_analysis_arguments(parser)
        _add_metadata_arguments(parser)
        _add_debug_arguments(parser)
        parser.add_argument(
            "--analyze-third-party",
            action="store_true",
            help="Analyzing third party apps",
        )

        arguments: argparse.Namespace = parser.parse_args()

        # TODO T147423951
        if arguments.system_jar_configuration_path is None:
            arguments.system_jar_configuration_path = _system_jar_configuration_path(
                # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
                os.fspath(configuration.get_path("default_system_jar_paths.json"))
            )
        if arguments.rules_paths is None:
            # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
            arguments.rules_paths = str(os.fspath(configuration.get_path("rules.json")))
        if arguments.model_generator_configuration_paths is None:
            arguments.model_generator_configuration_paths = _separated_paths_exist(
                # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
                os.fspath(configuration.get_path("default_generator_config.json"))
            )
        if arguments.model_generator_search_paths is None:
            arguments.model_generator_search_paths = _separated_paths_exist(
                ";".join(
                    os.fspath(path)
                    # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
                    for path in configuration.get_default_generator_search_paths()
                )
            )
        if arguments.lifecycles_paths is None:
            arguments.lifecycles_paths = _separated_paths_exist(
                # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
                os.fspath(configuration.get_path("lifecycles.json"))
            )
        if arguments.shims_paths is None:
            arguments.shims_paths = _separated_paths_exist(
                # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
                os.fspath(configuration.get_path("shims.json"))
            )
        if (
            # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
            configuration.FACEBOOK_SHIM
            and arguments.java_target is not None
            and arguments.apk_path is not None
        ):
            parser.error(
                "The analysis target can either be a java target (--java-target)"
                + " or an apk file (--apk-path), but not both."
            )
        if (
            # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
            configuration.FACEBOOK_SHIM
            and arguments.java_target is None
            and arguments.apk_path is None
        ):
            parser.error(
                "The analysis target should either be a java target (--java-target)"
                + " or an apk file (--apk-path)."
            )
        # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
        if not configuration.FACEBOOK_SHIM and arguments.apk_path is None:
            parser.error("The argument --apk-path is required.")

        # Build the vanilla java project.
        # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
        if configuration.FACEBOOK_SHIM and arguments.java_target:
            if os.path.isfile(arguments.java_target):
                jar_file = Path(arguments.java_target)
            else:
                jar_file = _extract_jex_file_if_exists(
                    _build_target(arguments.java_target, mode=arguments.java_mode),
                    arguments.java_target,
                    build_directory,
                )
            desugared_jar_file = _desugar_jar_file(jar_file)
            arguments.apk_path = os.fspath(_build_apk_from_jar(desugared_jar_file))

        # Build the mariana trench binary if necessary.
        binary = _get_analysis_binary(arguments)

        LOG.info(f"Extracting `{arguments.apk_path}`...")
        apk_directory = tempfile.mkdtemp(suffix="_apk")
        dex_directory = tempfile.mkdtemp(suffix="_dex")
        pyredex.utils.unzip_apk(arguments.apk_path, apk_directory)
        dex_mode = pyredex.unpacker.detect_secondary_dex_mode(apk_directory)
        dex_mode.unpackage(apk_directory, dex_directory)
        LOG.info(f"Extracted APK into `{apk_directory}` and DEX into `{dex_directory}`")

        # pyre-fixme[16]: Module `shim` has no attribute `configuration`.
        if configuration.FACEBOOK_SHIM and arguments.analyze_third_party:
            output = start_third_party_analysis(
                binary, arguments, apk_directory, dex_directory
            )
        else:
            with tempfile.NamedTemporaryFile(suffix=".json", mode="w") as options_file:
                _set_environment_variables(arguments)
                options_json = _get_command_options_json(
                    arguments, apk_directory, dex_directory
                )
                json.dump(options_json, options_file, indent=4)
                options_file.flush()
                command = [os.fspath(binary.resolve()), "--config", options_file.name]
                if arguments.gdb:
                    command = ["gdb", "--args"] + command
                elif arguments.lldb:
                    command = ["lldb", "--"] + command
                LOG.info(f"Running Mariana Trench: {' '.join(command)}")
                output = subprocess.run(command)
        if output.returncode != 0:
            LOG.fatal(f"Analysis binary exited with exit code {output.returncode}.")
            sys.exit(output.returncode)
    except (ClientError, ConfigurationError) as error:
        LOG.fatal(f"{type(error).__name__}: {error.args[0]}")
        LOG.fatal(f"Exiting with error code: {error.exit_code}")
        sys.exit(error.exit_code)
    except BadZipFile as error:
        LOG.fatal(
            f"APKError: Failed to extract APK due to {type(error).__name__}: {error.args[0]}"
        )
        LOG.fatal(f"Exiting with error code: {ExitCode.APK_ERROR}")
        sys.exit(ExitCode.APK_ERROR)
    except Exception:
        LOG.fatal(f"Unexpected error:\n{traceback.format_exc()}")
        LOG.fatal(f"Exiting with error code: {ExitCode.ERROR}")
        sys.exit(ExitCode.ERROR)
    finally:
        try:
            shutil.rmtree(build_directory)
        except IOError:
            pass  # Swallow.


if __name__ == "__main__":
    main()
