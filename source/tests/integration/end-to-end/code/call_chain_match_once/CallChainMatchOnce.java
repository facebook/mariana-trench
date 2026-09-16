/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

// Declares its `call-chain` effect sink wrapped in `MatchOnce[...]`.
class MatchOnceApi {
  void execute() {}
}

// Declares a `call-chain` effect sink at the same port without the wrapper, so
// it keeps propagating to every caller.
class PlainApi {
  void execute() {}
}

// Wrapped like `MatchOnceApi`, and additionally names its argument with
// `via_value_of`, so the issue carries the constant the caller passed.
class MatchOnceValueApi {
  void execute(long specifier) {}
}

public class CallChainMatchOnce {
  // No effect source, so both sinks propagate through here to `middle`.
  static void leaf() {
    new MatchOnceApi().execute();
    new PlainApi().execute();
    new MatchOnceValueApi().execute(1004L);
  }

  // Has the effect source, so both rules match here. The wrapped sink stops;
  // `PlainSink` keeps climbing.
  static void middle() {
    leaf();
  }

  // Has the effect source too. Only `PlainSink` still reaches this far, which
  // is the duplicate issue the wrapper exists to remove.
  static void outer() {
    middle();
  }
}
