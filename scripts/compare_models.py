#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import functools
import json
import subprocess
from collections import OrderedDict
from pathlib import Path
from typing import Callable, Collection


def builtin_compare(left: str, right: str) -> int:
    if left < right:
        return -1
    elif left == right:
        return 0
    elif left > right:
        return 1
    else:
        raise AssertionError(f"No strict order between {repr(left)} and {repr(right)}")


def lexicographic_compare(
    left: Collection[object],
    right: Collection[object],
    compare: Callable[[object, object], int],
) -> int:
    for left_element, right_element in zip(left, right):
        result = compare(left_element, right_element)
        if result != 0:
            return result

    return len(left) - len(right)


def _recursive_compare(left: object, right: object) -> int:
    left_type = type(left)
    right_type = type(right)

    if left_type is not right_type:
        return builtin_compare(left_type.__name__, right_type.__name__)

    if isinstance(left, (list, tuple)) and isinstance(right, (list, tuple)):
        return lexicographic_compare(left, right, _recursive_compare)
    elif isinstance(left, OrderedDict) and isinstance(right, OrderedDict):
        return lexicographic_compare(left.items(), right.items(), _recursive_compare)
    elif isinstance(left, int) and isinstance(right, int):
        return left - right
    elif isinstance(left, str) and isinstance(right, str):
        return builtin_compare(left, right)
    else:
        raise AssertionError(f"Unexpected type in _recursive_compare: {left_type}")


def _recursive_sort(data: object) -> object:
    if isinstance(data, list):
        for index in range(len(data)):
            data[index] = _recursive_sort(data[index])
        data.sort(key=functools.cmp_to_key(_recursive_compare))
        return data
    elif isinstance(data, dict):
        # sort keys
        keys_sorted = sorted(data.keys())
        # sort values
        results = OrderedDict()
        for key in keys_sorted:
            results[key] = _recursive_sort(data[key])
        return results
    elif isinstance(data, (int, str)):
        return data
    else:
        raise AssertionError(r"Unexpected type in _recursive_sort: {type(data)}")


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
        assert path_1.is_dir() and path_2.is_dir(), (
            "Both paths must be directories if --directories(-d) flag is specified"
        )

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
                "--new-file",
                path_1.resolve(),
                path_2.resolve(),
            ]
        )
    except subprocess.CalledProcessError:
        pass
