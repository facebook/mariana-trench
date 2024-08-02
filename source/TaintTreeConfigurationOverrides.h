/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <json/json.h>

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/ScalarAbstractDomain.h>

namespace marianatrench {

enum class TaintTreeConfigurationOverrideOptions : unsigned {
  MaxModelWidth,
  MaxModelHeight,
};

} // namespace marianatrench

template <>
struct sparta::PatriciaTreeKeyTrait<
    marianatrench::TaintTreeConfigurationOverrideOptions> {
  using IntegerType = unsigned;
};

namespace marianatrench {

class TaintTreeConfigurationOverrides final
    : public sparta::AbstractDomain<TaintTreeConfigurationOverrides> {
 private:
  using ScalarAbstractDomain =
      ScalarAbstractDomainScaffolding<scalar_impl::ScalarBottomIsZero>;
  using OptionMap = sparta::PatriciaTreeMapAbstractPartition<
      TaintTreeConfigurationOverrideOptions,
      ScalarAbstractDomain>;

 public:
  TaintTreeConfigurationOverrides() = default;

  explicit TaintTreeConfigurationOverrides(OptionMap options)
      : options_(std::move(options)) {}

  explicit TaintTreeConfigurationOverrides(
      std::initializer_list<std::pair<
          TaintTreeConfigurationOverrideOptions,
          ScalarAbstractDomain::IntType>> options);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      TaintTreeConfigurationOverrides)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(
      TaintTreeConfigurationOverrides,
      OptionMap,
      options_)

  void add(TaintTreeConfigurationOverrideOptions option, unsigned int value);

  Json::Value to_json() const;

  static TaintTreeConfigurationOverrides from_json(Json::Value value);

  friend std::ostream& operator<<(
      std::ostream& out,
      const TaintTreeConfigurationOverrides& overrides);

 private:
  OptionMap options_;
};

} // namespace marianatrench
