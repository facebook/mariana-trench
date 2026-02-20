#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set -euo pipefail

STUB_DIR="documentation/fb"

list_files() {
    grep -roh "from './fb/[^']*\.md'" documentation/ \
        | grep -o "[^/]*\.md" \
        | sort -u \
        || true
}

generate_stubs() {
    local files
    files=$(list_files)

    if [[ -z "$files" ]]; then
        return
    fi

    mkdir -p "$STUB_DIR"

    while IFS= read -r file; do
        touch "$STUB_DIR/$file"
    done <<< "$files"
}

cleanup_stubs() {
    rm -rf "$STUB_DIR"
}

case "${1:-}" in
    generate)
        generate_stubs
        ;;
    cleanup)
        cleanup_stubs
        ;;
    *)
        echo "Usage: $0 {generate|cleanup}"
        exit 1
        ;;
esac
