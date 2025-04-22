---
id: customize-sources-and-sinks
title: Customize Sources and Sinks
sidebar_label: Customize Sources and Sinks
---

import {OssOnly, FbInternalOnly} from 'docusaurus-plugin-internaldocs-fb/internal'; import FbCustomizeSourcesAndSinks from './fb/customize_sources_and_sinks.md';

This page provides a high-level overview of the steps needed to update or create new sources and sinks.

## Overview

Under the context of Mariana Trench, we talk about sources and sinks in terms of methods (or, rarely, fields). For example, we may say that the return value of a method is a source (or a sink). We may also say that the 2nd parameter of a method is a source (or a sink). Such description of a method is called a **"model"**. See [Models & Model Generators](./models.md) for more details about models and writing them.

<FbInternalOnly> <FbCustomizeSourcesAndSinks/> </FbInternalOnly>

<OssOnly>

To define sources or sinks that are not contained in the default set of [sources](https://github.com/facebook/mariana-trench/tree/main/configuration/model-generators/sources) and [sinks](https://github.com/facebook/mariana-trench/tree/main/configuration/model-generators/sinks), a user needs to:

1. Write one or more JSON files that respect our [model generator Domain Specific Language (DSL)](./models.md), which express how to generate models from methods and are hence called **"model generators"**.

   - For example, a model generator may say that, for all methods (that will be analyzed by Mariana Trench) whose name is `onActivityResult`, specify their 2nd parameter as a source.

   ```json
   {
     "model_generators": [
       {
         "find": "methods",
         "where": [
           {
             "constraint": "name",
             "pattern": "onActivityResult"
           }
         ],
         "model": {
           "sources": [
             {
               "kind": "TestSensitiveUserInput",
               "port": "Argument(2)"
             }
           ]
         }
       }
     ]
   }
   ```

2. Instruct Mariana Trench to read from your model generator, so that Mariana Trench will generate models at runtime.
   - Intuitively, the models generated (by interpreting model generators) express sources and sinks for each method **before** running Mariana Trench. Based on such models, Mariana Trench will automatically infer **new** models for each method at runtime.
   - To instruct Mariana Trench to read from customized JSON model generators, add your json model generator [here](https://github.com/facebook/mariana-trench/tree/main/configuration/model-generators).
   - Add the model generator name (i.e, the file name) in the [JSON configuration file](https://github.com/facebook/mariana-trench/blob/main/configuration/default_generator_config.json).
3. Update **"rules"** if necessary.
   - Background: Mariana Trench categorizes sources and sinks into different **"kinds"**, which are string-typed. For example, a source may have a kind of`JavascriptInterfaceUserInput`. A sink may have a kind of `Logging`. Mariana Trench only finds data flow **from sources of a particular kind to sinks of another paritcular kind**, which are called **"rules"**. See [Rules](./rules.md) for writing them.
   - To specify kinds that are not mentioned in the default set of rules or to specify rules that are different than the default rules, you need to specify a new rule in file [`rules.json`](https://github.com/facebook/mariana-trench/blob/main/configuration/rules.json), in order to instruct Mariana Trench to find data flow that matches the new rule.
   - For example, to catch flows from `TestSensitiveUserInput` in the example above and the sink kind `Logging`, you can add the following rule to the default [`rules.json`](https://github.com/facebook/mariana-trench/blob/main/configuration/rules.json):
   ```json
   {
    "name": "TestRule",
    "code": 18,
    "description": "A test rule",
    "sources": [
      "TestSensitiveUserInput"
    ],
    "sinks": [
      "Logging"
    ]
   }
   ```

</OssOnly>
