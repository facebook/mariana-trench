---
id: overview
title: Overview
sidebar_label: Overview
---
import {OssOnly, FbInternalOnly} from 'internaldocs-fb-helpers';
import FbOverview from './fb/overview.md';

## What is Mariana Trench

**Mariana Trench** is a security focused static analysis platform targeting Android. The tool provides an extensible global taint analysis similar to pre-existing tools like [Pysa](https://pyre-check.org/docs/pysa-basics) for Python. The tool leverages existing static analysis infrastructure (e.g, [SPARTA](https://github.com/facebookincubator/SPARTA)) built on top of [Redex](https://github.com/facebook/redex).

By default Mariana Trench analyzes [dalvik bytecode](https://source.android.com/devices/tech/dalvik/dalvik-bytecode) and can work with or without access to the source code.

## Background

### Sources and Sinks

Under the context of taint analysis [1], "sources" usually mean sensitive data that originates from users. For example, sources can be users' passwords or locations. "Sinks" usually mean functions or methods that use data that "flows" from sources, where the term "flow" is generally defined under the context of "information flow" [2].
> An operation, or series of operations, that uses the value of some object, say x, to derive a value for another, say y, causes a flow from x to y

As an example, sinks can be a logging API that writes data into a log file.

### What does Mariana Trench do?

A flow from sources to sinks indicate that for example user passwords may get logged into a file, which is not desirable and is called as an **"issue"** under the context of Mariana Trench. Mariana Trench is designed to automatically discover such issues.

## Usage

The usage of Mariana Trench can be summarized in three steps:

<OssOnly>

1. Specify customized "sources" and "sinks". (See [Models](./models.md))
2. Run Mariana Trench on an arbitrary Java repository (with the sources and sinks specified in Step 1), whether it be a repository for an Android application project or for a vanilla (or plain old) Java project.
3. View the analysis results from a web browser. (For steps 2 and 3 see [Getting Started](./getting_started.md))

</OssOnly>
<FbInternalOnly> <FbOverview/> </FbInternalOnly>

## References

1. [Tripp, Omer, et al. "TAJ: effective taint analysis of web applications." ACM Sigplan Notices 44.6 (2009): 87-97.](https://dl.acm.org/doi/10.1145/1542476.1542486)
2. [Denning, Dorothy E., and Peter J. Denning. "Certification of programs for secure information flow." Communications of the ACM 20.7 (1977): 504-513.](https://dl.acm.org/doi/10.1145/359636.359712)
