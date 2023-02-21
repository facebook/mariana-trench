/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <json/json.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PropagationKind.h>

namespace marianatrench {

/**
 * Class used to contain details for propagations, which represent the fact
 * that a method propagates taint from a parameter to another parameter or
 * return value.
 * This class is used to build the final `Taint` object which represents the
 * propagation in models.
 */
class PropagationConfig final {
 public:
  explicit PropagationConfig(
      AccessPath input_path,
      const PropagationKind* kind,
      PathTreeDomain output_paths,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features)
      : input_path_(std::move(input_path)),
        kind_(kind),
        output_paths_(std::move(output_paths)),
        inferred_features_(std::move(inferred_features)),
        user_features_(std::move(user_features)) {
    mt_assert(kind_ != nullptr);
    mt_assert(!output_paths_.is_bottom());
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PropagationConfig)

  friend bool operator==(
      const PropagationConfig& self,
      const PropagationConfig& other) {
    return self.input_path_ == other.input_path_ && self.kind_ == other.kind_ &&
        self.output_paths_ == other.output_paths_ &&
        self.inferred_features_ == other.inferred_features_ &&
        self.user_features_ == other.user_features_;
  }

  friend bool operator!=(
      const PropagationConfig& self,
      const PropagationConfig& other) {
    return !(self == other);
  }

  const AccessPath& input_path() const {
    return input_path_;
  }

  const PropagationKind* kind() const {
    return kind_;
  }

  const PathTreeDomain& output_paths() const {
    return output_paths_;
  }

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  static PropagationConfig from_json(
      const Json::Value& value,
      Context& context);

  friend std::ostream& operator<<(
      std::ostream& out,
      const PropagationConfig& propagation);

 private:
  AccessPath input_path_;
  const PropagationKind* kind_;
  PathTreeDomain output_paths_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
};

} // namespace marianatrench
