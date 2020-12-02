#!/usr/bin/env python3
# (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# pyre-strict

import argparse
import logging
import os
import os.path
import shlex
import shutil
import subprocess
import sys
import tempfile
import typing


FBSOURCE: str = os.path.expanduser("~/fbsource")
LOG: logging.Logger = logging.getLogger("root")
DEFAULT_BUCK_TARGET: str = "automation_fbandroid_stablesize_release"


def absolute_path(path: str) -> str:
    return os.path.join(FBSOURCE, path)


def redex_command(buck_target: str) -> str:
    # This is based on redex/facebook/tools/redex-build.py
    LOG.info(f"Building target `{buck_target}`")
    with tempfile.NamedTemporaryFile() as f:
        LOG.debug(f"Using trace file `{f.name}`")
        environment = os.environ.copy()
        environment["TRACE"] = "REDEX:1"
        environment["TRACEFILE"] = f.name
        subprocess.check_call(
            [
                "buck",
                "build",
                "@fbsource//fbandroid/mode/opt",
                "@fbsource//fbandroid/mode/server",
                "--show-output",
                buck_target,
            ],
            env=environment,
        )

        f.seek(0)
        first_line = f.readline().decode()
        if first_line == "":

            LOG.error("Could not extract the redex command line.")
            LOG.warning("If the build is cached, run `buck clean` first.")
            if buck_target != DEFAULT_BUCK_TARGET:
                LOG.warning("Make sure to use a release build target.")
            sys.exit(1)
        assert first_line == "To rerun this command outside of Buck:\n"
        return f.readline().decode().strip()


def parse_redex_command(command: str) -> argparse.Namespace:
    LOG.info("Parsing redex command")
    parser = argparse.ArgumentParser()

    for option in ("--sign", "--is-art-build"):
        parser.add_argument(option, action="store_true")

    for option in (
        "--config",
        "--keystore",
        "--keyalias",
        "--keypass",
        "--proguard-map",
        "--keep",
        "--out",
        "--redex-binary",
        "-A",
        "--used-js-assets",
    ):
        parser.add_argument(option)

    for option in ("-S", "-J"):
        parser.add_argument(option, action="append", default=[])

    parser.add_argument("-P", "--proguard-config", action="append", default=[])
    parser.add_argument("-j", "--jarpath", action="append", default=[])
    parser.add_argument("apk")

    argv = shlex.split(command)
    return parser.parse_args(argv[1:])


def parse_proguard_configuration(path: str) -> typing.Set[str]:
    LOG.info(f"Parsing proguard configuration file `{path}`")

    with open(path) as file:
        content = file.read()

    # argparse is too slow to handle this (20 Mb), use custom parsing instead.
    previous_token: typing.Optional[str] = None
    jar_paths: typing.Set[str] = set()

    for token in content.split():
        token = token.strip()

        if not token:
            continue
        elif previous_token == "-injars":
            jar_paths.add(absolute_path(token))
        elif previous_token == "-libraryjars":
            jar_paths.update(map(absolute_path, token.split(":")))

        previous_token = token

    return jar_paths


def jar_canonical_name(path: str) -> str:
    head, name = os.path.split(path)

    while True:
        head, tail = os.path.split(head)

        if not head or not tail or tail in ("fbandroid", "buck-out", "xplat"):
            return name
        else:
            new_name = f"{tail}.{name}"
            if len(new_name) > 255:
                return name
            else:
                name = new_name


def copy_jars(jar_paths: typing.Iterable[str], output_directory: str) -> None:
    LOG.info(f"Copying jar files into `{output_directory}`")

    for jar_path in jar_paths:
        try:
            jar_name = jar_canonical_name(jar_path)
            shutil.copy(jar_path, os.path.join(output_directory, jar_name))
        except FileNotFoundError:
            pass


def path_exists(path: str) -> str:
    path = os.path.expanduser(path)
    if not os.path.exists(path):
        raise ValueError(f"Path `{path}` does not exist.")
    return os.path.realpath(path)


def directory_exists(path: str) -> str:
    path = os.path.expanduser(path)
    if not os.path.isdir(path):
        raise ValueError(f"Path `{path}` is not a directory.")
    return os.path.realpath(path)


def main() -> None:
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(
        description="Build all the jars for a given buck target and copy them into a directory."
    )
    parser.add_argument("--target", default=DEFAULT_BUCK_TARGET)
    parser.add_argument("output_directory", type=directory_exists)
    arguments = parser.parse_args()

    command = redex_command(arguments.target)
    redex_arguments = parse_redex_command(command)

    jar_paths = set(map(absolute_path, redex_arguments.jarpath))

    for proguard_path in redex_arguments.proguard_config:
        proguard_path = absolute_path(proguard_path)
        jar_paths.update(parse_proguard_configuration(proguard_path))

    copy_jars(jar_paths, arguments.output_directory)


if __name__ == "__main__":
    main()
