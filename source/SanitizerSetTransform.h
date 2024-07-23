/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <boost/container/flat_set.hpp>
#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/SourceSinkKind.h>
#include <mariana-trench/Transform.h>

namespace marianatrench {

class SanitizerSetTransform final : public Transform {
 public:
  using Set = boost::container::flat_set<SourceSinkKind>;

  explicit SanitizerSetTransform(const Set& kinds) : kinds_(kinds) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SanitizerSetTransform)

  std::string to_trace_string() const override;
  void show(std::ostream&) const override;

  static const SanitizerSetTransform* from_trace_string(
      const std::string& transform,
      Context& context);

  static const SanitizerSetTransform* from_config_json(
      const Json::Value& transform,
      Context& context);

  const Set& kinds() const {
    return kinds_;
  }

  struct SetHash {
    // Directly define here for inline oppotunities
    std::size_t operator()(const Set& kinds) const {
      return boost::hash_range(kinds.begin(), kinds.end());
    }
  };

 private:
  Set kinds_;
};

} // namespace marianatrench
