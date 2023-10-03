---
id: debugging-fp-fns
title: Debugging False Positives/False Negatives
sidebar_label: Debugging False Positives/False Negatives
---
import {OssOnly, FbInternalOnly} from 'docusaurus-plugin-internaldocs-fb/internal';
import FbDebugging from './fb/debugging_fp_fns.md';

This document is mainly intended for software engineers, to help them debug false positives and false negatives.

<OssOnly>
## Setup

First, you need to run the analysis on your computer. This will create `model@XXX.json` files in the current directory, containing the results of the analysis.
</OssOnly>
<FbInternalOnly> <FbDebugging/> </FbInternalOnly>

## Investigate the output models

Now, your objective is to understand in which method we lost the flow (false negative) or introduced the invalid flow (false positive). You will need to look into the output models for that. I recommend to use the `explore_models.py` bento script.

Run the following command in the directory containing the output model files (i.e, `model@XXX.json`):
<OssOnly>
```
python3 -i mariana_trench_repository/scripts/explore_models.py
```
</OssOnly>
<FbInternalOnly>
```
bento console -i --file ~/fbsource/fbandroid/native/mariana-trench/scripts/explore_models.py
```
</FbInternalOnly>

This provides you with a few helper functions:
```
  index('.')                    Index all available models in the given directory.
  method_containing('Foo;.bar') Find all methods containing the given string.
  method_matching('Foo.*')      Find all methods matching the given regular expression.
  get_model('Foo;.bar')         Get the model for the given method.
  print_model('Foo;.bar')       Pretty print the model for the given method.
```
Use `index` to index all models first:
```
In [1]: index()
```
Now you can search for methods with `method_containing` and print their models with `print_model`.
You probably want to look at the first or last frame of the trace, to see if the source or sink is present. Then, you will want to follow the frames until you find the problematic method.

### Example

Let's suppose I am investigating a false negative, I want to find in which method we are losing the flow. I could start looking at the last frame, i.e the sink:
```
In [2]: method_containing('Landroid/content/Context;.sendOrderedBroadcast')
Out[2]:
['Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;)V',
 'Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V',
 'Landroid/content/Context;.sendOrderedBroadcastAsUser:(Landroid/content/Intent;Landroid/os/UserHandle;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V']

In [3]: print_model('Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V')
{
  "method": "Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V",
  "modes": [
    "skip-analysis",
    "add-via-obscure-feature",
    "taint-in-taint-out",
    "taint-in-taint-this",
    "no-join-virtual-overrides"
  ],
  "position": {
    "path": "android/content/Context.java"
  },
  ...
  "sinks": [
    {
      "callee_port": "Leaf",
      "caller_port": "Argument(1)",
      "kind": "LaunchingComponent",
      "origins": [
        "Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V"
      ]
    }
  ]
}
```
As expected, the method has a sink on Argument(1), so we are good for now. Next, I want to check the previous frame, which calls `Context.sendOrderedBroadcast`:
```
In [2]: method_containing('ShortcutManagerCompat;.requestPinShortcut:')
Out[2]: ['Landroidx/core/content/pm/ShortcutManagerCompat;.requestPinShortcut:(Landroid/content/Context;Landroidx/core/content/pm/ShortcutInfoCompat;Landroid/content/IntentSender;)Z']

In [3]: print_model('Landroidx/core/content/pm/ShortcutManagerCompat;.requestPinShortcut:(Landroid/content/Context;Landroidx/core/content/pm/ShortcutInfoCompat;Landroid/content/IntentSender;)Z')
{
  "method": "Landroidx/core/content/pm/ShortcutManagerCompat;.requestPinShortcut:(Landroid/content/Context;Landroidx/core/content/pm/ShortcutInfoCompat;Landroid/content/IntentSender;)Z",
  "position": {
    "line": 112,
    "path": "androidx/core/content/pm/ShortcutManagerCompat.java"
  },
  ...
  "sinks": [
     ...
    {
      "always_features": [
        "via-obscure",
        "via-obscure-taint-in-taint-this",
        "via-intent-extra",
        "has-intent-extras"
      ],
      "call_position": {
        "line": 130,
        "path": "androidx/core/content/pm/ShortcutManagerCompat.java"
      },
      "callee": "Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V",
      "callee_port": "Argument(1)",
      "caller_port": "Argument(1).mIntents",
      "distance": 1,
      "kind": "LaunchingComponent",
      "local_positions": [
        {
          "line": 121
        }
      ],
      "origins": [
        "Landroid/content/Context;.sendOrderedBroadcast:(Landroid/content/Intent;Ljava/lang/String;Landroid/content/BroadcastReceiver;Landroid/os/Handler;ILjava/lang/String;Landroid/os/Bundle;)V"
      ]
    }
  ]
}
```
I can see the frame from `ShortcutManagerCompat.requestPinShortcut` on `Argument(1).mIntents` to `Context.sendOrderedBroadcast` on `Argument(1)`. I can keep following frames until I find the method that misses a source or sink.

For frames from the source to the root callable, I should look at `generations`, and for frames from the root callable to the sink, I should look at `sinks`. On the root callable, I should look at `issues`.

## Investigating the transfer function

Once you know in which method you are losing the flow or introducing an invalid flow, you will need to run the analysis with logging enabled for that method, using:

<OssOnly>
```
mariana-trench \
  --apk-path='your-apk' \
  --log-method='method-name'
```
</OssOnly>
<FbInternalOnly>
```
buck run //fbandroid/native/mariana-trench/shim:shim -- \
  --apk-path='your-apk' \
  --log-method='method-name'
```
</FbInternalOnly>

This will log everything the transfer function does in that method, which might be a lot of logs. You can pipe this into a file or into `less`. Using logs, you should be able to see in which instruction you are losing the taint. Remember, the analysis computes a fixpoint, so the method will be analyzed multiple times. You should look at the last time it was analyzed (i.e, end of the logs).

Happy debugging!
