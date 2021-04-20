# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from pathlib import Path
from typing import Optional

FACEBOOK_SHIM: bool = False

SOURCE_ROOT: Path = Path(".")

BINARY_BUCK_TARGET: Optional[str] = None
BINARY_BUCK_BUILD_MODE: Optional[str] = None
BINARY_PATH_COMMAND: Optional[str] = "mariana-trench-binary"

DESUGAR_BUCK_TARGET: Optional[str] = None
DESUGAR_LOG_CONFIGURATION_PATH: Optional[str] = None

DEFAULT_GENERATOR_SEARCH_PATHS: str = "<generator search path>"

DEFAULT_MAXIMUM_SOURCE_SINK_DISTANCE: int = 99
