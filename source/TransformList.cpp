/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find_iterator.hpp>

#include <mariana-trench/KindFactory.h>
#include <mariana-trench/SourceAsTransform.h>
#include <mariana-trench/TransformList.h>

namespace marianatrench {

TransformList::TransformList(
    const std::vector<std::string>& transforms,
    Context& context) {
  mt_assert(transforms.size() != 0);
  transforms_.reserve(transforms.size());

  for (const auto& transform : transforms) {
    transforms_.push_back(
        context.transforms_factory->create_transform(transform));
  }
}

TransformList TransformList::reverse_of(const TransformList* transforms) {
  return TransformList(
      List(transforms->transforms_.rbegin(), transforms->transforms_.rend()));
}

TransformList TransformList::discard_sanitizers(
    const TransformList* transforms) {
  List no_sanitizers;
  for (const auto* transform : *transforms) {
    if (!transform->is<SanitizeTransform>()) {
      no_sanitizers.push_back(transform);
    }
  }
  return TransformList(std::move(no_sanitizers));
}

// TODO: T189558338 Handle cases where multiple, continuous transforms are
// present based on the canonicalization
bool TransformList::sanitizes(const Kind* kind, ApplicationDirection direction)
    const {
  const auto* maybe_sanitizer = direction == ApplicationDirection::Forward
      ? transforms_.front()
      : transforms_.back();

  if (const auto* sanitizer = maybe_sanitizer->as<SanitizeTransform>()) {
    return kind == sanitizer->kind();
  }
  return false;
}

bool TransformList::has_source_as_transform() const {
  return std::any_of(
      transforms_.begin(), transforms_.end(), [](const Transform* transform) {
        return transform->is<SourceAsTransform>();
      });
}

std::string TransformList::to_trace_string() const {
  std::string value{};

  for (auto iterator = transforms_.begin(), end = transforms_.end();
       iterator != end;) {
    value.append((*iterator)->to_trace_string());

    ++iterator;
    if (iterator != end) {
      value.append(":");
    }
  }

  return value;
}

TransformList TransformList::from_trace_string(
    const std::string& transforms,
    Context& context) {
  std::vector<const Transform*> result;

  auto transform_string = transforms;
  typedef boost::algorithm::split_iterator<std::string::iterator>
      string_split_iterator;
  for (string_split_iterator iterator = boost::algorithm::make_split_iterator(
           transform_string, first_finder(":", boost::is_equal()));
       iterator != string_split_iterator();
       ++iterator) {
    auto transform = std::string(iterator->begin(), iterator->end());
    result.push_back(Transform::from_trace_string(transform, context));
  }

  return TransformList(result);
}

TransformList TransformList::from_json(
    const Json::Value& value,
    Context& context) {
  const auto& transforms_array = JsonValidation::nonempty_array(value);

  List transforms{};
  transforms.reserve(transforms_array.size());

  for (const auto& transform : transforms_array) {
    transforms.push_back(Transform::from_json(transform, context));
  }

  return TransformList(std::move(transforms));
}

TransformList TransformList::from_kind(const Kind* kind, Context& context) {
  return TransformList(
      List{context.transforms_factory->create_source_as_transform(kind)});
}

} // namespace marianatrench
