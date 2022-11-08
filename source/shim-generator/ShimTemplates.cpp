/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>
#include <stdexcept>
#include <string>

#include <re2/re2.h>

#include <DexClass.h>
#include <TypeUtil.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/shim-generator/ShimTemplates.h>

namespace marianatrench {
namespace shim {
namespace {

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

  auto instantiated_parameter_map =
      target_template.parameter_map().instantiate_parameters(
          call_target->get_name(),
          call_target->get_class(),
          call_target->get_proto(),
          call_target->is_static(),
          shim_method);

  if (receiver_info.kind() == ReceiverInfo::Kind::INSTANCE ||
      receiver_info.kind() == ReceiverInfo::Kind::REFLECTION) {
    instantiated_parameter_map.add_receiver(
        std::get<ShimParameterPosition>(receiver_info.receiver()));
  }

  return ShimTarget(call_target, std::move(instantiated_parameter_map));
}

std::optional<ShimReflectionTarget> try_make_shim_reflection_target(
    const TargetTemplate& target_template,
    const ShimMethod& shim_method) {
  const auto& receiver_info = target_template.receiver_info();
  mt_assert(receiver_info.kind() == ReceiverInfo::Kind::REFLECTION);

  const auto* receiver_type = receiver_info.receiver_dex_type(shim_method);
  if (!receiver_type) {
    WARNING(
        1,
        "Shim method `{}` missing the receiver required for reflection shim callees`{}`.",
        shim_method.method()->show(),
        target_template.target());
    return std::nullopt;
  }

  auto method_spec = redex::get_method_spec(
      fmt::format("{}.{}", receiver_type->str(), target_template.target()));

  if (!method_spec) {
    WARNING(
        1,
        "Could not instantiate reflection shim target: `{}` specfied for method: `{}`",
        target_template,
        receiver_type->str(),
        shim_method.method()->show());
    return std::nullopt;
  }

  auto instantiated_parameter_map =
      target_template.parameter_map().instantiate_parameters(
          method_spec->name->str(),
          method_spec->cls,
          method_spec->proto,
          /* shim_target_is_static */ false,
          shim_method);

  instantiated_parameter_map.add_receiver(
      std::get<ShimParameterPosition>(receiver_info.receiver()));

  return ShimReflectionTarget(
      *method_spec, std::move(instantiated_parameter_map));
}

std::optional<ShimLifecycleTarget> try_make_shim_lifecycle_target(
    const TargetTemplate& target_template,
    const ShimMethod& shim_method) {
  const auto& receiver_info = target_template.receiver_info();
  mt_assert(
      receiver_info.kind() == ReceiverInfo::Kind::INSTANCE ||
      receiver_info.kind() == ReceiverInfo::Kind::REFLECTION);

  const auto* receiver_type = receiver_info.receiver_dex_type(shim_method);
  if (!receiver_type) {
    WARNING(
        1,
        "Shim method `{}` missing the receiver required for all shim callees`{}`.",
        shim_method.method()->show(),
        target_template.target());
    return std::nullopt;
  }

  return ShimLifecycleTarget(
      std::string(target_template.target()),
      std::get<ShimParameterPosition>(receiver_info.receiver()),
      receiver_info.kind() == ReceiverInfo::Kind::REFLECTION,
      target_template.parameter_map().infer_from_types());
}

} // namespace

ReceiverInfo::ReceiverInfo(Kind kind, ShimParameterPosition position)
    : kind_(kind), receiver_(position) {}

ReceiverInfo::ReceiverInfo(Kind kind, std::string type)
    : kind_(kind), receiver_(std::move(type)) {}

ReceiverInfo ReceiverInfo::from_json(const Json::Value& callee) {
  if (callee.isMember("static")) {
    return ReceiverInfo{Kind::STATIC, JsonValidation::string(callee, "static")};
  } else if (callee.isMember("type_of")) {
    return ReceiverInfo{
        Kind::INSTANCE,
        static_cast<ShimParameterPosition>(
            Root::from_json(JsonValidation::string(callee, "type_of"))
                .parameter_position())};
  } else if (callee.isMember("reflected_type_of")) {
    return ReceiverInfo{
        Kind::REFLECTION,
        static_cast<ShimParameterPosition>(
            Root::from_json(JsonValidation::string(callee, "reflected_type_of"))
                .parameter_position())};
  }

  throw JsonValidationError(
      callee,
      /* field */ std::nullopt,
      /* expected */
      "one of the keys:  static | type_of | reflected_type_of");
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
  const auto& parameters_map =
      JsonValidation::null_or_object(callee, "parameters_map");
  auto parameter_map = ShimParameterMapping::from_json(
      parameters_map,
      callee.isMember("infer_parameters_from_types")
          ? JsonValidation::boolean(callee, "infer_parameters_from_types")
          : parameters_map.isNull());
  auto receiver_info = ReceiverInfo::from_json(callee);

  if (callee.isMember("method_name")) {
    return TargetTemplate{
        receiver_info.kind() == ReceiverInfo::Kind::REFLECTION
            ? Kind::REFLECTION
            : Kind::DEFINED,
        JsonValidation::string(callee, "method_name"),
        std::move(receiver_info),
        std::move(parameter_map)};
  } else if (callee.isMember("lifecycle_name")) {
    return TargetTemplate{
        Kind::LIFECYCLE,
        JsonValidation::string(callee, "lifecycle_name"),
        std::move(receiver_info),
        std::move(parameter_map)};
  }

  throw JsonValidationError(
      callee,
      /* field */ "callees",
      /* expected */
      "each `callees` object to specify either `method_name` or `lifecycle_name`");
}

std::optional<ShimTargetVariant> TargetTemplate::instantiate(
    const Methods* methods,
    const ShimMethod& shim_method) const {
  switch (kind_) {
    case Kind::DEFINED:
      return try_make_shim_target(*this, methods, shim_method);

    case Kind::REFLECTION:
      return try_make_shim_reflection_target(*this, shim_method);

    case Kind::LIFECYCLE:
      return try_make_shim_lifecycle_target(*this, shim_method);
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
  auto shim = Shim(method_to_shim);

  for (const auto& target_template : targets_) {
    if (auto shim_target = target_template.instantiate(methods, shim_method)) {
      shim.add_target(*shim_target);
    }
  }

  if (shim.empty()) {
    return std::nullopt;
  }

  return shim;
}

} // namespace marianatrench
