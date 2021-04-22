#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import os
import sys
import tempfile
from pathlib import Path

# DO NOT USE THIS IN PRODUCTION.
# This script provides a simple wrapper to the shim that can be used directly
# from the `build` directory. This uses symbolic links to the repository, so it
# can be used for quick development.

REPOSITORY_ROOT: Path = Path("${REPOSITORY_ROOT}")
BUILD_ROOT: Path = Path("${BUILD_ROOT}")
REDEX_ROOT: Path = Path("${REDEX_ROOT}")


def _sync_python_files(fake_root: Path) -> None:
    (fake_root / "mariana_trench").mkdir()
    (fake_root / "mariana_trench" / "__init__.py").write_text("")
    (fake_root / "mariana_trench" / "shim").mkdir()
    (fake_root / "mariana_trench" / "shim" / "__init__.py").write_text("")
    for path in (REPOSITORY_ROOT / "shim").glob("*"):
        (fake_root / "mariana_trench" / "shim" / path.name).symlink_to(path)
    (fake_root / "pyredex").symlink_to(REDEX_ROOT / "bin" / "pyredex")


def _sync_configuration_files(fake_root: Path) -> None:
    (fake_root / "share").mkdir()
    (fake_root / "share" / "mariana-trench").mkdir()
    (fake_root / "share" / "mariana-trench" / "configuration").symlink_to(
        REPOSITORY_ROOT / "configuration"
    )


def _sync_binary(fake_root: Path) -> None:
    binary_path = BUILD_ROOT / "mariana-trench-binary"
    if not binary_path.exists():
        raise AssertionError("Missing mariana-trench-binary, run make first.")

    (fake_root / "bin").mkdir()
    (fake_root / "bin" / "mariana-trench-binary").symlink_to(binary_path)


if __name__ == "__main__":
    if os.fspath(REPOSITORY_ROOT).startswith("$"):
        raise AssertionError("This must be run from the cmake `build` directory.")

    with tempfile.TemporaryDirectory() as root:
        fake_root = Path(root)
        _sync_python_files(fake_root)
        _sync_configuration_files(fake_root)
        _sync_binary(fake_root)

        os.environ["PATH"] = "%s:%s" % (
            os.fspath(fake_root / "bin"),
            os.environ["PATH"],
        )
        sys.path.insert(0, os.fspath(fake_root))

        # pyre-ignore
        from mariana_trench.shim.shim import main

        main()
