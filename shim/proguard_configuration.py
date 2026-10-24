# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

from .exit_codes import ConfigurationError


def resolve_proguard_configuration_from_target(
    value: str,
    repository_root: str,
) -> str:
    """
    Resolve the ProGuard configuration of a build target. This build does not
    support it: pass `--assume-no-side-effects-configuration-paths` instead.
    """
    raise ConfigurationError(
        message="`--assume-no-side-effects-from-target` is not supported in this "
        "build. Use `--assume-no-side-effects-configuration-paths`."
    )
