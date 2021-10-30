/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Field.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

/**
 * A `FieldModel` summarizes what we know about a field similar to how a `Model`
 * summarizes this for a method. These models are not inferred during the
 * analysis and must be specified by users in model generators.
 *
 * *generations* and *sinks* describe source and sink taint on the field
 * respectively. This taint is not affected by assignments to the field within
 * the analyzed source code.
 *
 */
class FieldModel final {
 public:
  explicit FieldModel() : field_(nullptr) {}

  explicit FieldModel(
      const Field* field,
      const std::vector<Frame>& generations = {},
      const std::vector<Frame>& sinks = {});

  FieldModel(const FieldModel& other) = default;
  FieldModel(FieldModel&&) = default;
  FieldModel& operator=(const FieldModel& other) = default;
  FieldModel& operator=(FieldModel&&) = default;
  ~FieldModel() = default;

  bool operator==(const FieldModel& other) const;
  bool operator!=(const FieldModel& other) const;

  const Field* MT_NULLABLE field() const {
    return field_;
  }

  FieldModel instantiate(const Field* field) const;

  bool empty() const;

  const Taint& generations() const {
    return generations_;
  }

  const Taint& sinks() const {
    return sinks_;
  }

  void add_generation(Frame source);
  void add_sink(Frame source);

  void join_with(const FieldModel& other);

  static FieldModel from_json(
      const Field* MT_NULLABLE field,
      const Json::Value& value,
      Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const FieldModel& model);

 private:
  void check_frame_consistency(const Frame& frame, std::string_view kind) const;

  const Field* MT_NULLABLE field_;
  Taint generations_;
  Taint sinks_;
};

} // namespace marianatrench
