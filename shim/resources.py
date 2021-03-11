# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from pathlib import Path

from .constant import Constant


def get_string(constant: Constant) -> str:
    raise NotImplementedError(f"Unknown string constant `{constant}`")


def get_path(constant: Constant) -> Path:
    if constant == Constant.SOURCE_ROOT:
        return Path(".")

    raise NotImplementedError(f"Unknown path constant `{constant}`")
