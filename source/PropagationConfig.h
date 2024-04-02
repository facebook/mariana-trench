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
#include <mariana-trench/CallKind.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PropagationKind.h>
#include <mariana-trench/TransformKind.h>

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
      const Kind* kind,
      PathTreeDomain output_paths,
      FeatureMayAlwaysSet inferred_features,
      FeatureMayAlwaysSet locally_inferred_features,
      FeatureSet user_features)
      : input_path_(std::move(input_path)),
        kind_(kind),
        output_paths_(std::move(output_paths)),
        inferred_features_(std::move(inferred_features)),
        locally_inferred_features_(std::move(locally_inferred_features)),
        user_features_(std::move(user_features)),
        call_kind_(CallKind::propagation()) {
    mt_assert(kind_ != nullptr);
    mt_assert(kind_->discard_transforms()->is<PropagationKind>());
    mt_assert(!output_paths_.is_bottom());

    if (kind_->is<TransformKind>()) {
      call_kind_ = CallKind::propagation_with_trace(CallKind::Declaration);
    } else {
      call_kind_ = CallKind::propagation();
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PropagationConfig)

  friend bool operator==(
      const PropagationConfig& self,
      const PropagationConfig& other) {
    return self.input_path_ == other.input_path_ && self.kind_ == other.kind_ &&
        self.output_paths_ == other.output_paths_ &&
        self.inferred_features_ == other.inferred_features_ &&
        self.locally_inferred_features_ == other.locally_inferred_features_ &&
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

  const Kind* kind() const {
    return kind_;
  }

  const PropagationKind* propagation_kind() const {
    const auto* propagation_kind =
        kind_->discard_transforms()->as<PropagationKind>();
    mt_assert(propagation_kind != nullptr);
    return propagation_kind;
  }

  const PathTreeDomain& output_paths() const {
    return output_paths_;
  }

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  const CallKind& call_kind() const {
    return call_kind_;
  }

  const AccessPath* MT_NULLABLE callee_port() const;

  static PropagationConfig from_json(
      const Json::Value& value,
      Context& context);

  friend std::ostream& operator<<(
      std::ostream& out,
      const PropagationConfig& propagation);

 private:
  AccessPath input_path_;
  const Kind* kind_;
  PathTreeDomain output_paths_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureMayAlwaysSet locally_inferred_features_;
  FeatureSet user_features_;
  CallKind call_kind_;
};

} // namespace marianatrench
