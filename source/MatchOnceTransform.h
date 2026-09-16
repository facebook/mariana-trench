/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Transform.h>

namespace marianatrench {

/**
 * Marks a `call-chain` effect sink that is not propagated to callers once it
 * has matched a rule. Written `MatchOnce` in a rule's `transforms`.
 *
 * A regular `call-chain` effect sink climbs to every transitive caller within
 * `max_call_chain_source_sink_distance`, joining its features at each hop. For
 * a rule whose effect source sits at several levels of the chain that reports
 * the same flow once per level, each copy carrying the features of every
 * sibling flow underneath. This reports it at the nearest effect source
 * instead, which is the stopping criterion `call-chain-exploitability` already
 * gets from `PartiallyFulfilledExploitabilityRuleState`.
 *
 * Carrying it as a transform on the sink kind rather than as a flag on the
 * model that declared the sink is what makes it work at every hop. A model
 * flag is only visible on the first hop: above that the callee is an
 * intermediate method that had the sink *inferred* onto it and carries nothing
 * from the original declaration. A transformed kind travels with the taint.
 *
 * Unlike most transforms this is not applied by a propagation. The declaring
 * model builds the transformed kind directly, and the rule opts in by listing
 * `MatchOnce` in `transforms`, which is what makes the two match after
 * `canonicalize_sink_kind`.
 */
class MatchOnceTransform final : public Transform {
 public:
  MatchOnceTransform() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MatchOnceTransform)

  std::string to_trace_string() const override;
  void show(std::ostream&) const override;

  static const MatchOnceTransform* from_trace_string(
      const std::string& transform,
      Context& context);
};

/**
 * True if `kind` carries the `MatchOnce` marker, i.e. it is a transform kind
 * with that transform in its list.
 */
bool has_match_once_transform(const Kind* kind);

} // namespace marianatrench
