#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import logging
import subprocess

LOG: logging.Logger = logging.getLogger(__name__)


def get_job_definition(project_names, input_tenant):
    current_hash = subprocess.check_output(
        ["hg", "whereami"], universal_newlines=True
    ).strip()
    user = subprocess.check_output(["whoami"], universal_newlines=True).strip()
    tenant = input_tenant or "default-tenant"

    return {
        "command": "SandcastleMarianaTrenchLocalChangesAnalysis",
        "args": {
            "project_names": project_names,
            "hash": current_hash,
            "analysis_binary": "master",
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
        "project_names",
        metavar="project_name",
        type=str,
        nargs="+",
        help="The name of an apk or java target for the command to run on.\n"
        + "Must be one of the projects in the sitevar SV_MARIANA_TRENCH_ANALYSIS_CONFIGURATION \n",
    )
    parser.add_argument(
        "-t",
        "--tenant",
        type=str,
        help="If specified, the job will run on the specified Sandcastle tenant. Otherwise, it will run on default-tenant",
    )

    arguments = parser.parse_args()
    job = get_job_definition(arguments.project_names, arguments.tenant)
    spec = json.dumps(job, indent=2)

    results = json.loads(
        subprocess.check_output(
            ["scutil", "create", "--view", "json", "--bundle"],
            input=spec,
            universal_newlines=True,
        )
    )

    LOG.info("Sandcastle URL: " + results["url"])
