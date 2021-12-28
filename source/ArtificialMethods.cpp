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

ArtificialMethods::ArtificialMethods(Kinds& kinds, DexStoresVector& stores) {
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
  array_allocation_kind_ = kinds.get("ArrayAllocation");

  DexStore store("artificial classes");
  store.add_classes(scope);
  stores.push_back(store);
}

std::vector<Model> ArtificialMethods::models(Context& context) const {
  std::vector<Model> models;

  {
    auto* method = context.methods->get(array_allocation_method_);
    auto model = Model(method, context);
    model.add_mode(Model::Mode::SkipAnalysis, context);
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, 0)),
        Frame::leaf(array_allocation_kind_));
    models.push_back(model);
  }

  return models;
}

} // namespace marianatrench
