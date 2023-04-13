/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Field.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * A `FieldModel` summarizes what we know about a field similar to how a `Model`
 * summarizes this for a method. These models are not inferred during the
 * analysis and must be specified by users in model generators.
 *
 * *sources* and *sinks* describe source and sink taint on the field
 * respectively. This taint is not affected by assignments to the field within
 * the analyzed source code.
 *
 */
class FieldModel final {
 public:
  explicit FieldModel() : field_(nullptr) {}

  explicit FieldModel(const Field* field) : field_(field) {}

  explicit FieldModel(
      const Field* field,
      const std::vector<TaintConfig>& sources,
      const std::vector<TaintConfig>& sinks);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FieldModel)

  bool operator==(const FieldModel& other) const;
  bool operator!=(const FieldModel& other) const;

  const Field* MT_NULLABLE field() const {
    return field_;
  }

  FieldModel instantiate(const Field* field) const;

  bool empty() const;

  const Taint& sources() const {
    return sources_;
  }

  const Taint& sinks() const {
    return sinks_;
  }

  void add_source(TaintConfig source);
  void add_sink(TaintConfig source);

  void join_with(const FieldModel& other);

  static FieldModel from_json(
      const Field* MT_NULLABLE field,
      const Json::Value& value,
      Context& context);
  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  /*
   * Export the model to json and include the field position. For now, this is
   * always unknown
   */
  Json::Value to_json(Context& context) const;

  friend std::ostream& operator<<(std::ostream& out, const FieldModel& model);

 private:
  void check_taint_config_consistency(
      const TaintConfig& frame,
      std::string_view kind) const;

  void check_taint_consistency(const Taint& frame, std::string_view kind) const;

  void add_source(Taint source);
  void add_sink(Taint source);

  const Field* MT_NULLABLE field_;
  Taint sources_;
  Taint sinks_;
};

} // namespace marianatrench
