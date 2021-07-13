#!/bin/bash
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Removes all `*.actual` suffixes from files in the directory above this.

set -e
set -x

THIS_DIRECTORY=$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)

for file in $(find "${THIS_DIRECTORY}/../" -name "*.actual"); do
  mv "$file" "${file%.actual}"
done
