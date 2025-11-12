/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests

import android.content.Intent

class State {

  private var _state: Intent? = null

  val state: Intent?
    get() = _state

  fun updateResult(result: Intent) {
    _state = result
  }
}

public class KotlinScopeFunctions {
  fun tito(o: Any): Any {
    return o
  }

  fun testSimpleAlso() {
    val myState = State()
    val intent = Intent()
    // Kotlin scope functions like "also" executes a block of code within the context of an object.
    // Here, the lamdba argument "it" aliases "intent". The return value is the context object.
    myState.updateResult(intent.also { it.putExtra("new", Origin.source() as String) })

    // Expect issue
    Origin.sink(myState.state)
  }

  var source: String? = null

  fun testAlsoWithCaptures() {
    val myState = State()
    val intent = Intent()
    this.source = Origin.source() as String

    // The context object can also be named anything and can capture variables.
    // Here, "value" aliases "intent". The return value is the context object.
    myState.updateResult(intent.also { value -> value.putExtra("new", this.source) })

    // Expect issue
    Origin.sink(myState.state)
  }

  fun testWithAlsoChain() {
    val myState = State()
    val intent = Intent()
    val source = Origin.source() as String

    // Kotlin scope functions like "with" executes a block of code within the context of an object.
    // Here, the context object is available as the lambda receiver "this" within the body of "with"
    // and aliases "intent". The return value is the lambda return.
    myState.updateResult(
        with(intent) {
              this.putExtra("new", "safe")
              putExtra("source", source)
              tito(this) as Intent
            }
            .also { it.putExtra("other", source) }
    )

    // Expect issue
    Origin.sink(myState.state)
  }
}
