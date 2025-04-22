---
id: getting-started
title: Getting Started
sidebar_label: Getting Started
---
import useBaseUrl from '@docusaurus/useBaseUrl';

This guide will walk you through setting up Mariana Trench on your machine and get you to find your first remote code execution vulnerability in a small sample app.

## Prerequisites
Mariana Trench requires a recent version of [Python](https://www.python.org/downloads/). On MacOS you can get a current version through [homebrew](https://brew.sh/):

```shell
$ brew install python3
```

On a Debian flavored Linux (Ubuntu, Mint, Debian), you can use `apt-get`:

```shell
$ sudo apt-get install python3 python3-pip python3-venv
```

This guide also assumes you have the [Android SDK](https://developer.android.com/studio) installed and an environment variable `$ANDROID_SDK` pointed to the location of the SDK.

For the rest of this guide, we assume that you are working inside of a [virtual environment](https://docs.python.org/3/tutorial/venv.html). You can set this up with

```shell
$ python3 -m venv ~/.venvs/mariana-trench
$ source ~/.venvs/mariana-trench/bin/activate
(mariana-trench)$
```

The name of the virtual environment in front of your shell prompt indicates that the virtual environment is active.

## Installing Mariana Trench
Inside your virtual environment installing Mariana Trench is as easy as running

```shell
(mariana-trench)$ pip install mariana-trench
```

## Running Mariana Trench
We'll use a small app that is part of our documentation. You can get it by running

```shell
(mariana-trench)$ git clone https://github.com/facebook/mariana-trench
(mariana-trench)$ cd mariana-trench/documentation/sample-app
```

We are now ready to run the analysis

```shell
(mariana-trench)$ mariana-trench \
  --system-jar-configuration-path=$ANDROID_SDK/platforms/android-30/android.jar \
  --apk-path=app/build/outputs/apk/debug/app-debug.apk \
  --source-root-directory=app/src/main/java
# ...
INFO Analyzed 68886 models in 4.04s. Found 4 issues!
# ...
```

The analysis has found 4 issues in our sample app. The output of the analysis is a set of specifications for each method of the application.

## Post Processing
The specifications themselves are not meant to be read by humans. We need an additional processing step in order to make the results more presentable. We do this with [SAPP](https://github.com/facebook/sapp) PyPi installed for us:

```shell
(mariana-trench)$ sapp --tool=mariana-trench analyze .
(mariana-trench)$ sapp --database-name=sapp.db server --source-directory=app/src/main/java
# ...
2021-05-12 12:27:22,867 [INFO]  * Running on http://localhost:13337/ (Press CTRL+C to quit)
```

The last line of the output tells us that SAPP started a local webserver that lets us look at the results. Open the link and you will see the 4 issues found by the analysis.

## Exploring Results
Let's focus on the remote code execution issue found in the sample app. You can identify it by its issue code `1` (for all remote code executions) and the callable `void MainActivity.onCreate(Bundle)`. With only 4 issues to see it's easy to identify the issue manually but once more rules run, the filter functionality at the top right of the page comes in handy.

<img alt="Single Issue Display" src={useBaseUrl('img/issue.png')} />

The issue tells you that Mariana Trench found a remote code execution in `MainActivity.onCreate` where the data is coming from `Activity.getIntent` one call away, and flows into the constructor of `ProcessBuilder` 3 calls away. Click on "Traces" in the top right corner of the issue to see an example trace.

The trace surfaced by Mariana Trench consists of three parts.

The *source trace* represents where the data is coming from. In our example, the trace is very short: `Activity.getIntent` is called in `MainActivity.onCreate` directly.
<img alt="Trace Source" src={useBaseUrl('img/trace_source.png')} />

The *trace root* represents where the source trace meets the sink trace. In our example this is the `MainActivity.onCreate` method.
<img alt="Trace Root" src={useBaseUrl('img/trace_root.png')} />

The final part of the trace is the *sink trace*: This is where the data from the source flows down into a sink. In our example from `onCreate`, to `onClick`, to `execute`, and finally into the constructor of `ProcessBuilder`.
<img alt="Trace Source" src={useBaseUrl('img/trace_sink.png')} />

## Configuring Mariana Trench
You might be asking yourself, "how does the tool know what is user controlled data, and what is a sink?". This guide is meant to quickly get you started on a small app. We did not cover how to configure Mariana Trench. You can read more about that in the [Configuration section](./configuration.md).
