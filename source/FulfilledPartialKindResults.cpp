/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FulfilledPartialKindResults.h>

namespace marianatrench {

void FulfilledPartialKindResults::store_call(
    const IRInstruction* invoke,
    FulfilledPartialKindState state) {
  if (state.empty()) {
    calls_.erase(invoke);
  } else {
    calls_.insert_or_assign(invoke, std::move(state));
  }
}

void FulfilledPartialKindResults::store_artificial_call(
    const ArtificialCallee* artificial_callee,
    FulfilledPartialKindState state) {
  if (state.empty()) {
    artifical_calls_.erase(artificial_callee);
  } else {
    artifical_calls_.insert_or_assign(artificial_callee, std::move(state));
  }
}

const FulfilledPartialKindState& FulfilledPartialKindResults::get_call(
    const IRInstruction* invoke) const {
  auto it = calls_.find(invoke);
  if (it == calls_.end()) {
    return empty_state_;
  } else {
    return it->second;
  }
}

const FulfilledPartialKindState&
FulfilledPartialKindResults::get_artificial_call(
    const ArtificialCallee* artificial_callee) const {
  auto it = artifical_calls_.find(artificial_callee);
  if (it == artifical_calls_.end()) {
    return empty_state_;
  } else {
    return it->second;
  }
}

} // namespace marianatrench
