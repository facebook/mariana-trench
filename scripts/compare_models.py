#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import subprocess
from collections import OrderedDict
from pathlib import Path


def _recursive_sort(data: object) -> object:
    if isinstance(data, list):
        list_sorted = []
        for item in data:
            # The dump is necessary because list elements might be dictionaries
            # Hence we first stringify all elements regardless of types and then sort them
            list_sorted.append(json.dumps(_recursive_sort(item), sort_keys=True))
        list_sorted.sort()

        # Ensure contents in the return value are json values (instead of strings)
        results = []
        for item in list_sorted:
            results.append(json.loads(item, object_pairs_hook=OrderedDict))
        return results
    elif isinstance(data, dict):
        # sort keys
        keys_sorted = sorted(data.keys())
        # sort values
        results = OrderedDict()
        for key in keys_sorted:
            results[key] = _recursive_sort(data[key])
        return results
    else:
        return data


def process_file(input_file: Path) -> None:
    with open(input_file, "r") as file:
        data: object = json.load(file)
    data = _recursive_sort(data)
    with open(input_file, "w") as file:
        json.dump(data, file, indent=2, sort_keys=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compare the given JSON model files and overwrite such files with sorted contents."
    )
    parser.add_argument("first", type=str, help="The first model file path to compare.")
    parser.add_argument(
        "second", type=str, help="The second model file path to compare."
    )
    parser.add_argument(
        "-d",
        "--directories",
        action="store_true",
        help="If specified, indicates that the paths provided are directories containing (only) model json files.",
    )

    arguments: argparse.Namespace = parser.parse_args()

    path_1 = Path(arguments.first)
    path_2 = Path(arguments.second)
    directories: bool = arguments.directories
    if directories:
        assert (
            path_1.is_dir() and path_2.is_dir()
        ), "Both paths must be directories if --directories(-d) flag is specified"

        for model_file in list(path_1.glob("*.json")):
            process_file(model_file)
        for model_file in list(path_2.glob("*.json")):
            process_file(model_file)
    else:
        process_file(path_1)
        process_file(path_2)

    try:
        subprocess.check_call(
            [
                "diff",
                "--ignore-all-space",
                "--ignore-space-change",
                "--ignore-blank-lines",
                "--minimal",
                "--unified",
                path_1.resolve(),
                path_2.resolve(),
            ]
        )
    except subprocess.CalledProcessError:
        pass
