---
id: android-lifecycles
title: Android/API Lifcycles
sidebar_label: Android/API Lifcycles
---

import {OssOnly, FbInternalOnly} from 'docusaurus-plugin-internaldocs-fb/internal';
import FbAndroidLifecycles from './fb/android_lifecycles.md';

## Background
Framework classes often provide overridable methods that subclasses can override. These methods are frequently executed in some sequence. The most direct example of this would be the [Activity life-cycle](https://developer.android.com/guide/components/activities/activity-lifecycle). Sub-classes implement methods like onCreate(), onStart(), onResume(), etc. which is internally chained up in the base class.

The analysis may see this chain if the code for the base class is available. However, because the base class can be overridden by many different children, the analysis cannot easily differentiate between flows `Child1.onCreate() -> Child1.onStart()` and `Child1.onCreate() -> Child2.onStart()`. The latter could result in a false positives. There could also be too many children and which causes the analysis to drop taint and fail to find any flow.

To get around this, we allow users to define life-cycles for these framework classes.

## Life-cycle Configuration

<FbInternalOnly> <FbAndroidLifecycles /> </FbInternalOnly>

<OssOnly>

The default life-cycles are defined in [configuration/lifecycles.json](https://github.com/facebook/mariana-trench/blob/main/configuration/lifecycles.json).

</OssOnly>


## Life-cycle Definition
Life-cycles are defined in a JSON file and passed into the analysis via the `--lifecycles-paths` option. Here is a sample definition.

```
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
      }
    ]
  },
```

Most of the fields should be self-explanatory, with just a few additional notes:
* The analysis internally creates artificial methods with signature `<child of base_class_name>.<method_name>(args for callees)` that calls the "callees" in the defined sequence.
* "method_name" must be unique across all life-cycles. Children could extend multiple base classes. If their respective life-cycle definitions share a "method_name", there will be a conflict.
* Only children at the leaves of the class hierarchy will have the artificial method created, so taint flow will only be detected in these classes.
