/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <algorithm>

#include <mariana-trench/Access.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {
namespace {

// TODO: Used for the hardcoded version. Remove and use
// ShimMethod::type_position()
std::optional<ShimParameterPosition> get_type_position(
    const DexType* dex_type,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  auto found = parameter_types_to_position.find(dex_type);
  if (found == parameter_types_to_position.end()) {
    return std::nullopt;
  }

  LOG(5,
      "Found dex type {} in shim parameter position: {}",
      found->first->str(),
      found->second);

  return found->second;
}

std::vector<ShimParameterPosition> get_parameter_positions(
    const Method* method,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  std::vector<ShimParameterPosition> parameters;

  const auto* method_prototype = method->get_proto();
  if (!method_prototype) {
    return parameters;
  }

  const auto* dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return parameters;
  }

  for (const auto* dex_argument : *dex_arguments) {
    if (auto position =
            get_type_position(dex_argument, parameter_types_to_position)) {
      parameters.push_back(*position);
    }
  }

  return parameters;
}

bool valid_receiver_position(
    const Method* call_target,
    std::optional<ShimParameterPosition> receiver_position_in_shim,
    const ShimMethod& shim_method) {
  if (receiver_position_in_shim &&
      call_target->get_class() !=
          shim_method.parameter_type(*receiver_position_in_shim)) {
    ERROR(1, "Invalid call_on position specified for shim callee: {}");
    return false;
  }

  return receiver_position_in_shim.has_value();
}

bool valid_parameter_positions(
    const Method* call_target,
    std::vector<ShimParameterPosition> parameter_positions_in_shim,
    const ShimMethod& shim_method) {
  if (parameter_positions_in_shim.empty()) {
    return false;
  }

  const auto* call_target_arguments = call_target->get_proto()->get_args();

  // Check that all of the forwarded parameters from the shim have corresponding
  // parameters in the call_target.
  return std::all_of(
      parameter_positions_in_shim.begin(),
      parameter_positions_in_shim.end(),
      [&](ShimParameterPosition position) {
        auto parameter_type = shim_method.parameter_type(position);
        return parameter_type != nullptr &&
            std::any_of(
                   call_target_arguments->begin(),
                   call_target_arguments->end(),
                   [&](const DexType* dex_type) {
                     return dex_type == parameter_type;
                   });
      });
}

} // namespace

ShimMethod::ShimMethod(const Method* method) : method_(method) {
  ShimParameterPosition index = 0;

  if (!method_->is_static()) {
    // Include "this" as argument 0
    types_to_position_.emplace(method->get_class(), index++);
  }

  const auto* method_prototype = method_->get_proto();
  if (!method_prototype) {
    return;
  }

  const auto* dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return;
  }

  for (const auto* dex_argument : *dex_arguments) {
    types_to_position_.emplace(std::make_pair(dex_argument, index++));
  }
}

DexType* MT_NULLABLE
ShimMethod::parameter_type(ShimParameterPosition argument) const {
  return method_->parameter_type(argument);
}

std::optional<ShimParameterPosition> ShimMethod::type_position(
    const DexType* dex_type) const {
  auto found = types_to_position_.find(dex_type);
  if (found == types_to_position_.end()) {
    return std::nullopt;
  }

  LOG(5,
      "Found dex type {} in shim parameter position: {}",
      found->first->str(),
      found->second);

  return found->second;
}

std::vector<ShimParameterPosition> ShimMethod::method_parameter_positions(
    const Method* method) const {
  std::vector<ShimParameterPosition> parameters;

  const auto* method_prototype = method->get_proto();
  if (!method_prototype) {
    return parameters;
  }

  const auto* dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return parameters;
  }

  for (const auto* dex_argument : *dex_arguments) {
    if (auto position = type_position(dex_argument)) {
      parameters.push_back(*position);
    }
  }

  return parameters;
}

ShimTarget::ShimTarget(
    const Method* method,
    std::optional<ShimParameterPosition> receiver_position,
    std::vector<ShimParameterPosition> parameter_positions)
    : call_target(method),
      receiver_position_in_shim(receiver_position),
      parameter_positions_in_shim(parameter_positions) {}

std::optional<ShimTarget> ShimTarget::from_json(
    const Json::Value& value,
    Context& context) {
  if (!value.isMember("signature")) {
    ERROR(1, "Invalid shim definition. callee must have `signature`");
    return std::nullopt;
  }

  auto signature = JsonValidation::string(value, "signature");
  auto* call_target = context.methods->get(signature);

  if (call_target == nullptr) {
    WARNING(1, "Provided method signature not found: `{}`", signature);
    return std::nullopt;
  }

  std::optional<ShimParameterPosition> receiver_position_in_shim;
  if (value.isMember("call_on")) {
    receiver_position_in_shim =
        Root::from_json(JsonValidation::string(value, "call_on"))
            .parameter_position();
  }

  std::vector<ShimParameterPosition> forward_parameters;
  for (const auto& parameter_value :
       JsonValidation::null_or_array(value, "forward_parameters")) {
    forward_parameters.push_back(
        Root::from_json(JsonValidation::string(parameter_value))
            .parameter_position());
  }

  return ShimTarget(call_target, receiver_position_in_shim, forward_parameters);
}

std::optional<ShimTarget> ShimTarget::try_create(
    const Method* call_target,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  if (call_target == nullptr) {
    return std::nullopt;
  }

  LOG(5, "Creating shim target: {}", call_target->signature());

  auto receiver_position_in_shim =
      get_type_position(call_target->get_class(), parameter_types_to_position);

  auto parameter_positions_in_shim =
      get_parameter_positions(call_target, parameter_types_to_position);

  return ShimTarget{
      call_target, receiver_position_in_shim, parameter_positions_in_shim};
}

std::optional<ShimTarget> ShimTarget::instantiate(
    const ShimMethod& shim_method) const {
  std::optional<ShimParameterPosition> receiver_position;

  if (valid_receiver_position(
          call_target, receiver_position_in_shim, shim_method)) {
    receiver_position = receiver_position_in_shim;
  } else {
    receiver_position = shim_method.type_position(call_target->get_class());
  }

  if (!receiver_position) {
    return std::nullopt;
  }

  std::vector<ShimParameterPosition> parameter_positions;
  if (valid_parameter_positions(
          call_target, parameter_positions_in_shim, shim_method)) {
    parameter_positions = parameter_positions_in_shim;
  } else {
    parameter_positions = shim_method.method_parameter_positions(call_target);
  }

  return ShimTarget(call_target, receiver_position, parameter_positions);
}

Shim::Shim(const Method* method, std::vector<ShimTarget> targets)
    : method_(method), targets_(std::move(targets)) {}

std::ostream& operator<<(std::ostream& out, const ShimTarget& shim_target) {
  out << "ShimTarget(method=`";
  if (shim_target.call_target != nullptr) {
    out << shim_target.call_target->show();
  }
  out << "`";
  if (shim_target.receiver_position_in_shim) {
    out << ", call_on=Argument(" << *shim_target.receiver_position_in_shim
        << ")";
  }
  if (!shim_target.parameter_positions_in_shim.empty()) {
    out << ", forward_parameters=[";
    for (const auto& parameter : shim_target.parameter_positions_in_shim) {
      out << " Argument(" << parameter << "),";
    }
    out << " ]";
  }
  return out << ")";
}

std::ostream& operator<<(std::ostream& out, const Shim& shim) {
  out << "ShimTarget(method=`";
  if (auto* method = shim.method()) {
    out << method->show();
  }
  out << "`";
  if (!shim.empty()) {
    out << ",\n  targets=[\n";
    for (const auto& target : shim.targets()) {
      out << "    " << target << ",\n";
    }
    out << "  ]";
  }
  return out << ")";
}

} // namespace marianatrench
