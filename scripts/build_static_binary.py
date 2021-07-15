#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional, List, Dict

LOG: logging.Logger = logging.getLogger(__name__)

ZLIB_VERSION: str = "1.2.11"
ZLIB_URL: str = f"https://downloads.sourceforge.net/project/libpng/zlib/{ZLIB_VERSION}/zlib-{ZLIB_VERSION}.tar.gz"

JSONCPP_VERSION: str = "1.9.4"
JSONCPP_URL: str = (
    f"https://github.com/open-source-parsers/jsoncpp/archive/{JSONCPP_VERSION}.tar.gz"
)

GTEST_VERSION: str = "1.10.0"
GTEST_URL: str = (
    f"https://github.com/google/googletest/archive/release-{GTEST_VERSION}.tar.gz"
)

FMT_VERSION: str = "7.1.3"
FMT_URL: str = f"https://github.com/fmtlib/fmt/archive/{FMT_VERSION}.tar.gz"

RE2_VERSION: str = "2021-04-01"
RE2_URL: str = f"https://github.com/google/re2/archive/{RE2_VERSION}.tar.gz"

BOOST_VERSION: str = "1.76.0"
BOOST_URL: str = f"https://boostorg.jfrog.io/artifactory/main/release/{BOOST_VERSION}/source/boost_{BOOST_VERSION.replace('.', '_')}.tar.bz2"

REDEX_URL: str = "https://github.com/facebook/redex.git"


def _directory_exists(path: str) -> Path:
    path = os.path.expanduser(path)
    if not os.path.isdir(path):
        raise argparse.ArgumentTypeError(f"Path `{path}` is not a directory.")
    return Path(path)


def _run(
    command: List[str],
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
) -> "subprocess.CompletedProcess[str]":
    LOG.info(f"Running {' '.join(command)}")
    return subprocess.run(command, check=True, text=True, cwd=cwd, env=env)


def _download_and_extract(
    url: str, filename: str, work_directory: Path, extract_directory: Path
) -> None:
    download_directory = work_directory / "download"
    download_directory.mkdir(exist_ok=True)
    _run(
        ["curl", "-fL", "-o", filename, "--retry", "3", url],
        cwd=download_directory,
    )
    extract_directory.mkdir()
    _run(
        ["tar", "-x", "-C", str(extract_directory), "-f", filename],
        cwd=download_directory,
    )


def _flatten_directories(root: Path) -> None:
    """
    Given a path, remove any directory indirection.
    For instance, if we have a directory hierarchy:
    /a/b/c/foo
    /a/b/c/bar
    /a/b/c/bar/baz
    Given the path `/a`, we flatten it to:
    /a/foo
    /a/bar
    /a/bar/baz
    """
    subdirectory = root
    first_subdirectory = None
    while len(list(subdirectory.iterdir())) == 1:
        subdirectory = next(subdirectory.iterdir())
        if first_subdirectory is None:
            first_subdirectory = subdirectory

    if first_subdirectory is None:
        return

    for path in subdirectory.iterdir():
        path.replace(root / path.name)

    shutil.rmtree(first_subdirectory)


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a static binary for Mariana Trench."
    )
    parser.add_argument(
        "--repository",
        metavar="PATH",
        type=_directory_exists,
        default=".",
        help="Path to the root of the repository.",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=multiprocessing.cpu_count(),
        help="Number of jobs.",
    )
    parser.add_argument(
        "--output",
        metavar="PATH",
        type=str,
        default="mariana-trench-binary",
        help="Path to the output binary.",
    )
    arguments = parser.parse_args()

    if (
        not (arguments.repository / "README.md").exists()
        and (arguments.repository / "../README.md").exists()
    ):
        arguments.repository = (arguments.repository / "..").resolve(strict=True)

    return arguments


def _build_zlib(arguments: argparse.Namespace, work_directory: Path) -> Path:
    LOG.info(f"Building zib {ZLIB_VERSION}")
    zlib_build_directory = work_directory / f"build/zlib-{ZLIB_VERSION}"
    zlib_install_directory = work_directory / f"install/zlib-{ZLIB_VERSION}"
    _download_and_extract(
        ZLIB_URL,
        f"zlip-{ZLIB_VERSION}.tar.gz",
        work_directory,
        zlib_build_directory,
    )
    zlib_build_directory /= f"zlib-{ZLIB_VERSION}"
    _run(
        ["./configure", f"--prefix={zlib_install_directory}"],
        cwd=zlib_build_directory,
    )
    _run(
        ["make", "-j", str(arguments.jobs), "install"],
        cwd=zlib_build_directory,
    )
    return zlib_install_directory


def _build_jsoncpp(
    arguments: argparse.Namespace,
    work_directory: Path,
) -> Path:
    LOG.info(f"Building jsoncpp {JSONCPP_VERSION}")
    jsoncpp_build_directory = work_directory / f"build/jsoncpp-{JSONCPP_VERSION}"
    jsoncpp_install_directory = work_directory / f"install/jsoncpp-{JSONCPP_VERSION}"
    _download_and_extract(
        JSONCPP_URL,
        f"jsoncpp-{JSONCPP_VERSION}.tar.gz",
        work_directory,
        jsoncpp_build_directory,
    )
    jsoncpp_build_directory /= f"jsoncpp-{JSONCPP_VERSION}"
    _run(
        [
            "meson",
            "--buildtype",
            "release",
            "--default-library",
            "static",
            "--libdir",
            "lib",
            ".",
            "build-static",
        ],
        cwd=jsoncpp_build_directory,
    )
    _run(
        ["ninja", "-v", "-j", str(arguments.jobs)],
        cwd=jsoncpp_build_directory / "build-static",
    )
    jsoncpp_install_directory.mkdir()
    _run(
        ["ninja", "-v", "install"],
        cwd=jsoncpp_build_directory / "build-static",
        env={**os.environ, "DESTDIR": str(jsoncpp_install_directory)},
    )
    _flatten_directories(jsoncpp_install_directory)
    return jsoncpp_install_directory


def _build_boost(
    arguments: argparse.Namespace,
    work_directory: Path,
) -> Path:
    LOG.info(f"Building boost {BOOST_VERSION}")
    boost_build_directory = work_directory / f"build/boost-{BOOST_VERSION}"
    boost_install_directory = work_directory / f"install/boost-{BOOST_VERSION}"
    _download_and_extract(
        BOOST_URL,
        f"boost-{BOOST_VERSION}.tar.bz2",
        work_directory,
        boost_build_directory,
    )
    boost_build_directory /= f"boost_{BOOST_VERSION.replace('.', '_')}"
    _run(
        [
            "./bootstrap.sh",
            f"--prefix={boost_install_directory}",
            "--without-icu",
            "--with-libraries=system",
            "--with-libraries=filesystem",
            "--with-libraries=regex",
            "--with-libraries=program_options",
            "--with-libraries=iostreams",
            "--with-libraries=thread",
        ],
        cwd=boost_build_directory,
    )
    _run(
        [
            "./b2",
            "-d0",
            f"-j{arguments.jobs}",
            "install",
            "threading=multi",
            "link=static",
            "-sNO_LZMA=1",
            "-sNO_ZSTD=1",
            "-sNO_BZIP2=1",
            "-sNO_ZLIB=1",
        ],
        cwd=boost_build_directory,
    )
    return boost_install_directory


def _build_gtest(arguments: argparse.Namespace, work_directory: Path) -> Path:
    LOG.info(f"Building gtest {GTEST_VERSION}")
    gtest_build_directory = work_directory / f"build/gtest-{GTEST_VERSION}"
    gtest_install_directory = work_directory / f"install/gtest-{GTEST_VERSION}"
    _download_and_extract(
        GTEST_URL,
        f"gtest-{GTEST_VERSION}.tar.gz",
        work_directory,
        gtest_build_directory,
    )
    gtest_build_directory /= f"googletest-release-{GTEST_VERSION}"
    _run(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={gtest_install_directory}", "."],
        cwd=gtest_build_directory,
    )
    _run(["make", f"-j{arguments.jobs}"], cwd=gtest_build_directory)
    _run(["make", "install"], cwd=gtest_build_directory)
    return gtest_install_directory


def _build_fmt(arguments: argparse.Namespace, work_directory: Path) -> Path:
    LOG.info(f"Building fmt {FMT_VERSION}")
    fmt_build_directory = work_directory / f"build/fmt-{FMT_VERSION}"
    fmt_install_directory = work_directory / f"install/fmt-{FMT_VERSION}"
    _download_and_extract(
        FMT_URL,
        f"fmt-{FMT_VERSION}.tar.gz",
        work_directory,
        fmt_build_directory,
    )
    fmt_build_directory /= f"fmt-{FMT_VERSION}"
    _run(
        [
            "cmake",
            "-DBUILD_SHARED_LIBS=FALSE",
            f"-DCMAKE_INSTALL_PREFIX={fmt_install_directory}",
            ".",
        ],
        cwd=fmt_build_directory,
    )
    _run(["make", f"-j{arguments.jobs}"], cwd=fmt_build_directory)
    _run(["make", "install"], cwd=fmt_build_directory)
    return fmt_install_directory


def _build_re2(arguments: argparse.Namespace, work_directory: Path) -> Path:
    LOG.info(f"Building re2 {RE2_VERSION}")
    re2_build_directory = work_directory / f"build/re2-{RE2_VERSION}"
    re2_install_directory = work_directory / f"install/re2-{RE2_VERSION}"
    _download_and_extract(
        RE2_URL,
        f"re2-{RE2_VERSION}.tar.gz",
        work_directory,
        re2_build_directory,
    )
    re2_build_directory /= f"re2-{RE2_VERSION}"
    _run(
        [
            "cmake",
            "-DBUILD_SHARED_LIBS=FALSE",
            "-DRE2_BUILD_TESTING=OFF",
            f"-DCMAKE_INSTALL_PREFIX={re2_install_directory}",
            ".",
        ],
        cwd=re2_build_directory,
    )
    _run(["make", f"-j{arguments.jobs}"], cwd=re2_build_directory)
    _run(["make", "install"], cwd=re2_build_directory)
    return re2_install_directory


def _build_redex(
    arguments: argparse.Namespace,
    work_directory: Path,
    zlib: Path,
    jsoncpp: Path,
    boost: Path,
) -> Path:
    LOG.info("Building redex")
    redex_build_directory = work_directory / "build/redex-master"
    redex_install_directory = work_directory / "install/redex-master"
    _run(
        ["git", "clone", "--depth=1", REDEX_URL, redex_build_directory.name],
        cwd=redex_build_directory.parent,
    )
    redex_build_directory /= "build"
    redex_build_directory.mkdir()
    _run(
        [
            "cmake",
            "-DBUILD_TYPE=Static",
            f"-DCMAKE_INSTALL_PREFIX={redex_install_directory}",
            f"-DZLIB_HOME={zlib}",
            f"-DJSONCPP_DIR={jsoncpp}",
            f"-DBOOST_ROOT={boost}",
            "..",
        ],
        cwd=redex_build_directory,
    )
    _run(["make", f"-j{arguments.jobs}"], cwd=redex_build_directory)
    _run(["make", "install"], cwd=redex_build_directory)
    return redex_install_directory


def _build_mariana_trench(
    arguments: argparse.Namespace,
    work_directory: Path,
    zlib: Path,
    jsoncpp: Path,
    gtest: Path,
    fmt: Path,
    re2: Path,
    boost: Path,
    redex: Path,
) -> Path:
    LOG.info("Building mariana trench")
    build_directory = work_directory / "build/mariana-trench"
    build_directory.mkdir()
    install_directory = work_directory / "install/mariana-trench"
    _run(
        [
            "cmake",
            "-DLINK_TYPE=Static",
            f"-DCMAKE_INSTALL_PREFIX={install_directory}",
            f"-DZLIB_ROOT={zlib}",
            f"-DJSONCPP_DIR={jsoncpp}",
            f"-DGTest_DIR={gtest}/lib/cmake/GTest",
            f"-Dfmt_DIR={fmt}/lib/cmake/fmt",
            f"-Dre2_DIR={re2}/lib/cmake/re2",
            f"-DBOOST_ROOT={boost}",
            f"-DREDEX_ROOT={redex}",
            str(arguments.repository.resolve()),
        ],
        cwd=build_directory,
    )
    _run(["make", f"-j{arguments.jobs}"], cwd=build_directory)
    _run(["make", "install"], cwd=build_directory)
    return install_directory


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    arguments = _parse_arguments()

    if sys.platform not in ("darwin", "linux"):
        raise AssertionError("This operating system is not currently supported.")

    with tempfile.TemporaryDirectory(prefix="mt-static-") as directory:
        work_directory = Path(directory)
        LOG.info(f"Repository root is `{arguments.repository}`.")
        LOG.info(f"Using {arguments.jobs} concurrent jobs.")
        LOG.info(f"Work directory is `{work_directory}`.")
        build_directory = work_directory / "build"
        build_directory.mkdir()
        install_directory = work_directory / "install"
        install_directory.mkdir()

        zlib = _build_zlib(arguments, work_directory)
        jsoncpp = _build_jsoncpp(arguments, work_directory)
        gtest = _build_gtest(arguments, work_directory)
        fmt = _build_fmt(arguments, work_directory)
        re2 = _build_re2(arguments, work_directory)
        boost = _build_boost(arguments, work_directory)
        redex = _build_redex(arguments, work_directory, zlib, jsoncpp, boost)
        mariana_trench = _build_mariana_trench(
            arguments,
            work_directory,
            zlib,
            jsoncpp,
            gtest,
            fmt,
            re2,
            boost,
            redex,
        )

        LOG.info(f"Moving binary to `{arguments.output}`")
        (mariana_trench / "bin/mariana-trench-binary").rename(Path(arguments.output))


if __name__ == "__main__":
    main()
