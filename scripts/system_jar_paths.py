#!/usr/bin/env python3
# (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# pyre-strict

import argparse
import functools
import json
import logging
import os.path
import subprocess
import sys
import typing


LOG: logging.Logger = logging.getLogger("root")

ANDROID_SDK_JARS: typing.List[str] = [
    "/opt/android/sdk_D23134735/platforms/android-29/android.jar",
    "/opt/android/sdk_D23134735/platforms/android-29/optional/org.apache.http.legacy.jar",
]

EXCLUDE_JARS: typing.List[str] = [
    # The following jars fail to load.
    "fbandroid/third-party/android/databinding/4.0.0/compiler/compiler-4.0.0.jar",
    "fbandroid/third-party/android/databinding/4.0.0/compilerCommon/compilerCommon-4.0.0.jar",
    "fbandroid/third-party/auto-value-gson/auto-value-gson-0.8.0.jar",
    "fbandroid/third-party/auto-value/auto-common-0.10.jar",
    "fbandroid/third-party/auto-value/auto-value-1.6.3.jar",
    "fbandroid/third-party/java/asm/7.0/asm-7.0.jar",
    "fbandroid/third-party/java/eclipse-collections/eclipse-collections-8.2.0.jar",
    "fbandroid/third-party/java/eclipse-collections/eclipse-collections-api-8.2.0.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-core-3.14.5.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-core-generators-3.14.5.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-core-metamodel-generator-3.14.5.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-symbol-solver-core-3.14.5.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-symbol-solver-logic-3.14.5.jar",
    "fbandroid/third-party/java/java_ast_parser/javaparser-symbol-solver-model-3.14.5.jar",
    "fbandroid/third-party/java/jaxb/istack-commons.jar",
    "fbandroid/third-party/java/jaxb/jaxb-api.jar",
    "fbandroid/third-party/java/jaxb/jaxb-impl.jar",
    "fbandroid/third-party/java/okhttp/okhttp-3.14.2.jar",
    "fbandroid/third-party/org/jetbrains/kotlinx-metadata-jvm/0.0.5/kotlinx-metadata-jvm-0.0.5.jar",
    "fbandroid/third-party/webrtc/sdk/java/audio_device_java.jar",
    "fbandroid/third-party/webrtc/sdk/java/base_java.jar",
    "fbandroid/third-party/webrtc/sdk/java/libjingle_peerconnection_java.jar",
    "third-party/infer-javac-tools/infer-javac.jar",
    "third-party/kotlin/1.3.70/lib/kotlin-compiler.jar",
    "third-party/kotlin/1.3.70/lib/trove4j.jar",
    "xplat/third-party/j2objc/lib/jre_emul.jar",
    "xplat/third-party/javac-tools/tools_1.8.0_60.jar",
    # The following jars are conflicting with android.jar
    "fbandroid/third-party/android/androidx/annotation/annotation/1.2.0-alpha01/annotation-1.2.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/arch/core/core-common/2.2.0-alpha01/core-common-2.2.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/collection/collection-ktx/1.2.0-alpha01/collection-ktx-1.2.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/collection/collection/1.2.0-alpha01/collection-1.2.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/concurrent/concurrent-futures/1.1.0-alpha01/concurrent-futures-1.1.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/constraintlayout/constraintlayout-solver/1.1.3/constraintlayout-solver-1.1.3.jar",
    "fbandroid/third-party/android/androidx/lifecycle/lifecycle-common-java8/2.3.0-alpha01/lifecycle-common-java8-2.3.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/lifecycle/lifecycle-common/2.3.0-alpha01/lifecycle-common-2.3.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/paging/paging-common-ktx/2.1.2/paging-common-ktx-2.1.2.jar",
    "fbandroid/third-party/android/androidx/paging/paging-common/2.1.2/paging-common-2.1.2.jar",
    "fbandroid/third-party/android/androidx/room/room-common/2.3.0-alpha01/room-common-2.3.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/room/room-compiler/2.3.0-alpha01/room-compiler-2.3.0-alpha01.jar",
    "fbandroid/third-party/android/androidx/room/room-migration/2.3.0-alpha01/room-migration-2.3.0-alpha01.jar",
    # The following jars have conflicting class hierarchies.
    # This generates errors when building the override graph.
    "fbandroid/third-party/java/okhttp/okhttp-3.11.0.jar",
    "fbandroid/third-party/java/okhttp/okhttp-3.12.2-without-public-suffix-list.jar",
]


def phpunserialize(value: str) -> typing.Any:
    # We cannot use the 'phpunserialize' package, so we use a dirty hack.
    process = subprocess.Popen(
        [
            "php",
            "-r",
            'echo json_encode(unserialize(file_get_contents("php://stdin")));',
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = process.communicate(value.encode("utf-8"))
    assert process.returncode == 0
    assert stderr == b""
    return json.loads(stdout)


def get_sitevar(name: str) -> typing.Any:
    LOG.info(f"Fetching sitevar `{name}`")

    # We cannot use libfb, so we use a dirty hack.
    output = subprocess.check_output(
        ["configeratorc", "getConfig", f"sitevars/{name}.sitevar"]
    )
    output = json.loads(output)
    return phpunserialize(output["contents"])


@functools.lru_cache(maxsize=None)
def buck_query_prebuilt_jars(target: str) -> typing.Set[str]:
    LOG.info(f"Fetching prebuilt jars for target `{target}`")

    if target.startswith("fbcode"):
        return set()

    try:
        output = subprocess.check_output(
            [
                "buck",
                "query",
                f"kind('prebuilt_jar', deps('{target}'))",
                "--output-attribute",
                "binary_jar",
            ]
        )
    except subprocess.CalledProcessError as error:
        LOG.warning(f"Command failed with return code {error.returncode}")
        return set()

    output = json.loads(output)
    results: typing.Set[str] = set()

    for dependency, attributes in output.items():
        assert dependency.startswith("//")
        target_path, target_name = dependency.split(":", 1)
        binary_jar = attributes["binary_jar"]

        if binary_jar.startswith(":"):
            # Reference to another buck target
            results.update(buck_query_prebuilt_jars(f"{target_path}{binary_jar}"))
        elif binary_jar.startswith("//"):
            LOG.error(f"Unexpected binary_jar attribute: `{binary_jar}`")
        else:
            results.add(os.path.normpath(os.path.join(target_path[2:], binary_jar)))

    return results


def main() -> None:
    logging.basicConfig(level=logging.INFO)

    description = """
        Generate a json file on the standard output containing the paths to all
        the prebuilt jars used at Facebook.
    """
    parser = argparse.ArgumentParser(description=description)
    parser.parse_args()

    system_jars: typing.Set[str] = set()
    for application in get_sitevar("MARIANA_TRENCH_ANALYSIS_CONFIGURATION"):
        buck_target = application["buck_target"]
        if buck_target:
            system_jars.update(buck_query_prebuilt_jars(buck_target))

    system_jars.difference_update(EXCLUDE_JARS)

    system_jars: typing.List[str] = sorted(system_jars)
    system_jars = ANDROID_SDK_JARS + system_jars
    json.dump(system_jars, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
