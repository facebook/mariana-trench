#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path
from typing import Optional, Any, List

from pyre_extensions import none_throws, safe_json

try:
    from ..facebook.shim import configuration
except Exception:
    # pyre-ignore
    from . import configuration

import pyredex


LOG: logging.Logger = logging.getLogger(__name__)


class ClientError(Exception):
    pass


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
        raise ClientError(f"Invalid binary `{path}`.")
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
                    f"`{path} must contain a list of strings"
                )

    # Validation deferred to backend if we pass `;` separated list of paths
    # because they are allowed to not exist.
    return input


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
            raise ClientError(
                f"Could not find jar file `{path}` in `{jex_extract_directory}`."
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

    command = ["buck", "build", "--show-json-output"]
    if mode:
        command.append(str(mode))
    command.append(target)
    current_working_directory = Path(os.getcwd())
    working_directory = (
        current_working_directory / "fbcode"
        if is_fbcode_target
        else current_working_directory
    )
    output = subprocess.run(command, stdout=subprocess.PIPE, cwd=working_directory)
    if output.returncode != 0:
        raise ClientError(f"Error while building buck target `{target}`, aborting.")

    try:
        response = json.loads(output.stdout)
    except json.JSONDecodeError:
        response = {}

    if len(response) != 1 or len(next(iter(response.values()))) == 0:
        raise ClientError(f"Unexpected buck output:\n{output.stdout.decode()}")

    return working_directory / next(iter(response.values()))


def _build_executable_target(target: str, *, mode: Optional[str] = None) -> Path:
    return _check_executable(_build_target(target, mode=mode))


def _get_analysis_binary(arguments: argparse.Namespace) -> Path:
    from_arguments = arguments.binary
    if from_arguments:
        # Use the user-provided binary.
        return _check_executable(Path(from_arguments))

    buck_target = configuration.BINARY_BUCK_TARGET
    if buck_target:
        # Build the mariana-trench binary from buck (facebook-only).
        return _build_executable_target(
            buck_target,
            mode=arguments.build,
        )

    path_command = configuration.BINARY_PATH_COMMAND
    if path_command:
        # Find the mariana-trench binary in the path (open-source).
        command = shutil.which(path_command)
        if command is None:
            raise ClientError(f"Could not find `{path_command}` in PATH.")
        return Path(command)

    raise ClientError("Could not find the analyzer binary.")


def _desugar_jar_file(jar_path: Path) -> Path:
    LOG.info(f"Desugaring `{jar_path}`...")
    desugar_tool = _build_target(none_throws(configuration.DESUGAR_BUCK_TARGET))
    desugared_jar_file = jar_path.parent / (jar_path.stem + "-desugared.jar")
    output = subprocess.run(
        [
            "java",
            f"-Dlog4j.configurationFile={configuration.DESUGAR_LOG_CONFIGURATION_PATH}",
            "-jar",
            desugar_tool,
            os.fspath(jar_path),
            os.fspath(desugared_jar_file),
        ]
    )
    if output.returncode != 0:
        raise ClientError("Error while desugaring jar file, aborting.")

    LOG.info(f"Desugared jar file: `{desugared_jar_file}`.")
    return desugared_jar_file


def _build_apk_from_jar(jar_path: Path) -> Path:
    _, dex_file = tempfile.mkstemp(suffix=".jar")

    LOG.info(f"Running d8 on `{jar_path}`...")
    output = subprocess.run(
        [
            "/opt/android/sdk_D23134735/build-tools/29.0.2/d8",
            "-JXmx8G",
            jar_path,
            "--output",
            dex_file,
            "--lib",
            "/opt/android/sdk_D23134735/platforms/android-29/android.jar",
            "--min-api",
            "25",  # mininum api 25 corresponds to dex 37
        ]
    )
    if output.returncode != 0:
        raise ClientError("Error while running d8, aborting.")

    return Path(dex_file)


class VersionAction(argparse.Action):
    def __call__(self, parser: argparse.ArgumentParser, *_: Any) -> None:
        from . import package

        print(f"{package.name} {package.version}")
        print("Copyright (c) Facebook, Inc. and its affiliates.")
        parser.exit()


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

        target_arguments = parser.add_argument_group("Target arguments")
        target_arguments.add_argument(
            "--apk-path",
            type=_path_exists,
            help="The APK to analyze.",
        )
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

        binary_arguments = parser.add_argument_group("Analysis binary arguments")
        binary_arguments.add_argument(
            "--binary", type=str, help="The Mariana Trench binary."
        )
        if configuration.FACEBOOK_SHIM:
            binary_arguments.add_argument(
                "--build",
                type=str,
                default=none_throws(configuration.BINARY_BUCK_BUILD_MODE),
                metavar="BUILD_MODE",
                help="The Mariana Trench binary buck build mode.",
            )

        configuration_arguments = parser.add_argument_group("Configuration arguments")
        configuration_arguments.add_argument(
            "--system-jar-configuration-path",
            type=_system_jar_configuration_path,
            default=os.fspath(configuration.get_path("default_system_jar_paths.json")),
            help="A JSON configuration file with a list of paths to the system jars "
            + "or a `;` separated list of jars.",
        )
        configuration_arguments.add_argument(
            "--rules-paths",
            type=str,
            default=os.fspath(configuration.get_path("rules.json")),
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
            "--proguard-configuration-paths",
            type=_path_exists,
            default=None,
            help="If ProGuard configurations are provided, Mariana Trench will save time by ignoring unreachable objects.",
        )
        configuration_arguments.add_argument(
            "--lifecycles-paths",
            type=_separated_paths_exist,
            default=None,
            help="A `;`-separated list of files and directories containing lifecycle definitions.",
        )
        configuration_arguments.add_argument(
            "--model-generator-configuration-paths",
            type=_separated_paths_exist,
            default=os.fspath(configuration.get_path("default_generator_config.json")),
            help="""A `;`-separated list of paths specifying JSON configuration files. Each file is a list of paths
            to JSON model generators relative to the configuration file or names of CPP model generators.""",
        )
        configuration_arguments.add_argument(
            "--model-generator-search-paths",
            type=_separated_paths_exist,
            default=";".join(
                os.fspath(path)
                for path in configuration.get_default_generator_search_paths()
            ),
            help="A `;`-separated list of paths where we look up JSON model generators.",
        )
        configuration_arguments.add_argument(
            "--maximum-source-sink-distance",
            type=int,
            default=configuration.DEFAULT_MAXIMUM_SOURCE_SINK_DISTANCE,
            help="Limits the distance of sources and sinks from a trace entry point.",
        )

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
            "--skip-model-generation",
            action="store_true",
            help="Skip model generation.",
        )
        analysis_arguments.add_argument(
            "--disable-parameter-type-overrides",
            action="store_true",
            help="Disable analyzing methods with specific parameter type information.",
        )

        debug_arguments = parser.add_argument_group("Debugging arguments")
        debug_arguments.add_argument(
            "--verbosity",
            type=int,
            default=1,
            metavar="[1-5]",
            help="The logging verbosity.",
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
            "--dump-class-hierarchies",
            action="store_true",
            help="Dump the class hierarchies in `class_hierarchies.json`.",
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
            "--dump-dependencies",
            action="store_true",
            help="Dump the dependency graph in `dependencies.json`.",
        )

        arguments: argparse.Namespace = parser.parse_args()

        if (
            configuration.FACEBOOK_SHIM
            and arguments.java_target is not None
            and arguments.apk_path is not None
        ):
            parser.error(
                "The analysis target can either be a java target (--java-target)"
                + " or an apk file (--apk-path), but not both."
            )
        if (
            configuration.FACEBOOK_SHIM
            and arguments.java_target is None
            and arguments.apk_path is None
        ):
            parser.error(
                "The analysis target should either be a java target (--java-target)"
                + " or an apk file (--apk-path)."
            )
        if not configuration.FACEBOOK_SHIM and arguments.apk_path is None:
            parser.error("The argument --apk-path is required.")

        # Build the vanilla java project.
        if configuration.FACEBOOK_SHIM and arguments.java_target:
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

        options = [
            "--system-jar-paths",
            arguments.system_jar_configuration_path,
            "--apk-directory",
            apk_directory,
            "--dex-directory",
            dex_directory,
            "--rules-paths",
            arguments.rules_paths,
            "--repository-root-directory",
            arguments.repository_root_directory,
            "--source-root-directory",
            arguments.source_root_directory,
            "--apk-path",
            arguments.apk_path,
            "--output-directory",
            arguments.output_directory,
            "--maximum-source-sink-distance",
            str(arguments.maximum_source_sink_distance),
            "--model-generator-configuration-paths",
            arguments.model_generator_configuration_paths,
        ]

        if arguments.model_generator_search_paths is not None:
            options.append("--model-generator-search-paths")
            options.append(arguments.model_generator_search_paths)

        if arguments.proguard_configuration_paths:
            options.append("--proguard-configuration-paths")
            options.append(arguments.proguard_configuration_paths)

        if arguments.lifecycles_paths:
            options.append("--lifecycles-paths")
            options.append(arguments.lifecycles_paths)

        if arguments.source_exclude_directories:
            options.append("--source-exclude-directories")
            options.append(arguments.source_exclude_directories)

        if arguments.generated_models_directory:
            options.append("--generated-models-directory")
            options.append(arguments.generated_models_directory)

        if arguments.sequential:
            options.append("--sequential")
        if arguments.skip_source_indexing:
            options.append("--skip-source-indexing")
        if arguments.skip_model_generation:
            options.append("--skip-model-generation")
        if arguments.disable_parameter_type_overrides:
            options.append("--disable-parameter-type-overrides")

        trace_settings = [f"MARIANA_TRENCH:{arguments.verbosity}"]
        if "TRACE" in os.environ:
            trace_settings.insert(0, os.environ["TRACE"])
        os.environ["TRACE"] = ",".join(trace_settings)

        if arguments.log_method:
            for method in arguments.log_method:
                options.append("--log-method=%s" % method.strip())
        if arguments.dump_class_hierarchies:
            options.append("--dump-class-hierarchies")
        if arguments.dump_overrides:
            options.append("--dump-overrides")
        if arguments.dump_call_graph:
            options.append("--dump-call-graph")
        if arguments.dump_dependencies:
            options.append("--dump-dependencies")

        command = [os.fspath(binary.resolve())] + options
        if arguments.gdb:
            command = ["gdb", "--args"] + command
        elif arguments.lldb:
            command = ["lldb", "--"] + command
        LOG.info(f"Running Mariana Trench: {' '.join(command)}")
        output = subprocess.run(command)
        if output.returncode != 0:
            LOG.fatal(f"Analysis binary exited with exit code {output.returncode}.")
            sys.exit(output.returncode)
    except (ClientError, configuration.Error) as error:
        LOG.fatal(error.args[0])
        sys.exit(1)
    except Exception:
        LOG.fatal(f"Unexpected error:\n{traceback.format_exc()}")
        sys.exit(1)
    finally:
        try:
            shutil.rmtree(build_directory)
        except IOError:
            pass  # Swallow.


if __name__ == "__main__":
    main()
