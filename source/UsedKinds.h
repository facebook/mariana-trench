/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/TransformKind.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

class UsedKinds {
 public:
  using NamedKindToTransformsMap =
      std::unordered_map<const Kind*, std::unordered_set<const TransformList*>>;
  using PropagationKindTransformsSet = std::unordered_set<const TransformList*>;

 public:
  explicit UsedKinds(const TransformsFactory& transforms_factory)
      : transforms_factory_(transforms_factory) {}

  MOVE_CONSTRUCTOR_ONLY(UsedKinds);

  /* Load the rules from the json file specified in the given options. */
  static UsedKinds from_rules(
      const Rules& rules,
      const TransformsFactory& transforms_factory);

  /*
   * Before the analysis begins, a context might contain Kinds that are built
   * into the binary, or specified in a model generator, but aren't actually
   * used in any rule. These can be removed to save memory/time.
   */
  std::size_t remove_unused_kinds(
      const Rules& rules,
      const KindFactory& kind_factory,
      const Methods& methods,
      ArtificialMethods& artificial_methods,
      Registry& registry);

  std::size_t source_sink_size() const {
    return named_kind_to_transforms_.size();
  }

  std::size_t propagation_size() const {
    return propagation_kind_to_transforms_.size();
  }

  // Only use for testing purposes
  const NamedKindToTransformsMap& named_kind_to_transforms() const {
    return named_kind_to_transforms_;
  }

  // Only use for testing purposes
  const PropagationKindTransformsSet& propagation_kind_to_transforms() const {
    return propagation_kind_to_transforms_;
  }

  bool should_keep(const TransformKind* transform_kind) const;

 private:
  const TransformsFactory& transforms_factory_;
  NamedKindToTransformsMap named_kind_to_transforms_;
  PropagationKindTransformsSet propagation_kind_to_transforms_;
};

} // namespace marianatrench
