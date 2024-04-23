/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

ArtificialMethods::ArtificialMethods(
    const KindFactory& kind_factory,
    DexStoresVector& stores) {
  Scope scope;
  array_allocation_method_ = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/mariana_trench/artificial/ArrayAllocation;",
      /* method_name */ "allocateArray",
      /* parameter_types */ "I",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true,
      /* is_private */ false,
      /* is_native*/ false);
  array_allocation_kind_ = kind_factory.get("ArrayAllocation");
  array_allocation_kind_used_ = true;

  DexStore store("artificial classes");
  store.add_classes(scope);
  stores.push_back(store);
}

std::vector<Model> ArtificialMethods::models(Context& context) const {
  std::vector<Model> models;

  // One would expect to check the array_allocation_kind_used_ flag here
  // but this method is called before used-ness is determined.
  auto* method = context.methods->get(array_allocation_method_);
  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 0)),
      TaintConfig(
          array_allocation_kind_,
          /* callee_port */ nullptr,
          /* callee */ nullptr,
          /* call_kind */ CallKind::declaration(),
          /* call_position */ nullptr,
          /* class_interval_context */ CallClassIntervalContext(),
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ FeatureSet::bottom(),
          /* via_type_of_ports */ {},
          /* via_value_of_ports */ {},
          /* canonical_names */ {},
          /* output_paths */ {},
          /* local_positions */ {},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* extra_traces */ {}));
  models.push_back(model);

  return models;
}

void ArtificialMethods::set_unused_kinds(
    const std::unordered_set<const Kind*>& unused_kinds) {
  if (unused_kinds.find(array_allocation_kind_) != unused_kinds.end()) {
    array_allocation_kind_used_ = false;
  }
}

} // namespace marianatrench
