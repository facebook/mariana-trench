/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

class TransformList;

/**
 * Used to represent the transformations applied to the base_kind.
 * `base_kind` can be:
 *   - For Sources: NamedKind
 *   - For ParameterSources: NamedKind
 *   - For Sinks: NamedKind, PartialKind, TriggeredPartialKind
 *   - For Propagations: PropagationKind
 * `global_transforms` and `local_transforms` store ordered list of `Transform`,
 * `SanitizeTransform`, or `SourceAsTransform`
 */
class TransformKind final : public Kind {
 public:
  explicit TransformKind(
      const Kind* base_kind,
      const TransformList* MT_NULLABLE local_transforms,
      const TransformList* MT_NULLABLE global_transforms)
      : base_kind_(base_kind),
        local_transforms_(local_transforms),
        global_transforms_(global_transforms) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TransformKind)

  bool operator==(const TransformKind& other) const;

  void show(std::ostream&) const override;

  Json::Value to_json() const override;

  static const TransformKind* from_inner_json(
      const Json::Value& value,
      Context& context);

  std::string to_trace_string() const override;

  const Kind* base_kind() const {
    return base_kind_;
  }

  const TransformList* MT_NULLABLE local_transforms() const {
    return local_transforms_;
  }

  const TransformList* MT_NULLABLE global_transforms() const {
    return global_transforms_;
  }

  const Kind* discard_transforms() const override;

  bool has_source_as_transform() const;

  bool has_non_sanitize_transform() const;

 private:
  const Kind* base_kind_;
  const TransformList* MT_NULLABLE local_transforms_;
  const TransformList* MT_NULLABLE global_transforms_;
};

} // namespace marianatrench
