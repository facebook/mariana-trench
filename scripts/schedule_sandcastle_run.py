#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import logging
import os
import subprocess
import sys
import traceback
from typing import Any, Dict, Iterable, Optional

LOG: logging.Logger = logging.getLogger(__name__)


class ArgumentError(Exception):
    pass


def save_to_everpaste(path: str) -> str:
    return subprocess.check_output(
        [
            "clowder",
            "put",
            "--fbtype=EVERSTORE_EVERPASTEBLOB",
            "--extension=apk",
            path,
        ],
        universal_newlines=True,
    ).strip()


def sanitize_name(name: str) -> str:
    """
    The name here is used for naming a sandcastle job.
    The allowed characters are lowercase alphabets, digits, hyphens and underscores.
    """
    sanitized_name = ""
    for character in name:
        if character.isalnum() or character == "-" or character == "_":
            sanitized_name += character
    return sanitized_name.lower()[:200]


def get_names_to_everstore_handles(
    custom_projects_map: Iterable[str],
) -> Dict[str, str]:
    processed_paths = {}
    for pair in custom_projects_map:
        split_pair = pair.split("=", 1)
        path = split_pair[1]
        if not os.path.exists(path):
            raise ArgumentError(f"The provided path {path} doesn't exist")
        if len(path) < 4 or ".apk" != path[-4:]:
            raise ArgumentError(f"The provided path {path} does not point to an apk")
        name = sanitize_name(split_pair[0].strip())
        if name in processed_paths.keys():
            raise ArgumentError(
                f"Duplicate santized names: {name}. Please provide unique names consisting of lowercase alphabets, digits, underscores and hyphens for each apk."
            )
        processed_paths[name] = save_to_everpaste(path)
    return processed_paths


def get_job_definition(
    input_sitevar_projects: Optional[Iterable[str]],
    input_tenant: Optional[str],
    custom_projects_map: Optional[Iterable[str]],
) -> Dict[str, Any]:
    current_hash = subprocess.check_output(
        ["hg", "whereami"], universal_newlines=True
    ).strip()
    user = subprocess.check_output(["whoami"], universal_newlines=True).strip()
    tenant = input_tenant or "default-tenant"
    sitevar_projects = input_sitevar_projects or []
    everstore_handles = {}
    if custom_projects_map is not None:
        everstore_handles = get_names_to_everstore_handles(custom_projects_map)

    return {
        "command": "SandcastleMarianaTrenchLocalChangesAnalysis",
        "args": {
            "project_names": sitevar_projects,
            "hash": current_hash,
            "analysis_binary": "master",
            "custom_projects": everstore_handles,
        },
        "capabilities": {
            "type": "android",
            "vcs": "fb4a-fbsource",
            "tenant": tenant,
        },
        "timeToExpire": 86400,
        "priority": 0,
        "alias": "mariana-trench-test-local-changes",
        "description": f"Running Mariana Trench with {user}'s local changes",
        "user": user,
        "hash": current_hash,
        "oncall": "mariana-trench",
    }


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(
        description="Schedule a sandcastle job to run Mariana Trench analyses with local changes"
    )
    parser.add_argument(
        "-s",
        "--sitevar-projects",
        type=str,
        nargs="+",
        help="The name of an apk or java target for the analysis to run on."
        + "Must be one of the projects in the sitevar SV_MARIANA_TRENCH_ANALYSIS_CONFIGURATION \n",
    )
    parser.add_argument(
        "-c",
        "--custom-projects",
        metavar="KEY=VALUE",
        type=str,
        nargs="+",
        help="Pairs of a name and a path to a local apk to run the analysis on. The name will be used to name the individual sandcastle job analyzing that apk.",
    )
    parser.add_argument(
        "-t",
        "--tenant",
        type=str,
        help="If specified, the job will run on the specified Sandcastle tenant. Otherwise, it will run on default-tenant",
    )

    arguments = parser.parse_args()
    if arguments.sitevar_projects is None and arguments.custom_projects is None:
        LOG.critical(
            "Please provide either the names of sitevar projects or paths to custom apks for the analysis to run on"
        )
        sys.exit(1)

    try:
        job = get_job_definition(
            arguments.sitevar_projects, arguments.tenant, arguments.custom_projects
        )
    except ArgumentError as error:
        LOG.critical(error.args[0])
        sys.exit(1)
    except Exception:
        LOG.critical(f"Unexpected error:\n{traceback.format_exc()}")
        sys.exit(1)

    spec = json.dumps(job, indent=2)
    results = json.loads(
        subprocess.check_output(
            ["scutil", "create", "--view", "json", "--bundle"],
            input=spec,
            universal_newlines=True,
        )
    )
    LOG.info("Sandcastle URL: " + results["url"])
