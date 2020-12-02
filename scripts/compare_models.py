#!/usr/bin/env python3
# (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compare the given JSON model files and overwrite such files with sorted contents."
    )
    parser.add_argument("first", type=str, help="The first model file to compare.")
    parser.add_argument("second", type=str, help="The second model file to compare.")

    arguments: argparse.Namespace = parser.parse_args()

    file_1 = Path(arguments.first)
    file_2 = Path(arguments.second)

    data_1 = json.loads(file_1.read_text())
    data_2 = json.loads(file_2.read_text())

    data_1 = _recursive_sort(data_1)
    data_2 = _recursive_sort(data_2)

    file_1.write_text(json.dumps(data_1, indent=2, sort_keys=True))
    file_2.write_text(json.dumps(data_2, indent=2, sort_keys=True))

    try:
        subprocess.check_call(
            [
                "diff",
                "--ignore-all-space",
                "--ignore-space-change",
                "--ignore-blank-lines",
                "--minimal",
                "--unified",
                file_1.resolve(),
                file_2.resolve(),
            ]
        )
    except subprocess.CalledProcessError:
        pass
