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
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TransformsFactory.h>

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
  List no_sanitizers{};
  for (const auto* transform : *transforms) {
    if (!transform->is<SanitizerSetTransform>()) {
      no_sanitizers.push_back(transform);
    }
  }

  // No canonicalization needed because we removed all sanitizers
  return TransformList(std::move(no_sanitizers));
}

bool TransformList::has_source_as_transform() const {
  return std::any_of(
      transforms_.begin(), transforms_.end(), [](const Transform* transform) {
        return transform->is<SourceAsTransform>();
      });
}

bool TransformList::has_non_sanitize_transform() const {
  return std::any_of(
      transforms_.begin(), transforms_.end(), [](const Transform* transform) {
        return !transform->is<SanitizerSetTransform>();
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
  List result{};

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

  // from_trace_string only takes input of something MT previously dumped, so
  // we can assume that the result is canonicalized.
  return TransformList(std::move(result));
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

  // Users are not supposed to write transforms that contains sanitizers in json
  // config, these should be specified in the `sanitizers` field.
  return TransformList(std::move(transforms));
}

TransformList TransformList::from_kind(const Kind* kind, Context& context) {
  return TransformList(
      List{context.transforms_factory->create_source_as_transform(kind)});
}

TransformList TransformList::concat(
    const TransformList* left,
    const TransformList* right) {
  List transforms{};
  transforms.reserve(left->size() + right->size());
  transforms.insert(transforms.end(), left->begin(), left->end());
  transforms.insert(transforms.end(), right->begin(), right->end());

  return TransformList(std::move(transforms));
}

TransformList TransformList::canonicalize(
    const TransformList* transforms,
    const TransformsFactory& transforms_factory) {
  List canonicalized{};
  SanitizerSetTransform::Set sanitize_kinds{};

  for (const auto* transform : *transforms) {
    if (const auto* santize_transform =
            transform->as<SanitizerSetTransform>()) {
      const auto& current_sanitizers = santize_transform->kinds();
      sanitize_kinds.union_with(current_sanitizers);
    } else {
      if (!sanitize_kinds.empty()) {
        // We have a non-sanitizer transform after sanitizers, so we need to add
        // the sanitizers before this one and clear them out for next iteration
        canonicalized.push_back(
            transforms_factory.create_sanitizer_set_transform(sanitize_kinds));
        sanitize_kinds.clear();
      }
      canonicalized.push_back(transform);
    }
  }

  // Add the remaining sanitizers at the end
  if (!sanitize_kinds.empty()) {
    canonicalized.push_back(
        transforms_factory.create_sanitizer_set_transform(sanitize_kinds));
  }

  return TransformList(std::move(canonicalized));
}

TransformList TransformList::filter_global_sanitizers(
    const TransformList* incoming,
    const TransformList* existing_global,
    const TransformsFactory& transforms_factory) {
  SanitizerSetTransform::Set global_sanitizer_kinds{};
  for (const auto* global_sanitizer :
       existing_global
           ->find_consecutive_sanitizers<ApplicationDirection::Forward>()) {
    global_sanitizer_kinds.union_with(global_sanitizer->kinds());
  }

  // If there are no sanitizers in global transforms, we can just return
  if (global_sanitizer_kinds.empty()) {
    return *incoming;
  }

  const auto sanitizer_range =
      incoming->find_consecutive_sanitizers<ApplicationDirection::Backward>();

  // If there are no sanitizers in incoming transforms, we can just return
  if (sanitizer_range.empty()) {
    return *incoming;
  }

  List filtered{};
  for (const auto* incoming_sanitizer : sanitizer_range) {
    // Collect kinds that are not already in the global sanitizers
    SanitizerSetTransform::Set new_kinds =
        incoming_sanitizer->kinds().get_difference_with(global_sanitizer_kinds);

    // Add the sanitizers that are not entirely filtered
    if (!new_kinds.empty()) {
      filtered.push_back(
          transforms_factory.create_sanitizer_set_transform(new_kinds));
    }
  }

  // First copy over the transforms we did not touch
  List result{};
  result.reserve(incoming->size() - sanitizer_range.size() + filtered.size());
  result.insert(
      result.end(),
      incoming->begin(),
      incoming->end() - sanitizer_range.size());

  // Then copy over the sanitizers we did not filter out
  result.insert(result.end(), filtered.crbegin(), filtered.crend());

  return TransformList(std::move(result));
}

TransformList TransformList::discard_unmatched_sanitizers(
    const TransformList* incoming,
    const TransformsFactory& transforms_factory,
    transforms::TransformDirection direction) {
  // Since we call this function after TransformList::sanitizes(), which drops
  // taints if the kinds match, we know the sanitizers are guaranteed to not
  // match this base_kind and may be dropped with the following exceptions.
  // - Keep unmatched sink sanitizers during forward propagation
  // - Keep unmatched source sanitizers during backward propagation.

  bool discard_source = direction == transforms::TransformDirection::Forward;
  List result{};

  for (const auto* transform : *incoming) {
    if (const auto* sanitizer_transform =
            transform->as<SanitizerSetTransform>()) {
      // Filter out the source kinds
      auto new_kinds = sanitizer_transform->kinds();
      new_kinds.filter([discard_source](const auto& kind) {
        return !(discard_source ? kind.is_source() : kind.is_sink());
      });

      // If the sanitizers is not entirely discarded, add the remaining kinds
      if (!new_kinds.empty()) {
        result.push_back(
            transforms_factory.create_sanitizer_set_transform(new_kinds));
      }
    } else {
      result.push_back(transform);
    }
  }

  // No canonicalization needed because we removed all sanitizers
  return TransformList(std::move(result));
}

} // namespace marianatrench
