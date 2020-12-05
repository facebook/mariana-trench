#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import sys
import textwrap
from pathlib import Path

import jsonschema


def read_and_validate_json(json_file: Path) -> object:
    try:
        with open(json_file, "r") as file:
            return json.load(file)
    except json.decoder.JSONDecodeError as exception:
        print(
            f"`{json_file.name}` does not contain valid JSON: {exception}",
            file=sys.stderr,
        )
        sys.exit(1)


def validate_schema(json_path: Path, validator: jsonschema.Draft7Validator):
    json_contents = read_and_validate_json(model_generator)
    errors = sorted(validator.iter_errors(json_contents), key=lambda e: e.path)
    if len(errors) > 0:
        print(f"Invalid format in file {json_path}")
        for idx, error in enumerate(errors):
            instance = json.dumps(error.instance, indent=2)
            print(textwrap.indent(f"error: {error.path}: {error.message}:", "  "))
            new_line = "\n" if (idx < len(errors) - 1) else ""
            print(textwrap.indent(f"{instance}{new_line}", "    "))
        print("\n\n\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Validate the format of the given JSON model generator"
    )
    parser.add_argument(
        "--model-schema",
        type=str,
        help="The schema that describes the valid format of a JSON model generator.",
        default="fbandroid/native/mariana-trench/shim/resources/model_generator_schema.json",
    )
    parser.add_argument(
        "--model-generator-directory",
        type=str,
        help="The directory that contains the JSON model generators that need to be checked for validity.",
        default="fbandroid/native/mariana-trench/shim/resources/model_generators/",
    )

    arguments: argparse.Namespace = parser.parse_args()

    schema = read_and_validate_json(Path(arguments.model_schema))
    validator = jsonschema.Draft7Validator(schema)

    directory = Path(arguments.model_generator_directory)
    for model_generator in directory.glob("**/*"):
        if model_generator.is_file():
            validate_schema(model_generator.resolve(), validator)
