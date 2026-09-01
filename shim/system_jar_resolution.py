# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from typing import List

from pyre_extensions import safe_json

from .exit_codes import ConfigurationError


def jar_path_list(value: object, description: str) -> List[str]:
    try:
        elements = safe_json.validate(value, List[str])
    except safe_json.InvalidJson as error:
        raise ConfigurationError(
            message=f"{description} must be a list of strings."
        ) from error

    paths = [element.strip() for element in elements]
    if not all(paths):
        raise ConfigurationError(message=f"{description} must not contain empty paths.")

    return paths


def expand_system_jar_configuration(
    path: str,
    contents: object,
    repository_root: str,
) -> str:
    """
    Expand a system JAR configuration that is not a plain list of paths.
    This build supports only the list form.
    """
    raise ConfigurationError(message=f"`{path}` must contain a list of jar paths.")
