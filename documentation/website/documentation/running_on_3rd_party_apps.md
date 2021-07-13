---
id: running-on-3rd-party-apps
title: Running on 3rd-Party Apps
sidebar_label: Running on 3rd-Party Apps
---

This guide will walk you through the process to run Mariana Trench on 3rd party apps. We often use Mariana Trench to analyze our own applications where we have the source code readily available but Mariana Trench runs on byte-code and does not necessarily need access to source code.

This guide assumes you are familiar with running Mariana Trench. If you are not, read the [Getting Started](getting-started) guide first.

## Getting the APK
You need an APK first in order to analyze it. You can get this directly from your phone or use a site such as [apkmirror](https://www.apkmirror.com/). The rest of this guide assumes that you have downloaded `example.apk`.

## Decompile the APK
While Mariana Trench does not require the source for the analysis, the results are hard to process without any source reference.

```shell
$ brew install jadx
$ jadx -e example.apk
```

The latter will attempt to decompile the apk and store the results in a `example/` directory.

## Run Mariana Trench
We are now ready to run the analysis:

```shell
$ mariana-trench --apk-path=example.apk --source-root-directory=example/src/main/java/
```

Once the analysis terminates we can do our usual post-processing and look at the results:

```shell
$ sapp --tool=mariana-trench analyze .
$ sapp --database-name=sapp.db server --source-directory=example/src/main/java/
```

The major limitation of this current approach is that the location information Mariana Trench uses is coming from bytecode instructions, not from the source that we decompiled. This means going through the traces is extra tedious. We welcome suggestions or pull-requests to improve this process.
