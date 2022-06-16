/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>
#include <stdexcept>
#include <string>

#include <DexClass.h>
#include <TypeUtil.h>
#include <re2/re2.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/shim-generator/ShimTemplates.h>

namespace marianatrench {
namespace shim {
namespace {

static const std::string k_shim_target_prefix = "type_of";
static const std::string k_shim_reflection_target_prefix = "reflected_type_of";
static const std::string k_shim_static_target_prefix = "static";

static const re2::RE2 k_shim_target_placeholder(fmt::format(
    "%({}|{}|{}):(.*)%\\.?",
    k_shim_target_prefix,
    k_shim_reflection_target_prefix,
    k_shim_static_target_prefix));

ReceiverInfo::Kind string_to_receiver_kind(const std::string& kind) {
  if (kind == k_shim_target_prefix) {
    return ReceiverInfo::Kind::INSTANCE;
  } else if (kind == k_shim_static_target_prefix) {
    return ReceiverInfo::Kind::STATIC;
  } else if (kind == k_shim_reflection_target_prefix) {
    return ReceiverInfo::Kind::REFLECTION;
  } else {
    mt_unreachable();
  }
}

std::optional<ShimTarget> try_make_shim_target(
    const TargetTemplate& target_template,
    const Methods* methods,
    const ShimMethod& shim_method) {
  const auto& receiver_info = target_template.receiver_info();

  const auto* receiver_type = receiver_info.receiver_dex_type(shim_method);

  if (!receiver_type) {
    WARNING(
        1,
        "Shim method `{}` missing the receiver required for all shim callees `{}`.",
        shim_method.method()->show(),
        target_template.target());
    return std::nullopt;
  }

  const Method* call_target = methods->get(
      fmt::format("{}.{}", receiver_type->str(), target_template.target()));

  if (call_target == nullptr) {
    WARNING(
        1,
        "Could not instantiate shim target: `{}` for resolved receiver: `{}` specfied for method: `{}`",
        target_template,
        receiver_type->str(),
        shim_method.method()->show());
    return std::nullopt;
  }

  if (receiver_info.kind() == ReceiverInfo::Kind::STATIC &&
      !call_target->is_static()) {
    WARNING(
        1,
        "Expected static shim target: `{}` specfied for method: `{}`",
        call_target->show(),
        shim_method.method()->show());
    return std::nullopt;
  }

  auto instantiated_parameter_map = target_template.parameter_map().instantiate(
      call_target->get_name(),
      call_target->get_class(),
      call_target->get_proto(),
      call_target->is_static(),
      shim_method);

  if (receiver_info.kind() == ReceiverInfo::Kind::INSTANCE ||
      receiver_info.kind() == ReceiverInfo::Kind::REFLECTION) {
    // Include "this" as argument 0
    instantiated_parameter_map.insert(
        /* this */ 0,
        std::get<ShimParameterPosition>(receiver_info.receiver()));
  }

  return ShimTarget(call_target, std::move(instantiated_parameter_map));
}

} // namespace

ReceiverInfo::ReceiverInfo(Kind kind, ShimParameterPosition position)
    : kind_(kind), receiver_(position) {}

ReceiverInfo::ReceiverInfo(Kind kind, std::string type)
    : kind_(kind), receiver_(std::move(type)) {}

ReceiverInfo ReceiverInfo::from_json(
    const Json::Value& value,
    const std::string& field) {
  std::string signature = JsonValidation::string(value, field);

  std::string kind_string;
  std::string receiver;
  if (!re2::RE2::PartialMatch(
          signature, k_shim_target_placeholder, &kind_string, &receiver)) {
    throw JsonValidationError(
        value,
        field,
        fmt::format(
            "receiver placeholder format {}",
            k_shim_target_placeholder.pattern()));
  }

  auto kind = string_to_receiver_kind(kind_string);
  if (kind == Kind::STATIC) {
    return ReceiverInfo{kind, std::move(receiver)};
  }

  return ReceiverInfo{
      kind,
      static_cast<ShimParameterPosition>(
          Root::from_json(receiver).parameter_position())};
}

const DexType* MT_NULLABLE
ReceiverInfo::receiver_dex_type(const ShimMethod& shim_method) const {
  switch (kind_) {
    case Kind::STATIC:
      return DexType::get_type(std::get<std::string>(receiver_));

    case Kind::INSTANCE:
      return shim_method.parameter_type(
          std::get<ShimParameterPosition>(receiver_));

    case Kind::REFLECTION: {
      auto dex_type = shim_method.parameter_type(
          std::get<ShimParameterPosition>(receiver_));
      if (dex_type && dex_type != type::java_lang_Class()) {
        LOG(1,
            "Reflection shim expected receiver type: {} but got {}",
            type::java_lang_Class()->str(),
            dex_type->str());

        return nullptr;
      }

      return dex_type;
    }
  }
}

TargetTemplate::TargetTemplate(
    Kind kind,
    std::string target,
    ReceiverInfo receiver_info,
    ShimParameterMapping parameter_map)
    : kind_(kind),
      target_(std::move(target)),
      receiver_info_(std::move(receiver_info)),
      parameter_map_(std::move(parameter_map)) {}

TargetTemplate TargetTemplate::from_json(const Json::Value& callee) {
  auto parameter_map = ShimParameterMapping::from_json(
      JsonValidation::null_or_object(callee, "parameters_map"));

  if (callee.isMember("signature")) {
    auto receiver_info = ReceiverInfo::from_json(callee, "signature");
    std::string target = JsonValidation::string(callee, "signature");
    // Remove placeholder for receiver.
    re2::RE2::Replace(&target, k_shim_target_placeholder, "");

    return TargetTemplate{
        receiver_info.kind() == ReceiverInfo::Kind::REFLECTION
            ? Kind::REFLECTION
            : Kind::DEFINED,
        std::move(target),
        std::move(receiver_info),
        std::move(parameter_map)};
  } else if (callee.isMember("lifecycle_of")) {
    return TargetTemplate{
        Kind::LIFECYCLE,
        JsonValidation::string(callee, "method_name"),
        ReceiverInfo::from_json(callee, "lifecycle_of"),
        std::move(parameter_map)};
  }

  throw JsonValidationError(
      callee,
      /* field */ "callees",
      /* expected */
      "each `callees` object to specify either `signature` or `lifecycle_of`");
}

std::optional<ShimTarget> TargetTemplate::instantiate(
    const Methods* methods,
    const ShimMethod& shim_method) const {
  switch (kind_) {
    case Kind::DEFINED:
      return try_make_shim_target(*this, methods, shim_method);

    case Kind::REFLECTION:
    case Kind::LIFECYCLE:
      LOG(1, "{} not handled.", *this);
      return std::nullopt;
  }
}

std::ostream& operator<<(std::ostream& out, const ReceiverInfo& info) {
  out << "receiver=";
  if (info.kind_ == ReceiverInfo::Kind::STATIC) {
    out << std::get<std::string>(info.receiver_);
  } else {
    out << "Argument(" << std::get<ShimParameterPosition>(info.receiver_)
        << ")";
  }

  if (info.kind_ == ReceiverInfo::Kind::REFLECTION) {
    out << " (reflection)";
  }

  return out;
}

std::ostream& operator<<(std::ostream& out, const TargetTemplate& target) {
  out << "TargetTemplate(";
  out << target.receiver_info_ << ", ";

  if (target.kind_ == TargetTemplate::Kind::LIFECYCLE) {
    out << "lifecycle ";
  }
  out << "method=" << target.target_;
  out << ", " << target.parameter_map_;

  return out << ")";
}

} // namespace shim

ShimTemplate::ShimTemplate(std::vector<shim::TargetTemplate> targets)
    : targets_(std::move(targets)) {}

ShimTemplate ShimTemplate::from_json(const Json::Value& shim_json) {
  using namespace shim;
  std::vector<TargetTemplate> target_templates;

  for (const auto& callee :
       JsonValidation::null_or_array(shim_json, "callees")) {
    target_templates.push_back(TargetTemplate::from_json(callee));
  }

  return ShimTemplate(std::move(target_templates));
}

std::optional<Shim> ShimTemplate::instantiate(
    const Methods* methods,
    const Method* method_to_shim) const {
  LOG(5, "Instantiating ShimTemplate for {}", method_to_shim->show());

  auto shim_method = ShimMethod(method_to_shim);
  std::vector<ShimTarget> instantiated_shim_targets;

  for (const auto& target_template : targets_) {
    if (auto shim_target = target_template.instantiate(methods, shim_method)) {
      instantiated_shim_targets.push_back(*shim_target);
    }
  }

  if (instantiated_shim_targets.empty()) {
    return std::nullopt;
  }

  return Shim(method_to_shim, std::move(instantiated_shim_targets));
}

} // namespace marianatrench
