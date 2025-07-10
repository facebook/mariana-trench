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
from typing import Any, Dict, Iterable, List, NamedTuple, Tuple, Union

"""
Set of functions that can be used to explore models.
Usage: python3 -i explore_models.py
"""

Model = Dict[str, Any]


class FilePosition(NamedTuple):
    path: str
    offset: int
    length: int


__model_index: Dict[str, FilePosition] = {}
__field_index: Dict[str, FilePosition] = {}
__callgraph_index: Dict[str, FilePosition] = {}
__dependencies_index: Dict[str, FilePosition] = {}


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


def _index_models_file(
    path: str,
) -> Tuple[Dict[str, FilePosition], Dict[str, FilePosition]]:
    print(f"Indexing `{path}`")
    index = {}
    field_index = {}

    with open(path, "rb") as file:
        for line, offset in _iter_with_offset(file):
            if line.startswith(b"//"):
                continue

            position = FilePosition(path=path, offset=offset, length=len(line))
            model = json.loads(line)
            if "method" in list(model.keys()):
                method = _method_string(model["method"])
                index[method] = position
            else:
                field_index[model["field"]] = position

    return (index, field_index)


def _index_callgraph_file(path: str) -> Dict[str, FilePosition]:
    print(f"Indexing `{path}`")
    callgraphs = {}

    with open(path, "rb") as file:
        for line, offset in _iter_with_offset(file):
            if line.startswith(b"//"):
                continue

            position = FilePosition(path=path, offset=offset, length=len(line))
            callgraph = json.loads(line)

            method = list(callgraph.keys())
            if len(method) != 1:
                raise AssertionError(
                    f"Expected a single key in the callgraph or dependencies json. Found {len(method)}"
                )

            callgraphs[method[0]] = position

    return callgraphs


def index_models(results_directory: str = ".") -> None:
    """Index all available method and field models in the given directory."""
    global __model_index, __field_index
    __model_index = {}
    __field_index = {}

    paths = []
    for path in os.listdir(results_directory):
        if not path.startswith("model@"):
            continue

        paths.append(os.path.join(results_directory, path))

    with multiprocessing.Pool() as pool:
        for index, field_index in pool.imap_unordered(_index_models_file, paths):
            __model_index.update(index)
            __field_index.update(field_index)


def index_callgraphs(results_directory: str = ".") -> None:
    """Index all available callgraphs in the given directory."""
    global __callgraph_index, __dependencies_index
    __callgraph_index = {}
    __dependencies_index = {}

    callgraph_paths = []
    dependencies_paths = []
    for path in os.listdir(results_directory):
        if path.startswith("call-graph@"):
            callgraph_paths.append(os.path.join(results_directory, path))
        elif path.startswith("dependencies@"):
            dependencies_paths.append(os.path.join(results_directory, path))
        else:
            continue

    with multiprocessing.Pool() as pool:
        for call_graph in pool.imap_unordered(_index_callgraph_file, callgraph_paths):
            __callgraph_index.update(call_graph)

        for dependency in pool.imap_unordered(
            _index_callgraph_file, dependencies_paths
        ):
            __dependencies_index.update(dependency)

    print(f"{len(__callgraph_index)} callgraphs")
    print(f"{len(__dependencies_index)} dependencies")


def index(results_directory: str = ".") -> None:
    """Index all available models, fields, and callgraphs in the given directory."""
    index_models(results_directory)
    index_callgraphs(results_directory)


def _assert_loaded() -> None:
    if len(__model_index) == 0 and len(__callgraph_index) == 0:
        raise AssertionError("call index() first")


def _index_keys_containing(index: Dict[str, Any], string: str) -> List[str]:
    _assert_loaded()
    return sorted([name for name in list(index.keys()) if string in name])


def method_containing(string: str) -> List[str]:
    """Find all methods containing the given string."""
    if len(__model_index) > 0:
        return _index_keys_containing(__model_index, string)
    else:
        return _index_keys_containing(__callgraph_index, string)


def field_containing(string: str) -> List[str]:
    """Find all fields containing the given string."""
    return _index_keys_containing(__field_index, string)


def _index_keys_matching(index: Dict[str, Any], pattern: str) -> List[str]:
    _assert_loaded()
    regex = re.compile(pattern)
    return sorted([name for name in list(index.keys()) if re.search(regex, name)])


def method_matching(pattern: str) -> List[str]:
    """Find all methods matching the given regular expression."""
    return _index_keys_matching(__model_index, pattern)


def field_matching(pattern: str) -> List[str]:
    """Find all fields matching the given regular expression."""
    return _index_keys_matching(__field_index, pattern)


def _get_bytes(key: str, index: Dict[str, Any]) -> bytes:
    _assert_loaded()

    if key not in index:
        raise AssertionError(f"no model for `{key}`.")

    file_position = index[key]
    with open(file_position.path, "rb") as file:
        file.seek(file_position.offset)
        return file.read(file_position.length)


def get_model(method: str) -> Model:
    """Get the model for the given method."""
    return json.loads(_get_bytes(method, __model_index))


def get_field_model(field: str) -> Model:
    """Get the model for the given field."""
    return json.loads(_get_bytes(field, __field_index))


def _print_helper(input: bytes) -> None:
    try:
        subprocess.run(["jq", "-C"], input=input, check=True)
    except FileNotFoundError:
        # User doesn't have jq installed
        json.dump(json.loads(input), sys.stdout, indent=2)
        sys.stdout.write("\n")


def print_model(method: str) -> None:
    """Pretty print the model for the given method."""
    _print_helper(_get_bytes(method, __model_index))


def print_field_model(field: str) -> None:
    """Pretty print the model for the given field."""
    _print_helper(_get_bytes(field, __field_index))


def print_callees(method: str) -> None:
    """Pretty print the callees for the given method."""
    _print_helper(_get_bytes(method, __callgraph_index))


def print_callers(method: str) -> None:
    """Pretty print the callers for the given method."""
    _print_helper(_get_bytes(method, __dependencies_index))


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
        (index_models, "index_models('.')"),
        (index_callgraphs, "index_callgraphs('.')"),
        (method_containing, "method_containing('Foo;.bar')"),
        (field_containing, "field_containing('Foo;.bar')"),
        (method_matching, "method_matching('Foo.*')"),
        (field_matching, "field_matching('Foo.*')"),
        (get_model, "get_model('Foo;.bar')"),
        (get_field_model, "get_field_model('Foo;.bar')"),
        (print_model, "print_model('Foo;.bar')"),
        (print_field_model, "print_field_model('Foo;.bar')"),
        (print_callees, "print_callees('Foo;.bar')"),
        (print_callers, "print_callers('Foo;.bar')"),
        (dump_model, "dump_model('Foo;.bar', 'bar.json', [indent=2])"),
        (dump_field_model, "dump_field_model('Foo;.bar', 'bar.json', [indent=2])"),
    ]
    max_width = max(len(command[1]) for command in commands)
    for command, example in commands:
        print(f"  {example:<{max_width}} {command.__doc__}")


if __name__ == "__main__":
    print_help()
