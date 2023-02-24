/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/FulfilledPartialKindState.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * Stores fulfilled partial kinds for each call.
 * This is computed in the forward taint analysis and is passed to the backward
 * taint analysis to create sinks.
 */
class FulfilledPartialKindResults final {
 public:
  FulfilledPartialKindResults() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FulfilledPartialKindResults)

  void store_call(const IRInstruction* invoke, FulfilledPartialKindState state);
  void store_artificial_call(
      const ArtificialCallee* artificial_callee,
      FulfilledPartialKindState state);

  const FulfilledPartialKindState& get_call(const IRInstruction* invoke) const;
  const FulfilledPartialKindState& get_artificial_call(
      const ArtificialCallee* artificial_callee) const;

 private:
  FulfilledPartialKindState empty_state_;
  std::unordered_map<const IRInstruction*, FulfilledPartialKindState> calls_;
  std::unordered_map<const ArtificialCallee*, FulfilledPartialKindState>
      artifical_calls_;
};

} // namespace marianatrench
