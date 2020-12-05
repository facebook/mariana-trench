/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/algorithm/string/predicate.hpp>

#include <mariana-trench/Generator.h>

namespace {

std::unordered_map<std::string, marianatrench::ParameterPosition>
    k_touch_signatures = {
        {"Landroid/view/ViewGroup;.dispatchTouchEvent:(Landroid/view/MotionEvent;)Z",
         1},
        {"Landroid/app/Activity;.dispatchTouchEvent:(Landroid/view/MotionEvent;)Z",
         1},
        {"Landroid/view/View;.onTouchEvent:(Landroid/view/MotionEvent;)Z", 1},
};

} // namespace

namespace marianatrench {

class TouchEventSinkGenerator : public Generator {
 public:
  explicit TouchEventSinkGenerator(Context& context)
      : Generator("touch_event_sinks", context) {}

  std::vector<Model> run(const DexStoresVector& stores) {
    std::vector<Model> models;

    for (const auto& touch_signature : k_touch_signatures) {
      // Add sink model.
      auto* method = methods_.get(touch_signature.first);
      if (!method) {
        continue;
      }

      auto model = Model(method, context_);
      model.add_sink(
          AccessPath(Root(Root::Kind::Argument, touch_signature.second)),
          generator::sink(
              context_,
              method,
              /* kind */ "TouchEvent"));
      models.push_back(model);
    }

    return models;
  }
};

} // namespace marianatrench
