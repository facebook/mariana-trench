---
id: rules
title: Rules
sidebar_label: Rules
---

import MultiSourceSinkRule from './fb/multi_source_sink_rules.md';

<!-- Careful saving this file, weird formatting happens to json blocks -->

A rule describes flows that we want to catch (e.g, user input flowing into command execution).
A rule is made of a set of source [kinds](./models.md#kinds), a set of sink kinds, a name, a code, and a description.

Here is an example of a rule in JSON:
```json
{
  "name": "User input flows into code execution (RCE)",
  "code": 1,
  "description": "Values from user-controlled source may eventually flow into code execution",
  "sources": [
    "UserCamera",
    "UserInput",
  ],
  "sinks": [
    "CodeAsyncJob",
    "CodeExecution",
  ]
}
```

For guidance on modeling sources and sinks, see the next section, [Models and Model Generators](./models.md).

Rules used by Mariana Trench can be specified with the `--rules-paths` argument. The default set of rules that run can be found in [configuration/rules.json](https://github.com/facebook/mariana-trench/blob/main/configuration/rules.json).

<MultiSourceSinkRule />
