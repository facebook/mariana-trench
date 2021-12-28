#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import json
import multiprocessing
import os
import re
import subprocess
import sys
from typing import Any, Union, Dict, Tuple, Iterable, NamedTuple, List

"""
Set of functions that can be used to explore models.
Usage: python3 -i explore_models.py
"""

Model = Dict[str, Any]


class FilePosition(NamedTuple):
    path: str
    offset: int
    length: int


__index: Dict[str, FilePosition] = {}
__field_index: Dict[str, FilePosition] = {}


def _method_string(method: Union[str, Dict[str, Any]]) -> str:
    if isinstance(method, str):
        return method

    name = method["name"]

    parameter_type_overrides = method.get("parameter_type_overrides")
    if parameter_type_overrides:
        parameter_type_overrides = (
            f"{override['parameter']}: {override['type']}"
            for override in parameter_type_overrides
        )
        name += "[%s]" % ", ".join(parameter_type_overrides)

    return name


def _iter_with_offset(lines: Iterable[bytes]) -> Iterable[Tuple[bytes, int]]:
    offset = 0
    for line in lines:
        yield (line, offset)
        offset += len(line)


def _index_file(path: str) -> Tuple[Dict[str, FilePosition], Dict[str, FilePosition]]:
    print(f"Indexing `{path}`")
    index = {}
    field_index = {}

    with open(path, "rb") as file:
        for line, offset in _iter_with_offset(file):
            if line.startswith(b"//"):
                continue

            position = FilePosition(path=path, offset=offset, length=len(line))
            model = json.loads(line)
            if "method" in model.keys():
                method = _method_string(model["method"])
                index[method] = position
            else:
                field_index[model["field"]] = position

    return (index, field_index)


def index(results_directory: str = ".") -> None:
    """Index all available method and field models in the given directory."""
    global __index, __field_index
    __index = {}
    __field_index = {}

    paths = []
    for path in os.listdir(results_directory):
        if not path.startswith("model@"):
            continue

        paths.append(os.path.join(results_directory, path))

    with multiprocessing.Pool() as pool:
        for index, field_index in pool.imap_unordered(_index_file, paths):
            __index.update(index)
            __field_index.update(field_index)

    print(f"Indexed {len(__index)} models")


def _assert_loaded() -> None:
    if len(__index) == 0:
        raise AssertionError("call index() first")


def _index_keys_containing(index: Dict[str, Any], string: str) -> List[str]:
    _assert_loaded()
    return sorted(filter(lambda name: string in name, index.keys()))


def method_containing(string: str) -> List[str]:
    """Find all methods containing the given string."""
    return _index_keys_containing(__index, string)


def field_containing(string: str) -> List[str]:
    """Find all fields containing the given string."""
    return _index_keys_containing(__field_index, string)


def _index_keys_matching(index: Dict[str, Any], pattern: str) -> List[str]:
    _assert_loaded()
    regex = re.compile(pattern)
    return sorted(filter(lambda name: re.search(regex, name), index.keys()))


def method_matching(pattern: str) -> List[str]:
    """Find all methods matching the given regular expression."""
    return _index_keys_matching(__index, pattern)


def field_matching(pattern: str) -> List[str]:
    """Find all fields matching the given regular expression."""
    return _index_keys_matching(__field_index, pattern)


def _get_model_bytes(key: str, index: Dict[str, Any]) -> bytes:
    _assert_loaded()

    if key not in index:
        raise AssertionError(f"no model for `{key}`.")

    file_position = index[key]
    with open(file_position.path, "rb") as file:
        file.seek(file_position.offset)
        return file.read(file_position.length)


def get_model(method: str) -> Model:
    """Get the model for the given method."""
    return json.loads(_get_model_bytes(method, __index))


def get_field_model(field: str) -> Model:
    """Get the model for the given field."""
    return json.loads(_get_model_bytes(field, __field_index))


def _print_model_helper(model: bytes) -> None:
    try:
        subprocess.run(["jq", "-C"], input=model, check=True)
    except FileNotFoundError:
        # User doesn't have jq installed
        json.dump(json.loads(model), sys.stdout, indent=2)
        sys.stdout.write("\n")


def print_model(method: str) -> None:
    """Pretty print the model for the given method."""
    _print_model_helper(_get_model_bytes(method, __index))


def print_field_model(field: str) -> None:
    """Pretty print the model for the given field."""
    _print_model_helper(_get_model_bytes(field, __field_index))


def dump_model(method: str, filename: str, indent: int = 2) -> None:
    """Dump model for the given method to given filename."""
    with open(filename, "w") as f:
        f.write(json.dumps(get_model(method), indent=indent))


def dump_field_model(field: str, filename: str, indent: int = 2) -> None:
    """Dump model for the given field to given filename."""
    with open(filename, "w") as f:
        f.write(json.dumps(get_field_model(field), indent=indent))


def print_help() -> None:
    print("# Mariana Trench Model Explorer")
    print("Available commands:")
    commands = [
        (index, "index('.')"),
        (method_containing, "method_containing('Foo;.bar')"),
        (field_containing, "field_containing('Foo;.bar')"),
        (method_matching, "method_matching('Foo.*')"),
        (field_matching, "field_matching('Foo.*')"),
        (get_model, "get_model('Foo;.bar')"),
        (get_field_model, "get_field_model('Foo;.bar')"),
        (print_model, "print_model('Foo;.bar')"),
        (print_field_model, "print_field_model('Foo;.bar')"),
        (dump_model, "dump_model('Foo;.bar', 'bar.json', [indent=2])"),
        (dump_field_model, "dump_field_model('Foo;.bar', 'bar.json', [indent=2])"),
    ]
    max_width = max(len(command[1]) for command in commands)
    for command, example in commands:
        print(f"  {example:<{max_width}} {command.__doc__}")


if __name__ == "__main__":
    print_help()
