---
id: known-false-negatives
title: Known False Negatives
sidebar_label: Known False Negatives
---
import {OssOnly, FbInternalOnly} from 'docusaurus-plugin-internaldocs-fb/internal';
import FbKnownFalseNegatives from './fb/known_false_negatives.md';

Like any static analysis tools, Mariana Trench has false negatives. This documents the more well-known places where taint is dropped. Note that this is not an exhaustive list. See [this wiki](./debugging_fp_fns.md) for instructions on how to debug them.

Many of these options are *configurable*, not hard limits. There are analysis time, memory, and quality tradeoffs.

## Trace too Long

Mariana Trench stops propagating taint beyond a certain depth. This depth is currently configured at 7. In code:
```
// This method has depth 1.
public int get_source_1() { return source(); }

// This method has depth 2.
public int get_source_2() { return get_source_1(); }

...

// This method has depth 7.
public int get_source_7() { return get_source_6(); }

// This method theoretically has depth 8, but MT drops the source here.
public int get_source_8() { return get_source_7(); }
```
**Workaround:** If the chain of wrappers obviously leads to a source or sink, instead of defining the source at `source()`, one could write an additional model marking `get_source_7()` as a source.

## Fields of Fields of Fields of Fields...

Taint of an object is dropped when it occurs too deep within the object. This depth is configured at 4. In code:
```
public void taintedThis() {
  this.mField1 = source(); // This is OK
  this.mField1.mField2.mField3.mField4.mField5 = source(); // This gets dropped
}
```

**Workaround:** This isn’t much of a workaround, but one can manually configure the source on “this.mField1.....mField4” instead. This will be a form of over-abstraction and could lead to false positives.

## Fanouts

If a virtual method has too many overrides, beyond a certain number (currently configured at 40), we stop considering all overrides and look only at the direct method being called. In code:
```
interface IFace {
  public int possibleSource();
}

class Class1 implements IFace {
  public int possibleSource() { return 1; }
}
...

class Class41 implements IFace {
  public int possibleSource() { return source(); }
}

int maybeIssue(IFace iface) {
  // The source will get dropped here because there are too many overrides.
  // MT will not report an issue.
  sink(iface.possibleSource());
}


```
**Workaround:** Unfortunately, there are no known workarounds.

## Propagation across Arguments

Mariana Trench computes propagations for each method (this may be known as “tito” (taint-in-taint-out) in other tools). Propagations tell the analysis that if an argument is tainted by a source, whether its return value, or the method’s “this” object become tainted by the argument. However, Mariana Trench does not propagate taint from one argument to another. In code:
```
void setIntentVaue(Intent intent, Uri uri) {
  // MT sees that intent.putExtra has a propagation from uri (Argument(2)) to
  // intent (Argument(0) or this).
  intent.putExtra("label", uri);

  // However, when it finishes analyzing setIntentValue, it will not track the
  // propagation from uri to intent.
}

void falseNegative() {
  Uri uri = source();
  Intent intent = new Intent();

  // If this were the code, MT will detect a source->sink flow at launchActivitySink.
  // intent.putExtra("label", uri);

  // MT loses the flow from uri->intent at this point.
  setIntentValue(intent, uri);

  launchActivitySink(intent);
}
```
**Workaround:** Write a propagation model for the method. While Mariana Trench does not infer propagations across arguments, it does allow manual specification of such models.

<FbInternalOnly> <FbKnownFalseNegatives/> </FbInternalOnly>
