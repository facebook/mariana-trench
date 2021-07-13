---
id: configuration
title: Configuration
sidebar_label: Configuration
---

Mariana Trench is highly configurable and we recommend that you invest time into adjusting the tool to your specific use cases. At Facebook, we have dedicated security engineers that will spend a significant amount of their time adding new rules and model generators to improve the analysis results.

This page will cover the more important, non-trivial configuration options. Note that you will spend most of your time configuring Mariana Trench writing model generators. These are covered in the [next section](models.md).


## Command Line Options

You can get a full set of options by running `mariana-trench --help`. The following is an abbreviated version of the output.
```shell
$ mariana-trench --help

Target arguments:
  --apk-path APK_PATH   The APK to analyze.

Output arguments:
  --output-directory OUTPUT_DIRECTORY
                        The directory to store results in.

Configuration arguments:
  --system-jar-configuration-path SYSTEM_JAR_CONFIGURATION_PATH
                        A JSON configuration file with a list of paths to the system jars.
  --rules-paths RULES_PATHS
                        A `;`-separated list of rules files and directories containing rules files.
  --repository-root-directory REPOSITORY_ROOT_DIRECTORY
                        The root of the repository. Resulting paths will be relative to this.
  --source-root-directory SOURCE_ROOT_DIRECTORY
                        The root where source files for the APK can be found.
  --model-generator-configuration-paths MODEL_GENERATOR_CONFIGURATION_PATHS
                        A `;`-separated list of paths specifying JSON configuration files. Each file is a list of paths to JSON model generators relative to the
                        configuration file or names of CPP model generators.
  --model-generator-search-paths MODEL_GENERATOR_SEARCH_PATHS
                        A `;`-separated list of paths where we look up JSON model generators.
  --maximum-source-sink-distance MAXIMUM_SOURCE_SINK_DISTANCE
                        Limits the distance of sources and sinks from a trace entry point.
  ```

#### `--apk-path`
Mariana Trench analyzes Dalvik bytecode. You provide it with the android app (APK) to analyze.

#### `--output-directory OUTPUT_DIRECTORY`
The output of the analysis is a file containing metadata about the particular run in JSON format as well as sharded files containing data flow specifications for every method in the APK. These files need to be processed by SAPP (see [Getting Started](getting_started.md)) after the analysis. The flag specifies where these files are saved.

#### `--system-jar-configuration-path SYSTEM_JAR_CONFIGURATION_PATH`
This path points to a json file containing a list of `.jar` files that the analysis should include in the analysis. It's important that this contains at least the `android.jar` on your system. This file is typically located in your android SDK distribution at `$ANDROID_SDK/platforms/android-30/android.jar`. Without the `android.jar`, Mariana Trench will not know about many methods from the standard library that might be important for your model generators.

#### `--rules-paths RULES_PATHS`
A `;` separated search path pointing to files and directories containing rules files. These files specify what taint flows Mariana Trench should look for. Check out the [`rules.json`](https://github.com/facebook/mariana-trench/blob/master/configuration/rules.json#L2-L13) that's provided by default. It specifies that we want to find flows from user controlled input (`ActivityUserInput`) into `CodeExecution` sinks and that this constitutes a remote code execution.

#### `--source-root-directory SOURCE_ROOT_DIRECTORY`
Mariana Trench will do a source indexing path before the analysis. This is because Dalvik/Java bytecode does not contain complete location information, only filenames (not paths) and line numbers. The index is later used to emit precise locations.

#### `--model-generator-configuration-paths MODEL_GENERATOR_CONFIGURATION_PATHS`
A `;` separated set of files containing the names of model generators to run. See [`default_generator_config.json`](https://github.com/facebook/mariana-trench/blob/master/configuration/default_generator_config.json) for an example.

#### `--model-generator-search-paths MODEL_GENERATOR_SEARCH_PATHS`
A `;` separated search path where Mariana Trench will try to find the model generators specified in the generator configuration.

#### `--maximum-source-sink-distance MAXIMUM_SOURCE_SINK_DISTANCE`
For performance reasons it can be useful to limit the maximum length of a trace Mariana Trench tries to find (note that longer traces also tend to be harder to interpret). Due to the modular nature of the analysis the value specified here limits the maximum length from the trace root to the source, and from the trace root to the sink. This means found traces can have length of `2 x MAXIMUM_SOURCE_SINK_DISTANCE`.
