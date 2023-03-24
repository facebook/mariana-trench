/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/TransformList.h>

namespace marianatrench {

TransformList TransformList::reverse_of(const TransformList* transforms) {
  return TransformList(
      List(transforms->transforms_.rbegin(), transforms->transforms_.rend()));
}

std::string TransformList::to_trace_string() const {
  std::string value{};

  for (auto iterator = transforms_.begin(), end = transforms_.end();
       iterator != end;) {
    value.append((*iterator)->to_trace_string());

    std::advance(iterator, 1);
    if (iterator != end) {
      value.append(":");
    }
  }

  return value;
}

TransformList TransformList::from_json(
    const Json::Value& value,
    Context& context) {
  const auto& transforms_array = JsonValidation::nonempty_array(value);

  List transforms{};
  transforms.reserve(transforms_array.size());

  for (const auto& transform : transforms_array) {
    transforms.push_back(context.kinds->get(JsonValidation::string(transform)));
  }

  return TransformList(std::move(transforms));
}

} // namespace marianatrench
