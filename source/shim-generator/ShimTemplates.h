/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <variant>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {
namespace shim {

class ReceiverInfo {
 public:
  /**
   * `Kind` describes how the receiver of the shim target is interpreted.
   */
  enum class Kind {
    STATIC,
    INSTANCE,
    REFLECTION,
  };

 private:
  explicit ReceiverInfo(Kind type, ShimParameterPosition position);
  explicit ReceiverInfo(Kind type, std::string position);

 public:
  static ReceiverInfo from_json(const Json::Value& callee);

  /* Find this receiver's DexType corresponding to ShimMethod */
  const DexType* MT_NULLABLE
  receiver_dex_type(const ShimMethod& shim_method) const;

  Kind kind() const {
    return kind_;
  }

  const std::variant<ShimParameterPosition, std::string>& receiver() const {
    return receiver_;
  }

  friend std::ostream& operator<<(std::ostream& out, const ReceiverInfo& info);

 private:
  Kind kind_;
  std::variant<ShimParameterPosition, std::string> receiver_;
};

class TargetTemplate final {
 public:
  /**
   * `Kind` describes how the shim target is initialized.
   */
  enum class Kind {
    DEFINED,
    REFLECTION,
    LIFECYCLE,
  };

 private:
  explicit TargetTemplate(
      Kind kind,
      std::string target,
      ReceiverInfo receiver_info,
      ShimParameterMapping parameter_mapping);

 public:
  static TargetTemplate from_json(const Json::Value& callee);

  std::optional<ShimTargetVariant> instantiate(
      const Methods* methods,
      const ShimMethod& shim_method) const;

  std::string_view target() const {
    return target_;
  }

  const ReceiverInfo& receiver_info() const {
    return receiver_info_;
  }

  const ShimParameterMapping& parameter_map() const {
    return parameter_map_;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const TargetTemplate& target);

 private:
  Kind kind_;
  std::string target_;
  ReceiverInfo receiver_info_;
  ShimParameterMapping parameter_map_;
};

} // namespace shim

class ShimTemplate final {
 private:
  explicit ShimTemplate(std::vector<shim::TargetTemplate> targets);

 public:
  static ShimTemplate from_json(const Json::Value& shim_json);

  std::optional<Shim> instantiate(
      const Methods* methods,
      const Method* method_to_shim) const;

 private:
  std::vector<shim::TargetTemplate> targets_;
};

} // namespace marianatrench
