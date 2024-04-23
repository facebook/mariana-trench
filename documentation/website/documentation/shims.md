---
id: shims
title: Shims
sidebar_label: Shims
---

import FbShims from './fb/shims.md';

## What is a "shim"?

We can think of a “shim” as the ability to say: "a call to a method matching
`<shimmed-method>` also implies calls to methods matching `<shim-target-1>`,
`<shim-target-2>`, …, `<shim-target-n>`.  This allows us to manually augment the
call-graph with artificial calls to methods at specific call-sites. This is
useful for simulating events in the android application by mimicking calls to
the known event-handlers which allows us to capture dataflows otherwise missed
due to the missing invocation of event-handlers.

For example, when a new fragment is added to a `FragmentManager`, the
`FragmentManager` is responsible for moving them through its lifecycle states
([reference](https://developer.android.com/guide/fragments/lifecycle#fragmentmanager)).
But we do not see the calls to the lifecycle event handlers on the new instance
of the fragment and can miss flows. We can define shims as follows to fill in
such missing links.

```java
class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedinstanceState) {
        if (savedInstanceState == null) {
            Fragment myFragment = new MyFragment();
            // highlight-next-line
            myFragment.setArguments(getTaintedBundle());  // Source
            getSupportFragmentManager().beginTransaction()
                .setReorderingAllowed(true)
                // highlight-next-line
                .add(R.id.fragment_container_view, myFragment, null)  // FragmentManager handles `myFragment`'s lifecycle.
                .commit();
        }
    }
}

class MyFragment extends Fragment {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        // highlight-next-line
        sink(getArguments()); // Issue if `mArguments` is tainted using `setArguments()`
    }
}
```

In this case, we can define a shim on `FragmentTransaction`'s `add()`  to
_trigger_ the lifecycle wrapper method of it's `Fragment` argument to mimic the
android's lifecycle management to catch this flow.

```json
{
    "find": "methods",
    "where": [
        {
            "constraint": "signature_pattern",
            "pattern": "Landroidx/fragment/app/FragmentTransaction;\\.add:\\(ILandroidx/fragment/app/Fragment;.*"
        }
    ],
    "shim": {
        "callees": [
            {
                "type_of": "Argument(2)",
                "lifecycle_name": "xfragment_lifecycle_wrapper"
            }
        ]
    }
},
```

## Terminologies
- shimmed-method: The method matching the `"where"` clause in the shim generator.
- shim-target: Method matching each of the `"callees"` object specified in the shim generator.
- parameters map: Mapping for arguments from shim-target to shimmed-method.

All callsites of the `shimmed-method` implies calls to all the `shim-targets`
specified with arguments propagated from the shimmed-method to shim-target based
on the parameters map.


## Specifying Shims
### Configuration file
The Mariana Trench binary consumes shim configuration files specified with the
argument `--shims-paths`. Each file contains a json array consisting of shim
definitions. Default set of shims can found in
[shims.json](https://github.com/facebook/mariana-trench/blob/main/configuration/shims.json)

### Shim Definition
Each shim definition object consists of the following keys:
- `find`: Currently only option is `methods`.
- `where`: A list of "constraints" which identifies the "shimmed-method". This is same as in [model generators](models.md#generators).
- `shim`: A list of "callees" each of which identifies a "shim-target". Each `callees` object needs to define the following:
  #### Receiver for the shim-target
  Receiver can be defined using one of the following keys:
  - `static`: Used to specify static methods as shim-targets.

    Expected value: A string specifying the dex class containing the shim-target method.

  - `type_of`: Used to specify an instance argument of the shimmed-method as the receiver.

    Expected value: A string specifying the _port_/_access path_ of the shimmed-method.

  - `reflected_type_of`: Used to specify a reflection argument of the shimmed-method as the receiver.

    Expected value: A string specifying the _port/access path_ of the shimmed-method. The type of the specified shimmed-method argument _must_ be: `Ljava/lang/Class;`.

  #### Method to call
  Method can be defined using one of the following keys:
  - `method_name`: Used to specify an existing method of the receiver as the shim-target.

    Expected value: A string specifying the dex proto of the method. This is of the form: `<method name>:(<parameter types>)<return type>`.

  - `lifecycle_name`: Used to specify lifecycle method specified using the option `--lifecycles-paths` as the shim-target.

    Expected value: A string matching the `method_name` specified in the lifecycle configuration.

  #### Parameters map (optional)
  A map specifying how the parameters of the shimmed-method should be propagated to the shim-target.
  If not specified, each argument of the shim-target is mapped to the first
  argument of the shimmed-method with the matching type.

  Expected format is as follows where the "key" refers to the shim-target and the value refers to the shimmed-method.
  ```json
  "parameters_map": {
      "Argument(<int>)": "Argument(<int>)",
      ...
  }
  ```

### Example
```java showLineNumbers
class TargetA {
    void methodA(Object o) {}
}

class TargetB {
    void methodB(Object o1, Object o2) {}
}

class TargetC {
    static void methodC(Object o) {}
}

class Shimmed {
    void shimMe(A a, Class b, Object o) {}
}

class Test {
    void test() {
        new Shimmed().shimMe(new TargetA(), TargetB.class, new Object());
    }
}
```

Shim definitions:

```json
{
    "find": "methods",
    "where": [
        {
            "constraint": "signature_pattern",
            "pattern": "LShimmed;\\.shimMe:\\(LTargetA;Ljava/lang/Class;Ljava/lang/Object;\\)V"
        }
    ],
    "shim": {
        "callees": [
            {
                "type_of": "Argument(1)",
                "method_name": "methodA:(Ljava/lang/Object;)V"
            },
            {
                "reflected_type_of": "Argument(2)",
                "method_name": "methodB:(Ljava/lang/Object;Ljava/lang/Object;)V",
                "parameters_map": {
                    "Argument(2)": "Argument(3)"
                }
            },
            {
                "static": "LTargetC;",
                "method_name": "method:C(Ljava/lang/Object;)V",
                "parameters_map": {
                    "Argument(0)": "Argument(3)"
                }
            }
        ]
    }
},
```
With this shim definition, the call to method `shimMe()` on line X will introduce calls to:
- `LTargetA.methodA:(Ljava/lang/Object;)V` where argument `o` is inferred to be `Argument(3)` of method `LShimmed;.shimMe:(LTargetA;Ljava/lang/Class;Ljava/lang/Object;)V`.
- `LTargetB.methodB:(Ljava/lang/Object;)V` where argument `o2` is mapped to `Argument(3)` of method `LShimmed;.shimMe:(LTargetA;Ljava/lang/Class;Ljava/lang/Object;)V` as specified. Note that argument `Class b` of the shimmed-method is resolved to be `TargetB` at the callsite.
- `LTargetC.methodC:()V` where argument `o` is mapped to `Argument(3)` of method `LShimmed;.shimMe:(LTargetA;Ljava/lang/Class;Ljava/lang/Object;)V` as specified. Note here that we specify `Argument(0)` of the shim-target in the parameters_map as this is a static method.

Note that when issues are found due to taint flow through shimmed-method to
shim-target, the trace following the call-site of the _shimmed-method_ will be
the _shim-target_ and a feature `via-shim:<shimmed-method>` will be introduced
at that point.

Sample shim definitions [here](https://github.com/facebook/mariana-trench/blob/main/source/tests/integration/end-to-end/code/shims/shims.json).

<FbShims />
