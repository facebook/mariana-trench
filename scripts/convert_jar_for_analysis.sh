#!/bin/sh
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

D8=/opt/android/sdk_DEFAULT/build-tools/29.0.2/d8
$D8 --min-api 24 --output classes_for_analysis.zip "$@"
