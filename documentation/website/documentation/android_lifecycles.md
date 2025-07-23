---
id: android-lifecycles
title: Android/API Lifecycles
sidebar_label: Android/API Lifecycles
---

import {OssOnly, FbInternalOnly} from 'docusaurus-plugin-internaldocs-fb/internal';
import FbAndroidLifecycles from './fb/android_lifecycles.md';

## Background
Framework classes often provide overridable methods that subclasses can override. These methods are frequently executed in some sequence. The most direct example of this would be the [Activity lifecycle](https://developer.android.com/guide/components/activities/activity-lifecycle). Sub-classes implement methods like onCreate(), onStart(), onResume(), etc. which is internally chained up in the base class.

The analysis may see this chain if the code for the base class is available. However, because the base class can be overridden by many different children, the analysis cannot easily differentiate between flows `Child1.onCreate() -> Child1.onStart()` and `Child1.onCreate() -> Child2.onStart()`. The latter could result in a false positives. There could also be too many children and which causes the analysis to drop taint and fail to find any flow.

To get around this, we allow users to define lifecycles for these framework classes.

## Lifecycle Configuration

<FbInternalOnly> <FbAndroidLifecycles /> </FbInternalOnly>

<OssOnly>

The default lifecycles are defined in [configuration/lifecycles.json](https://github.com/facebook/mariana-trench/blob/main/configuration/lifecycles.json).

</OssOnly>


## Lifecycle Definition
Lifecycles are defined in a JSON file and passed into the analysis via the `--lifecycles-paths` option. The definition contains three basic components, a `"base_class_name"`, a `"method_name"`, and set of overridable methods (callees) with their name and prototypes. Mariana Trench internally creates artificial methods with signature `<child of base_class_name>.<method_name>(args for callees)` that invokes the callees.

A few notes on lifecycle definitions in general:
* The `"method_name"` must be unique across all lifecycles. Children could extend multiple base classes. If their respective lifecycle definitions share a `"method_name"`, there will be a conflict.
* Only children at the leaves of the class hierarchy will have the artificial method created, so taint flow will only be detected in these classes.
* All callee methods are expected to be defined in the base class and is validated to detect invalid configurations. However, if a method only exists in some derived classes down the class hierarchy, the callee should contain a `"defined_in_derived_class"` field. This field specifies the derived class where the method is defined, indicating Mariana Trench that the method does not exist in the base class, but in some classes extending it. This allows the method to be included in the generated lifecycle method.

The way the method is constructed (e.g., the order callees are invoked) depends on the kind of lifecycle definitions. Mariana Trench currently supports two kinds of lifecycle definitions: linear lifecycles and lifecycle graphs.

### Linear Lifecycles
In linear lifecycle definitions, the callees are defined in the `"callees"` array. The analysis generates the artificial method that invokes the callees in the order defined in the array (thus the name "linear"). Here is a sample definition:

```json
  {
    "base_class_name": "Landroidx/fragment/app/FragmentActivity;",
    "method_name": "activity_lifecycle_wrapper",
    "callees": [
      {
        "method_name": "onCreate",
        "return_type": "V",
        "argument_types": [
          "Landroid/os/Bundle;"
        ]
      },
      {
        "method_name": "onStart",
        "return_type": "V",
        "argument_types": []
      },
      {
        "method_name": "onTest",
        "return_type": "V",
        "argument_types": [
          "Ljava/lang/Object;"
        ],
        "defined_in_derived_class": "Lcom/facebook/marianatrench/integrationtests/FragmentOneActivity;"
      }
    ]
  }
```

### Lifecycle Graphs

One issue with the linear lifecycle definition is that it does not model more complex lifecycle transitions in real-world settings. For example, when the user navigates away from the activity, spends some time in other activity, and then navigates back, the `onStart` method would be called again. This state transition loop cannot be captured by linear lifecycle definitions.

The lifecycle graph aims to solve this issue by allowing users to specify the transition relationships between the lifecycle states together with their corresponding methods as graphs. A sample lifecycle graph definition looks like this:

```json
  {
    "base_class_name": "Landroid/app/Activity;",
    "method_name": "activity_lifecycle_wrapper",
    "control_flow_graph": {
      "entry": {
        "instructions": [
          {
            "method_name": "<init>",
            "return_type": "V",
            "argument_types": []
          }
        ],
        "successors": [
          "onCreate"
        ]
      },
      "onCreate": {
        "instructions": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": [
              "Landroid/os/Bundle;"
            ]
          }
        ],
        "successors": [
          "exit"
        ]
      },
      "exit": {
        "instructions": [
          {
            "method_name": "onStart",
            "return_type": "V",
            "argument_types": []
          }
        ],
        "successors": [
          "onCreate"
        ]
      }
    }
  }
```

Several important points to note for the definition:
* By defining the `"control_flow_graph"` field instead of `"callees"` in linear lifecycles, Mariana Trench will automatically treat this definition as a lifecycle graph.
* Each object in the `"control_flow_graph"` is a graph node that represents the lifecycle state. For each node, the `"instructions"` array contains the callees that should be invoked in the current state. The `"successors"` array contains keys of the states that the current state could transit to (e.g. `"onCreate"` would transit to `"onStart"`).
* There are two special node names, `"entry"` and `"exit"`, which corresponds to the entry and exit nodes of the graph. The entry node is required. The exit node is optional, but any node that is not the exit node must have at least one successor.

#### Method Generation for Lifecycle Graphs

Internally, Mariana Trench will create one basic block for each graph node. The basic block will contain invocations to the methods associated with the node, and end with a `switch` that connects it to each of its successors. As an example, the above configuration may generate the following code for a class `MainActivity` that extends `android.app.Activity`:

```
Block 0 (entry):
  IOPCODE_LOAD_PARAM_OBJECT v0
  IOPCODE_LOAD_PARAM_OBJECT v1
  Successors: {3}
Block 1:
  INVOKE_VIRTUAL v0, v1, Lcom/facebook/marianatrench/integrationtests/MainActivity;.onCreate:(Landroid/os/Bundle;)V
  SWITCH v2
  Successors: {2}
Block 2:
  INVOKE_VIRTUAL v0, Lcom/facebook/marianatrench/integrationtests/MainActivity;.onStart:()V
  SWITCH v3
  Successors: {4, 1}
Block 3:
  INVOKE_VIRTUAL v0, Lcom/facebook/marianatrench/integrationtests/MainActivity;.<init>:()V
  SWITCH v4
  Successors: {1}
Block 4:
  RETURN_VOID
```


Here, the block 1, 2, and 3 maps to the `"onCreate"`, `"exit"`, and `"entry"` nodes, respectively. Block 0 is the real entry block of the method, which loads all parameters and jumps into the block associated with the `"entry"` node (i.e., block 3). Block 4 is the real exit node of the method, which contains the return instruction. The block associated with the `"exit"` node (i.e., block 2) will have a branch to this real exit block.
