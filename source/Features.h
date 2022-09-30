/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <mariana-trench/Feature.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

class Features final {
 public:
  Features() = default;
  Features(const Features&) = delete;
  Features(Features&&) = delete;
  Features& operator=(const Features&) = delete;
  Features& operator=(Features&&) = delete;
  ~Features() = default;

  const Feature* get(const std::string& data) const;

  const Feature* get_via_type_of_feature(const DexType* MT_NULLABLE type) const;
  const Feature* get_via_cast_feature(const DexType* MT_NULLABLE type) const;
  const Feature* get_via_value_of_feature(
      const std::optional<std::string_view>& value) const;
  const Feature* get_via_shim_feature(const Method* MT_NULLABLE method) const;
  /**
   * This feature is added to (non-artificial) source taint that is collapsed
   * before checking for flows into sinks.
   */
  const Feature* get_issue_broadening_feature() const;
  /**
   * This feature is added to the input taint of a propagation that is collapsed
   * before applying that propagation.
   */
  const Feature* get_propagation_broadening_feature() const;

 private:
  UniquePointerFactory<std::string, Feature> factory_;
};

} // namespace marianatrench
