# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import os
from pathlib import Path
from typing import List, Optional


class Error(Exception):
    pass


FACEBOOK_SHIM: bool = False

BINARY_BUCK_TARGET: Optional[str] = None
BINARY_BUCK_BUILD_MODE: Optional[str] = None
BINARY_PATH_COMMAND: Optional[str] = "mariana-trench-binary"

DESUGAR_BUCK_TARGET: Optional[str] = None
DESUGAR_LOG_CONFIGURATION_PATH: Optional[str] = None

DEFAULT_MAXIMUM_SOURCE_SINK_DISTANCE: int = 7


def _get_configuration_directory() -> Path:
    for directory in os.environ["PATH"].split(os.pathsep):
        path = Path(directory).parent / "share/mariana-trench/configuration"
        if path.is_dir():
            return path

    raise Error("Could not find `share/mariana-trench/configuration` in PATH.")


def get_path(filename: str) -> Path:
    path = _get_configuration_directory() / filename
    if not path.is_file():
        raise Error(
            f"Could not find `{filename}` in `{_get_configuration_directory()}`."
        )
    return path


def get_default_generator_search_paths() -> List[Path]:
    return [_get_configuration_directory() / "model-generators"]
