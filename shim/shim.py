#!/usr/bin/env python3
# (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# TODO (T47489191): remove FB specific paths from this script

import argparse
import enum
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import traceback
import typing
import zipfile

import pyredex
import redex


LOG: logging.Logger = logging.getLogger(__name__)


def _check_executable(path: pathlib.Path) -> pathlib.Path:
    if not (path.exists() and os.access(path, os.X_OK)):
        raise EnvironmentError(f"Invalid binary `{path}`")
    return path


class BuildMode(enum.Enum):
    ASAN = "@fbandroid/mode/asan"
    ASAN_LSAN = "@fbandroid/mode/asan_lsan"
    ASAN_UBSAN = "@fbandroid/mode/asan_ubsan"
    TSAN = "@fbandroid/mode/tsan"
    UBSAN = "@fbandroid/mode/ubsan"
    DEV = "@fbandroid/mode/dev"
    OPT = "@fbandroid/mode/opt"

    def __str__(self) -> str:
        return self.value


class ExtractJexException(Exception):
    pass


def _extract_jex_file_if_exists(
    path: pathlib.Path, target: str, resource_directory: pathlib.Path
) -> pathlib.Path:
    jex_extract_directory = pathlib.Path(resource_directory) / "jex"

    def run_unzip_command(command: typing.List[str]) -> pathlib.Path:
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
                    f"Unable to extract binary file `{path}` with command `{command_string}`: {stderr}."
                )

        jar_file_path = jex_extract_directory / (
            target.rsplit(":", maxsplit=1)[1] + ".jar"
        )
        if jar_file_path.exists():
            return jar_file_path
        else:
            raise EnvironmentError(
                f"Cannot find jar file {path} in {jex_extract_directory}"
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
        LOG.warning(f"Now try to extract file `{path}` with `unzip`.")
        return run_unzip_command(["unzip", "-d", str(jex_extract_directory), str(path)])


def _build_target(
    target: str, *, mode: typing.Optional[BuildMode] = None
) -> pathlib.Path:
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

    command = ["buck", "build"]
    if mode:
        command.append(str(mode))
    command.append(target)
    current_working_directory = pathlib.Path(os.getcwd())
    working_directory = (
        current_working_directory / "fbcode"
        if is_fbcode_target
        else current_working_directory
    )
    output = subprocess.run(command, stderr=subprocess.PIPE, cwd=working_directory)
    if output.returncode:
        raise EnvironmentError(f"Unable to build binary:\n{output.stderr.decode()}")

    LOG.info("Getting binary...")
    output = (
        subprocess.check_output(
            ["buck", "targets", "--show-output", target],
            stderr=subprocess.DEVNULL,
            cwd=working_directory,
        )
        .decode("utf-8")
        .strip()
    )
    return working_directory / output.split(" ")[1]


def _build_executable_target(
    target: str, *, mode: typing.Optional[BuildMode] = None
) -> pathlib.Path:
    return _check_executable(_build_target(target, mode=mode))


def _get_analysis_binary(arguments: argparse.Namespace) -> pathlib.Path:
    from_arguments = arguments.binary
    if from_arguments:
        # Use the user-provided binary.
        return _check_executable(pathlib.Path(from_arguments))

    # Build the mariana-trench binary from source.
    return _build_executable_target(
        "fbsource//fbandroid/native/mariana-trench/source:mariana-trench",
        mode=arguments.build,
    )


def _desugar_jar_file(jar_path: pathlib.Path) -> pathlib.Path:
    LOG.info(f"Desugaring `{jar_path}`...")
    target = "fbsource//fbandroid/native/mariana-trench/desugar/com/facebook/marianatrench:desugar"
    subprocess.check_call(["buck", "build", target])
    binary = (
        subprocess.check_output(
            ["buck", "targets", "--show-output", target], stderr=subprocess.DEVNULL
        )
        .decode("utf-8")
        .strip()
    )
    desugared_jar_file = jar_path.parent / (jar_path.stem + "-desugared.jar")
    subprocess.check_call(
        [
            "java",
            "-Dlog4j.configurationFile=fbandroid/native/mariana-trench/desugar/resources/log4j2.properties",
            "-jar",
            binary.split(" ")[1],
            os.fspath(jar_path),
            os.fspath(desugared_jar_file),
        ]
    )
    LOG.info(f"Desugared jar file: `{desugared_jar_file}`...")
    return desugared_jar_file


def _build_apk_from_jar(jar_path: pathlib.Path) -> pathlib.Path:
    _, dex_file = tempfile.mkstemp(suffix=".jar")

    LOG.info(f"Running d8 on `{jar_path}`...")
    output = subprocess.run(
        [
            "/opt/android/sdk_D23134735/build-tools/29.0.2/d8",
            jar_path,
            "--output",
            dex_file,
            "--lib",
            "/opt/android/sdk_D23134735/platforms/android-29/android.jar",
            "--min-api",
            "25",  # mininum api 25 corresponds to dex 37
        ],
        stderr=subprocess.PIPE,
    )
    if output.returncode:
        raise EnvironmentError(f"Fail to run d8:\n{output.stderr.decode()}")

    return pathlib.Path(dex_file)


def _get_resource_path(path: str, resource_directory: pathlib.Path) -> str:
    return os.fspath(
        resource_directory
        / (pathlib.Path("fbandroid/native/mariana-trench/shim/resources") / path)
    )


def _extract_all_resources(resource_directory: pathlib.Path) -> None:
    pex_path = __file__.split(".pex")[0] + ".pex"
    with zipfile.ZipFile(pex_path, "r") as file:
        file.extractall(path=resource_directory)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")

    def _path_exists(path: str) -> str:
        path = os.path.expanduser(path)
        if not os.path.exists(path):
            raise ValueError(f"Path `{path}` does not exist.")
        return os.path.realpath(path)

    def _directory_exists(path: str) -> str:
        path = os.path.expanduser(path)
        if not os.path.isdir(path):
            raise ValueError(f"Path `{path}` is not a directory.")
        path = os.path.realpath(path)
        if path and path[-1] != "/":
            path = path + "/"
        return path

    resource_directory = pathlib.Path(tempfile.mkdtemp())
    try:
        _extract_all_resources(resource_directory)

        parser = argparse.ArgumentParser()

        target_arguments = parser.add_argument_group("Target arguments")
        target_arguments.add_argument(
            "--apk-path", type=_path_exists, help="The APK to analyze."
        )
        target_arguments.add_argument(
            "--java-target",
            type=str,
            help="The java buck target to analyze. If the target is `java_library`, append `-javaX` were X is the java version (e.g. 11).",
        )
        target_arguments.add_argument(
            "--java-mode", type=str, help="The buck mode for building the java target."
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
        binary_arguments.add_argument(
            "--build",
            type=BuildMode,
            choices=BuildMode,
            default=BuildMode.OPT,
            metavar="BUILD_MODE",
            help="The Mariana Trench binary build mode.",
        )

        configuration_arguments = parser.add_argument_group("Configuration arguments")
        configuration_arguments.add_argument(
            "--system-jar-configuration-path",
            type=_path_exists,
            default=_get_resource_path(
                "default_system_jar_paths.json", resource_directory
            ),
            help="A JSON configuration file with a list of paths to the system jars.",
        )
        configuration_arguments.add_argument(
            "--models-paths",
            type=str,
            default=_get_resource_path("default_models.json", resource_directory),
            help="A `;`-separated list of models files and directories containing models files.",
        )
        configuration_arguments.add_argument(
            "--rules-paths",
            type=str,
            default=_get_resource_path("rules.json", resource_directory),
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
            "--proguard-configuration-paths",
            type=_path_exists,
            default=None,
            help="If ProGuard configurations are provided, Mariana Trench will save time by ignoring unreachable objects.",
        )
        configuration_arguments.add_argument(
            "--generator-configuration-path",
            type=_path_exists,
            default=_get_resource_path(
                "default_generator_config.json", resource_directory
            ),
            help="""A JSON configuration file specifying a list of absolute paths
            to JSON model generators or names of CPP model generators.""",
        )
        configuration_arguments.add_argument(
            "--maximum-source-sink-distance",
            type=int,
            default=7,
            help="Limits the distance of sources and sinks from a trace entry point.",
        )

        analysis_arguments = parser.add_argument_group("Analysis arguments")
        analysis_arguments.add_argument(
            "--sequential",
            action="store_true",
            help="Run the analysis sequentially, one a single thread.",
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
            "--dump-dependencies",
            action="store_true",
            help="Dump the dependency graph in `dependencies.gv`.",
        )

        arguments: argparse.Namespace = parser.parse_args()

        if arguments.java_target and arguments.apk_path:
            parser.error(
                "The analysis target can either be a java target (--java-target) or an apk file (--apk-path), but not both."
            )
        if arguments.java_target is None and arguments.apk_path is None:
            parser.error(
                "The analysis target should either be a java target (--java-target) or an apk file (--apk-path)."
            )

        # Build the vanilla java project.
        if arguments.java_target:
            jar_file = _extract_jex_file_if_exists(
                _build_target(arguments.java_target, mode=arguments.java_mode),
                arguments.java_target,
                resource_directory,
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
            "--system-jar-configuration-path",
            arguments.system_jar_configuration_path,
            "--apk-directory",
            apk_directory,
            "--dex-directory",
            dex_directory,
            "--models-paths",
            arguments.models_paths,
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
            "--generator-configuration-path",
            arguments.generator_configuration_path,
        ]

        if arguments.proguard_configuration_paths:
            options.append("--proguard-configuration-paths")
            options.append(arguments.proguard_configuration_paths)

        if arguments.generated_models_directory:
            options.append("--generated-models-directory")
            options.append(arguments.generated_models_directory)

        if arguments.sequential:
            options.append("--sequential")
        if arguments.disable_parameter_type_overrides:
            options.append("--disable-parameter-type-overrides")

        trace_settings = [f"MARIANA_TRENCH:{arguments.verbosity}"]
        if "TRACE" in os.environ:
            trace_settings.insert(0, os.environ["TRACE"])
        os.environ["TRACE"] = ",".join(trace_settings)

        if arguments.log_method:
            for method in arguments.log_method:
                options.append("--log-method=%s" % method.strip())
        if arguments.dump_dependencies:
            options.append("--dump-dependencies")

        command = [os.fspath(binary.resolve())] + options
        if arguments.gdb:
            command = ["gdb", "--args"] + command
        elif arguments.lldb:
            command = ["lldb", "--"] + command
        LOG.info(f"Running Mariana Trench: {' '.join(command)}")
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as error:
        # pyre-fixme[16]: `Logger` has no attribute `fatal`.
        LOG.fatal(f"Command `{' '.join(error.cmd)}` exited with non-zero exit code.")
        LOG.fatal(f"Exiting...")
        sys.exit(1)
    except ExtractJexException as error:
        LOG.fatal(error.args[0])
        LOG.fatal(f"Exiting...")
        sys.exit(1)
    except Exception:
        LOG.fatal(f"Unexpected error: {traceback.format_exc()}")
        LOG.fatal(f"Exiting...")
        sys.exit(1)
    finally:
        try:
            shutil.rmtree(resource_directory)
        except IOError:
            pass  # Swallow.
