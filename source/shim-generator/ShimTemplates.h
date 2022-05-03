/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

class ShimTemplate final {
 public:
  explicit ShimTemplate(std::vector<ShimTarget> shim_targets);
  static ShimTemplate from_json(const Json::Value& shim_json, Context& context);

  std::optional<Shim> instantiate(const Method* method_to_shim) const;

 private:
  std::vector<ShimTarget> shim_targets_;
};

} // namespace marianatrench
