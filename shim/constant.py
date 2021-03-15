# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from enum import Enum, auto


class Constant(Enum):
    SOURCE_ROOT = auto()

    BINARY_TARGET = auto()
    BINARY_BUILD_MODE = auto()

    DESUGAR_TARGET = auto()
    DESUGAR_LOG_CONFIGURATION_PATH = auto()

    DEFAULT_GENERATOR_SEARCH_PATHS = auto()
