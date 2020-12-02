#!/usr/bin/env python3
# (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

import json
import multiprocessing
import os
import re
import subprocess
from typing import Any, Union, Dict, Tuple, Iterable, NamedTuple

"""
Set of functions that can be used in bento to explore models.
Usage: bento console -i --file explore_models.py
"""

Model = Dict[str, Any]


class FilePosition(NamedTuple):
    path: str
    offset: int
    length: int


__index: Dict[str, FilePosition] = {}


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


def _index_file(path: str) -> Dict[str, FilePosition]:
    print(f"Indexing `{path}`")
    index = {}

    with open(path, "rb") as file:
        for line, offset in _iter_with_offset(file):
            if line.startswith(b"//"):
                continue

            model = json.loads(line)
            method = _method_string(model["method"])

            index[method] = FilePosition(path=path, offset=offset, length=len(line))

    return index


def index(results_directory: str = ".") -> None:
    """Index all available models in the given directory."""
    global __index
    __index = {}

    paths = []
    for path in os.listdir(results_directory):
        if not path.startswith("model@"):
            continue

        paths.append(os.path.join(results_directory, path))

    with multiprocessing.Pool() as pool:
        for index in pool.imap_unordered(_index_file, paths):
            __index.update(index)

    print(f"Indexed {len(__index)} models")


def _assert_loaded() -> None:
    if len(__index) == 0:
        raise AssertionError("call index() first")


def method_containing(string: str):
    """Find all methods containing the given string."""
    _assert_loaded()
    return sorted(filter(lambda name: string in name, __index.keys()))


def method_matching(pattern: str):
    """Find all methods matching the given regular expression."""
    _assert_loaded()
    regex = re.compile(pattern)
    return sorted(filter(lambda name: re.search(regex, name), __index.keys()))


def _get_model_bytes(method: str) -> bytes:
    _assert_loaded()

    if method not in __index:
        raise AssertionError(f"no model for method `{method}`.")

    file_position = __index[method]
    with open(file_position.path, "rb") as file:
        file.seek(file_position.offset)
        return file.read(file_position.length)


def get_model(method: str) -> Model:
    """Get the model for the given method."""
    return json.loads(_get_model_bytes(method))


def print_model(method: str) -> None:
    """Pretty print the model for the given method."""
    model = _get_model_bytes(method)
    subprocess.run(["jq", "-C"], input=model, check=True)


def print_help() -> None:
    print("# Mariana Trench Model Explorer")
    print("Available commands:")
    commands = [
        (index, "index('.')"),
        (method_containing, "method_containing('Foo;.bar')"),
        (method_matching, "method_matching('Foo.*')"),
        (get_model, "get_model('Foo;.bar')"),
        (print_model, "print_model('Foo;.bar')"),
    ]
    max_width = max(len(command[1]) for command in commands)
    for command, example in commands:
        print(f"  {example:<{max_width}} {command.__doc__}")


if __name__ == "__main__":
    print_help()
