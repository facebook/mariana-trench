# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import enum


class ExitCode(enum.IntEnum):
    SUCCESS = 0
    # 1-99 reserved for mariana-trench binary
    ERROR = 100
    BUCK_ERROR = 101
    CONFIGURATION_ERROR = 102
    JAVA_TARGET_ERROR = 103


class ClientError(Exception):
    exit_code: ExitCode

    def __init__(self, message: str, exit_code: ExitCode = ExitCode.ERROR) -> None:
        super().__init__(message)
        self.exit_code = exit_code


class ConfigurationError(ClientError):
    def __init__(self, message: str) -> None:
        super().__init__(message, ExitCode.CONFIGURATION_ERROR)
