---
id: feature-descriptions
title: Feature Glossary
sidebar_label: Feature Glossary
---

As explained in the [features section of the models wiki](../models/#features), a feature can be used to tag a flow and help filtering issues. A feature describes a property of a flow. A feature can be any arbitrary string. A feature that's prefixed with `always-` signals that every path in the issue has that feature associated with it, while lacking that prefix means that at least one path, but not all paths, contains that feature.

This page will cover the purpose of the pre-configured features to help you understand how you can use them best.

## Pre-configured features

- via-caller-exported
  - This feature is applied when the root callable is directly or indirectly called from an exported component defined in the Android manifest. For example, if the root callable is in the `MainActivity` and the `MainActivity` is exported, this feature will be attached. It is needed in order to determine if an `Intent` source is third-party controllable or not. This feature is sometimes accompanied by `via-class` which tells you which class Mariana Trench used to determine that the root callable is called from an exported class.
- via-caller-unexported
  - Same as `via-caller-exported` but applied if the root callable is considered to be called only via unexported components
- via-caller-permission
  - Similair to `via-caller-exported` but applied if the root callable paths to a manifest entry that has a protectionLevel or Android permission declared.
- via-explicit-intent
  - Applied when the taint flow goes via a class or package name setter on an Intent. This can be used to infer whether a launched Intent can resolve to third party apps or only to a specifically defined app (implicit versus explicit intents).
- via-inner-class-this
  - Anonymous classes in Java byte code transfer the taint from the parent class to the anonymous class via `this.this$0` which can lead to [broaden false positives](../models/#taint-broadening). This feature can be used to filter out such flows when they are a common false positive pattern.
- cast:[...]
  - Cast features such as `cast:boolean` are applied when the tainted data is converted to that specific type. This allows for example to filter out data flows such as `taintedString.length()` where the returned tainted integer may no longer be of interest.
- via-obscure
  - Obscure methods are methods for which Mariana Trench doesn't have any byte code available. Therefore we generally apply taint-in-taint-out behaviour on these methods and add the feature `via-obscure` to tell the user that the data flow went along an obscure method.
- via-[...]-broadening
  - Is applied when any of the four broaden operations is applied (see [Models](../models/#taint-broadening)).
