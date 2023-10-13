#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import collections
import logging
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Optional


LOG: logging.Logger = logging.getLogger(__name__)


def _path_exists(path: str) -> Path:
    path = os.path.expanduser(path)
    if not os.path.exists(path):
        raise argparse.ArgumentTypeError(f"Path `{path}` does not exist.")
    return Path(path)


def _directory_exists(path: str) -> Path:
    path = os.path.expanduser(path)
    if not os.path.isdir(path):
        raise argparse.ArgumentTypeError(f"Path `{path}` is not a directory.")
    return Path(path)


def _check_executable(path: Path) -> Path:
    if not (path.exists() and os.access(path, os.X_OK)):
        raise argparse.ArgumentTypeError(f"Path `{path}` is not executable.")
    return path


def _executable_exists(path: str) -> Path:
    return _check_executable(_path_exists(path))


def _check_python_directory(path: Path) -> Path:
    init_path = path / "__init__.py"
    if not init_path.exists():
        raise argparse.ArgumentTypeError(f"Path `{init_path}` does not exist.")
    return path


def _python_directory_exists(path: str) -> Path:
    return _check_python_directory(_directory_exists(path))


def _run_python(arguments: List[str], working_directory: Optional[Path] = None) -> None:
    LOG.info(f"Running python {' '.join(arguments)}")
    subprocess.run([sys.executable] + arguments, check=True, cwd=working_directory)


def _install_build_dependencies() -> None:
    _run_python(["-m", "pip", "install", "--upgrade", "pip", "build"])


def _prepare_build_directory(
    build_root: Path,
    package_name: str,
    package_version: str,
    repository: Path,
    binary: Path,
    pyredex: Path,
) -> None:
    LOG.info(f"Preparing build at `{build_root}`")
    _add_initial_files(build_root)
    _sync_readme(build_root, repository)
    _sync_python_files(build_root, repository, pyredex)
    _sync_configuration_files(build_root, repository)
    _sync_binary(build_root, binary)
    _add_package_py(build_root, package_name, package_version)
    _add_pyproject(build_root)
    _add_setup_cfg(build_root, package_name, package_version)


def _build_package(build_root: Path) -> None:
    _run_python(["-m", "build"], working_directory=build_root)


def _distribution_platform() -> str:
    if sys.platform == "linux":
        return "manylinux1_x86_64"
    elif sys.platform == "darwin":
        return "macosx_10_11_x86_64"
    else:
        raise AssertionError("This operating system is not currently supported.")


def _copy_package(build_root: Path, output_path: Path) -> None:
    # Find the source distribution and wheel
    dist_directory = build_root / "dist"
    wheel = list(dist_directory.glob("**/*.whl"))
    source_distribution = list(dist_directory.glob("**/*.tar.gz"))
    if not len(wheel) == 1 and not len(source_distribution) == 1:
        raise AssertionError(f"Unexpected files found in `{build_root}/dist`.")
    source_distribution, wheel = source_distribution[0], wheel[0]

    # Rename and move under the output path
    output_path /= "dist"
    output_path.mkdir(exist_ok=True)
    shutil.move(
        # pyre-fixme[6]: Expected `str` for 1st param but got `Path`.
        wheel,
        output_path
        / wheel.name.replace("-any.whl", f"-{_distribution_platform()}.whl"),
    )
    # pyre-fixme[6]: Expected `str` for 1st param but got `Path`.
    shutil.move(source_distribution, output_path / source_distribution.name)


def _install_package(build_root: Path) -> None:
    _run_python(["-m", "pip", "install", "."], working_directory=build_root)


def _mkdir_and_init(module_path: Path) -> None:
    module_path.mkdir()
    init_path = module_path / "__init__.py"
    init_path.write_text(
        """\
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.
"""
    )


def _add_initial_files(build_root: Path) -> None:
    _mkdir_and_init(build_root / "mariana_trench")
    _mkdir_and_init(build_root / "mariana_trench" / "shim")
    (build_root / "share").mkdir()
    (build_root / "share" / "mariana-trench").mkdir()


def _copy(source_path: Path, target_path: Path) -> None:
    LOG.info(f"Copying `{source_path}` into `{target_path}`")
    shutil.copy(source_path, target_path)


def _sync_readme(build_root: Path, repository: Path) -> None:
    _copy(repository / "README.md", build_root / "README.md")


def _rsync_files(
    filters: List[str],
    source_directory: Path,
    target_directory: Path,
    arguments: List[str],
) -> None:
    LOG.info(f"Copying `{source_directory}` into `{target_directory}`")
    command = ["rsync", "--quiet"]
    command.extend(arguments)
    command.extend(["--filter=" + filter_string for filter_string in filters])
    command.append(str(source_directory))
    command.append(str(target_directory))
    subprocess.run(command, check=True)


def _sync_python_files(build_root: Path, repository: Path, pyredex: Path) -> None:
    filters = ["- tests/", "+ */", "-! *.py"]
    _rsync_files(filters, repository / "shim", build_root / "mariana_trench", ["-avm"])
    _rsync_files(filters, pyredex, build_root, ["-avm"])


def _sync_configuration_files(build_root: Path, repository: Path) -> None:
    _rsync_files(
        ["+ */", "-! *.json"],
        repository / "configuration",
        build_root / "share/mariana-trench",
        ["-avm"],
    )
    _rsync_files(
        ["+ */", "-! *.models"],
        repository / "configuration",
        build_root / "share/mariana-trench",
        ["-avm"],
    )


def _sync_binary(build_root: Path, binary: Path) -> None:
    (build_root / "bin").mkdir()
    _copy(binary, build_root / "bin" / "mariana-trench-binary")


def _add_package_py(build_root: Path, package_name: str, package_version: str) -> None:
    path = build_root / "mariana_trench/shim/package.py"
    path.write_text(
        f"""\
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

name = '{package_name}'
version = '{package_version}'
"""
    )


def _add_pyproject(build_root: Path) -> None:
    LOG.info(f"Generating `{build_root}/pyproject.toml`")
    pyproject = build_root / "pyproject.toml"
    pyproject.write_text(
        """\
[build-system]
requires = [
    "setuptools>=42",
    "wheel"
]
build-backend = "setuptools.build_meta"
"""
    )


def _add_setup_cfg(
    build_root: Path,
    package_name: str,
    package_version: str,
) -> None:
    LOG.info(f"Generating `{build_root}/setup.cfg`")

    configuration_directories = collections.defaultdict(list)
    for path in build_root.glob("share/mariana-trench/**/*"):
        if path.is_file():
            path = path.relative_to(build_root)
            configuration_directories[path.parent].append(path)

    configuration_files = ""
    for directory, paths in list(configuration_directories.items()):
        configuration_files += f"{directory} =\n"
        for path in paths:
            configuration_files += f"    {path}\n"

    setup_cfg = build_root / "setup.cfg"
    setup_cfg.write_text(
        f"""\
[metadata]
name = {package_name}
version = {package_version}
description = A security focused static analysis platform targeting Android.
long_description = file: README.md
long_description_content_type = text/markdown
url = https://mariana-tren.ch/
download_url = https://github.com/facebook/mariana-trench
author = Facebook
author_email = pyre@fb.com
maintainer = Facebook
maintainer_email = pyre@fb.com
license = MIT
keywords = security, taint, flow, static, analysis, android, java
classifiers =
    Development Status :: 5 - Production/Stable
    Environment :: Console
    Intended Audience :: Developers
    License :: OSI Approved :: MIT License
    Operating System :: MacOS
    Operating System :: POSIX :: Linux
    Programming Language :: Python
    Programming Language :: Python :: 3
    Programming Language :: Python :: 3.6
    Programming Language :: C++
    Topic :: Security
    Topic :: Software Development
    Typing :: Typed

[options]
packages = find:
python_requires = >=3.6
install_requires =
    pyre_extensions
    fb-sapp

[options.entry_points]
console_scripts =
    mariana-trench = mariana_trench.shim.shim:main

[options.data_files]
bin = bin/mariana-trench-binary
{configuration_files}
"""
    )


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--name", type=str, default="mariana-trench", help="Package name."
    )
    parser.add_argument(
        "--version", type=str, default="0.dev0", help="Package version."
    )
    parser.add_argument(
        "--repository",
        type=_directory_exists,
        default=".",
        help="Path to the root of the repository.",
    )
    parser.add_argument(
        "--binary",
        type=_executable_exists,
        help="Path to the analyzer binary.",
        required=True,
    )
    parser.add_argument(
        "--pyredex",
        type=_python_directory_exists,
        help="Path to the pyredex directory.",
        required=True,
    )
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("build", help="Build the pypi package")
    subparsers.add_parser("install", help="Install the pypi package")
    arguments: argparse.Namespace = parser.parse_args()

    repository: Path = arguments.repository
    if (
        not (repository / "README.md").exists()
        and (repository / "../README.md").exists()
    ):
        repository = repository / ".."

    _install_build_dependencies()
    with tempfile.TemporaryDirectory() as build_root:
        build_path = Path(build_root)
        _prepare_build_directory(
            build_path,
            arguments.name,
            arguments.version,
            repository,
            arguments.binary,
            arguments.pyredex,
        )

        if arguments.command == "build":
            _build_package(build_path)
            _copy_package(build_path, Path(os.getcwd()))
        elif arguments.command == "install":
            _install_package(build_path)


if __name__ == "__main__":
    main()
