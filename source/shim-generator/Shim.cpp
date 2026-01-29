/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>
#include <TypeUtil.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {
namespace {

static const InstantiatedShim::FlatSet<ShimTarget> empty_shim_targets;

static const InstantiatedShim::FlatSet<ShimLifecycleTarget>
    empty_lifecycle_targets;

static const InstantiatedShim::FlatSet<ShimReflectionTarget>
    empty_reflection_targets;

bool verify_has_parameter_type(
    std::string_view method_name,
    const DexType* dex_class,
    const DexProto* dex_proto,
    bool is_static,
    ParameterPosition position) {
  auto number_of_parameters =
      dex_proto->get_args()->size() + (is_static ? 0u : 1u);
  if (position >= number_of_parameters) {
    ERROR(
        1,
        "Parameter mapping for shim_target `{}.{}{}` contains a port on parameter {} but the method only has {} parameters.",
        show(dex_class),
        method_name,
        show(dex_proto),
        position,
        number_of_parameters);
    return false;
  }

  if (!is_static && position == 0u) {
    // `this` parameter position is always valid.
    return true;
  }

  return (
      dex_proto->get_args()->at(position - (is_static ? 0u : 1u)) != nullptr);
}

bool verify_to_return(
    std::string_view shim_target_method,
    const DexProto* shim_target_proto,
    bool shim_target_is_static,
    const ShimTargetPortMapping& instantiated_port_mapping,
    const ShimMethod& shim_method,
    ShimRoot return_to) {
  bool is_valid = true;

  if (shim_target_proto->get_rtype() == type::_void()) {
    ERROR(
        1,
        "return_to specified but shim target `{}` has void return type",
        shim_target_method);
    is_valid = false;
  }

  if (!shim_method.is_valid_port(return_to)) {
    ERROR(
        1,
        "return_to `{}` is not a valid port for shim method `{}`",
        return_to,
        shim_method.method()->show());

    is_valid = false;
  }

  if (!shim_target_is_static && return_to.is_argument()) {
    const auto receiver_port = instantiated_port_mapping.at(Root::argument(0));
    if (receiver_port && receiver_port->is_return()) {
      // This case is unusual but can be used to model a body of the
      // shimmed-method like:
      //
      // ```
      // ReturnedValue shimmed_method(arg1) {
      //  ret = new ReturnedValue();
      //  arg1 = ret.shimTargetReturnsASource();
      //  return ret;
      // }
      // ```
      WARNING(
          1,
          "Shim on Return port of `{}` specifies a return_to to `{}`. Verify that this is intentional.",
          shim_method.method()->show(),
          return_to);
    }
  }

  return is_valid;
}

std::unordered_map<Root, Register> get_root_registers(
    const IRInstruction* instruction,
    const ShimTargetPortMapping& port_mapping) {
  std::unordered_map<Root, Register> root_registers;

  for (const auto& [root, shimmed_method_root] : port_mapping) {
    if (shimmed_method_root.is_return()) {
      mt_assert_log(
          root.is_argument() && root.parameter_position() == 0,
          "Return port can only be receiver");
      root_registers.emplace(root, k_result_register);
    } else {
      auto shim_parameter_position = shimmed_method_root.parameter_position();
      mt_assert(shim_parameter_position < instruction->srcs_size());
      root_registers.emplace(root, instruction->src(shim_parameter_position));
    }
  }

  return root_registers;
}

std::optional<Register> get_return_to_register(
    const IRInstruction* instruction,
    const ShimTargetPortMapping& port_mapping) {
  const auto& return_to = port_mapping.return_to();
  if (!return_to) {
    return std::nullopt;
  }

  if (return_to->is_return()) {
    return k_result_register;
  }

  auto parameter_position = return_to->parameter_position();
  mt_assert_log(
      parameter_position < instruction->srcs_size(),
      "Invalid return_to parameter position");

  return instruction->src(parameter_position);
}

} // namespace

ShimMethod::ShimMethod(const Method* method) : method_(method) {
  ParameterPosition index = 0;

  if (!method_->is_static()) {
    // Include `this` as argument 0
    types_to_position_.emplace(method->get_class(), Root::argument(index++));
  }

  const auto* dex_arguments = method_->get_proto()->get_args();
  if (!dex_arguments) {
    return;
  }

  for (auto* dex_argument : *dex_arguments) {
    types_to_position_.emplace(dex_argument, Root::argument(index++));
  }
}

const Method* ShimMethod::method() const {
  return method_;
}

const DexType* MT_NULLABLE ShimMethod::parameter_type(ShimRoot argument) const {
  return method_->parameter_type(argument.parameter_position());
}

DexType* MT_NULLABLE ShimMethod::return_type() const {
  return method_->return_type();
}

bool ShimMethod::is_valid_port(ShimRoot port) const {
  if (port.is_argument()) {
    return parameter_type(port) != nullptr;
  } else if (port.is_return()) {
    return return_type() != nullptr;
  }
  return false;
}

std::optional<ShimRoot> ShimMethod::type_position(
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

ShimTargetPortMapping::ShimTargetPortMapping(
    std::initializer_list<MapType::value_type> init)
    : map_(init), infer_from_types_(false), return_to_(std::nullopt) {}

bool ShimTargetPortMapping::operator==(
    const ShimTargetPortMapping& other) const {
  return infer_from_types_ == other.infer_from_types_ && map_ == other.map_ &&
      return_to_ == other.return_to_;
}

bool ShimTargetPortMapping::operator<(
    const ShimTargetPortMapping& other) const {
  // Lexicographic comparison
  return std::tie(infer_from_types_, map_, return_to_) <
      std::tie(other.infer_from_types_, other.map_, other.return_to_);
}

bool ShimTargetPortMapping::empty() const {
  return map_.empty();
}

bool ShimTargetPortMapping::contains(Root position) const {
  return map_.count(position) > 0;
}

std::optional<ShimRoot> ShimTargetPortMapping::at(
    Root parameter_position) const {
  auto found = map_.find(parameter_position);
  if (found == map_.end()) {
    return std::nullopt;
  }

  return found->second;
}

void ShimTargetPortMapping::insert(
    Root parameter_position,
    ShimRoot shim_parameter_position) {
  map_.insert({parameter_position, shim_parameter_position});
}

void ShimTargetPortMapping::set_infer_from_types(bool value) {
  infer_from_types_ = value;
}

bool ShimTargetPortMapping::infer_from_types() const {
  return infer_from_types_;
}

void ShimTargetPortMapping::add_receiver(ShimRoot shim_parameter_position) {
  // Include `this` as argument 0
  insert(Root::argument(0), shim_parameter_position);
}

void ShimTargetPortMapping::remove_receiver() {
  // Remove `this` as argument 0
  map_.erase(Root::argument(0));
}

void ShimTargetPortMapping::infer_parameters_from_types(
    const DexProto* shim_target_proto,
    bool shim_target_is_static,
    const ShimMethod& shim_method) {
  const auto* dex_arguments = shim_target_proto->get_args();
  if (!dex_arguments) {
    return;
  }

  ParameterPosition first_parameter_position = shim_target_is_static ? 0 : 1;

  for (std::size_t position = 0; position < dex_arguments->size(); position++) {
    if (auto shim_position =
            shim_method.type_position(dex_arguments->at(position))) {
      insert(
          Root::argument(
              static_cast<ParameterPosition>(
                  position + first_parameter_position)),
          *shim_position);
    }
  }
}

const std::optional<ShimRoot>& ShimTargetPortMapping::return_to() const {
  return return_to_;
}

void ShimTargetPortMapping::set_return_to(std::optional<ShimRoot> return_to) {
  return_to_ = return_to;
}

ShimTargetPortMapping ShimTargetPortMapping::from_json(
    const Json::Value& value,
    bool infer_from_types,
    std::optional<ShimRoot> return_to) {
  ShimTargetPortMapping port_mapping;
  port_mapping.set_infer_from_types(infer_from_types);
  port_mapping.set_return_to(return_to);

  if (value.isNull()) {
    return port_mapping;
  }

  JsonValidation::validate_object(value);

  for (auto item = value.begin(); item != value.end(); ++item) {
    auto parameter_argument = JsonValidation::string(item.key());
    auto shim_argument = JsonValidation::string(*item);
    port_mapping.insert(
        Root::from_json(parameter_argument), Root::from_json(shim_argument));
  }

  return port_mapping;
}

ShimTargetPortMapping ShimTargetPortMapping::instantiate(
    std::string_view shim_target_method,
    const DexType* shim_target_class,
    const DexProto* shim_target_proto,
    bool shim_target_is_static,
    const ShimMethod& shim_method) const {
  ShimTargetPortMapping instantiated_port_mapping;
  instantiated_port_mapping.set_infer_from_types(infer_from_types());

  for (const auto& [shim_target_position, shim_position] : map_) {
    if (shim_target_position.is_argument() &&
        !verify_has_parameter_type(
            shim_target_method,
            shim_target_class,
            shim_target_proto,
            shim_target_is_static,
            shim_target_position.parameter_position())) {
      continue;
    }

    instantiated_port_mapping.insert(shim_target_position, shim_position);
  }

  if (infer_from_types()) {
    instantiated_port_mapping.infer_parameters_from_types(
        shim_target_proto, shim_target_is_static, shim_method);
  }

  // Validate and set return_to
  if (return_to_.has_value() &&
      verify_to_return(
          shim_target_method,
          shim_target_proto,
          shim_target_is_static,
          instantiated_port_mapping,
          shim_method,
          *return_to_)) {
    instantiated_port_mapping.set_return_to(return_to_);
  }

  return instantiated_port_mapping;
}

ShimTarget::ShimTarget(
    DexMethodSpec method_spec,
    ShimTargetPortMapping port_mapping,
    bool is_static)
    : method_spec_(std::move(method_spec)),
      port_mapping_(std::move(port_mapping)),
      is_static_(is_static) {
  mt_assert(
      method_spec_.cls != nullptr && method_spec_.name != nullptr &&
      method_spec_.proto != nullptr);
}

ShimTarget::ShimTarget(const Method* method, ShimTargetPortMapping port_mapping)
    : ShimTarget(
          DexMethodSpec{
              method->get_class(),
              DexString::get_string(method->get_name()),
              method->get_proto()},
          std::move(port_mapping),
          method->is_static()) {}

bool ShimTarget::operator==(const ShimTarget& other) const {
  return is_static_ == other.is_static_ && method_spec_ == other.method_spec_ &&
      port_mapping_ == other.port_mapping_;
}

bool ShimTarget::operator<(const ShimTarget& other) const {
  // Lexicographic comparison
  return std::tie(
             is_static_,
             method_spec_.cls,
             method_spec_.name,
             method_spec_.proto,
             port_mapping_) <
      std::tie(
             other.is_static_,
             other.method_spec_.cls,
             other.method_spec_.name,
             other.method_spec_.proto,
             other.port_mapping_);
}

std::optional<Register> ShimTarget::receiver_register(
    const IRInstruction* instruction) const {
  if (is_static_) {
    return std::nullopt;
  }

  auto receiver_position = port_mapping_.at(Root::argument(0));
  if (!receiver_position) {
    return std::nullopt;
  }

  // Return value is stored in the special k_result_register
  if (receiver_position->is_return()) {
    return k_result_register;
  } else {
    auto receiver_parameter_position = receiver_position->parameter_position();
    mt_assert(receiver_parameter_position < instruction->srcs_size());
    return instruction->src(receiver_parameter_position);
  }
}

std::unordered_map<Root, Register> ShimTarget::root_registers(
    const IRInstruction* instruction) const {
  return get_root_registers(instruction, port_mapping_);
}

std::optional<Register> ShimTarget::return_to_register(
    const IRInstruction* instruction) const {
  return get_return_to_register(instruction, port_mapping_);
}

ShimReflectionTarget::ShimReflectionTarget(
    DexMethodSpec method_spec,
    ShimTargetPortMapping port_mapping)
    : method_spec_(method_spec),
      port_mapping_(std::move(port_mapping)),
      is_resolved_(false) {
  mt_assert(
      method_spec_.cls == type::java_lang_Class() &&
      method_spec_.name != nullptr && method_spec_.proto != nullptr);
  mt_assert_log(
      port_mapping_.contains(Root::argument(0)),
      "Missing parameter mapping for receiver for reflection shim target");
}

ShimReflectionTarget::ShimReflectionTarget(
    const Method* resolved_reflection_method,
    ShimTargetPortMapping instantiated_port_mapping) {
  mt_assert(!resolved_reflection_method->is_static());
  mt_assert(!instantiated_port_mapping.contains(Root::argument(0)));

  method_spec_ = DexMethodSpec{
      resolved_reflection_method->get_class(),
      DexString::get_string(resolved_reflection_method->get_name()),
      resolved_reflection_method->get_proto()};
  port_mapping_ = std::move(instantiated_port_mapping);
  is_resolved_ = true;
}

bool ShimReflectionTarget::operator==(const ShimReflectionTarget& other) const {
  return method_spec_ == other.method_spec_ &&
      port_mapping_ == other.port_mapping_ &&
      is_resolved_ == other.is_resolved_;
}

bool ShimReflectionTarget::operator<(const ShimReflectionTarget& other) const {
  // Lexicographic comparison
  return std::tie(
             method_spec_.cls,
             method_spec_.name,
             method_spec_.proto,
             port_mapping_,
             is_resolved_) <
      std::tie(
             other.method_spec_.cls,
             other.method_spec_.name,
             other.method_spec_.proto,
             other.port_mapping_,
             other.is_resolved_);
}

Register ShimReflectionTarget::receiver_register(
    const IRInstruction* instruction) const {
  auto receiver_parameter_position =
      port_mapping_.at(Root::argument(0))->parameter_position();
  mt_assert(receiver_parameter_position < instruction->srcs_size());

  return instruction->src(receiver_parameter_position);
}

ShimReflectionTarget ShimReflectionTarget::resolve(
    const Method* shimmed_callee,
    const Method* resolved_reflection) const {
  ShimMethod shimmed_method{shimmed_callee};

  auto instantiated_port_mapping = port_mapping_.instantiate(
      resolved_reflection->get_name(),
      resolved_reflection->get_class(),
      resolved_reflection->get_proto(),
      resolved_reflection->is_static(),
      shimmed_method);

  // For reflection receivers, do not propagate the `this` argument, as it
  // is always a new instance.
  instantiated_port_mapping.remove_receiver();

  return ShimReflectionTarget{
      resolved_reflection, std::move(instantiated_port_mapping)};
}

std::unordered_map<Root, Register> ShimReflectionTarget::root_registers(
    const IRInstruction* instruction) const {
  mt_assert(is_resolved_);
  return get_root_registers(instruction, port_mapping_);
}

std::optional<Register> ShimReflectionTarget::return_to_register(
    const IRInstruction* instruction) const {
  mt_assert(is_resolved_);
  return get_return_to_register(instruction, port_mapping_);
}

ShimLifecycleTarget::ShimLifecycleTarget(
    std::string method_name,
    ShimTargetPortMapping port_mapping,
    bool is_reflection)
    : method_name_(std::move(method_name)),
      port_mapping_(std::move(port_mapping)),
      is_reflection_(is_reflection),
      is_resolved_(false) {}

ShimLifecycleTarget::ShimLifecycleTarget(
    const Method* lifecycle_method,
    ShimTargetPortMapping instantiated_port_mapping,
    bool is_reflection)
    : method_name_(lifecycle_method->get_name()),
      port_mapping_(std::move(instantiated_port_mapping)),
      is_reflection_(is_reflection),
      is_resolved_(true) {}

ShimLifecycleTarget ShimLifecycleTarget::resolve(
    const Method* shimmed_callee,
    const Method* lifecycle_method) const {
  ShimMethod shimmed_method{shimmed_callee};

  auto receiver_position = port_mapping_.at(Root::argument(0));
  mt_assert_log(
      receiver_position.has_value(),
      "Missing receiver position in unresolved ShimLifecycleTarget");

  auto instantiated_port_mapping = port_mapping_.instantiate(
      lifecycle_method->get_name(),
      lifecycle_method->get_class(),
      lifecycle_method->get_proto(),
      lifecycle_method->is_static(),
      shimmed_method);

  // For reflection receivers, do not propagate the `this` argument, as it
  // is always a new instance.
  if (is_reflection_) {
    instantiated_port_mapping.remove_receiver();
  }

  return ShimLifecycleTarget{
      lifecycle_method, std::move(instantiated_port_mapping), is_reflection_};
}

bool ShimLifecycleTarget::operator==(const ShimLifecycleTarget& other) const {
  return is_resolved_ == other.is_resolved_ &&
      is_reflection_ == other.is_reflection_ &&
      method_name_ == other.method_name_ &&
      port_mapping_ == other.port_mapping_;
}

bool ShimLifecycleTarget::operator<(const ShimLifecycleTarget& other) const {
  return std::tie(is_resolved_, is_reflection_, method_name_, port_mapping_) <
      std::tie(
             other.is_resolved_,
             other.is_reflection_,
             other.method_name_,
             other.port_mapping_);
}

Register ShimLifecycleTarget::receiver_register(
    const IRInstruction* instruction) const {
  auto receiver_position = port_mapping_.at(Root::argument(0));
  mt_assert_log(
      receiver_position.has_value(),
      "Missing receiver position in ShimLifecycleTarget");

  // Return value is stored in the special k_result_register
  if (receiver_position->is_return()) {
    return k_result_register;
  }

  auto receiver_parameter_position = receiver_position->parameter_position();
  mt_assert(receiver_parameter_position < instruction->srcs_size());
  return instruction->src(receiver_parameter_position);
}

std::unordered_map<Root, Register> ShimLifecycleTarget::root_registers(
    const IRInstruction* instruction) const {
  mt_assert(is_resolved_);
  return get_root_registers(instruction, port_mapping_);
}

InstantiatedShim::InstantiatedShim(const Method* method) : method_(method) {}

void InstantiatedShim::add_target(ShimTargetVariant target) {
  if (std::holds_alternative<ShimTarget>(target)) {
    targets_.emplace(std::get<ShimTarget>(target));
  } else if (std::holds_alternative<ShimReflectionTarget>(target)) {
    reflections_.emplace(std::get<ShimReflectionTarget>(target));
  } else if (std::holds_alternative<ShimLifecycleTarget>(target)) {
    lifecycles_.emplace(std::get<ShimLifecycleTarget>(target));
  } else {
    mt_unreachable();
  }
}

void InstantiatedShim::merge_with(InstantiatedShim other) {
  targets_.insert(
      std::make_move_iterator(other.targets_.begin()),
      std::make_move_iterator(other.targets_.end()));
  reflections_.insert(
      std::make_move_iterator(other.reflections_.begin()),
      std::make_move_iterator(other.reflections_.end()));
  lifecycles_.insert(
      std::make_move_iterator(other.lifecycles_.begin()),
      std::make_move_iterator(other.lifecycles_.end()));
}

Shim::Shim(
    const InstantiatedShim* MT_NULLABLE instantiated_shim,
    InstantiatedShim::FlatSet<ShimTarget> intent_routing_targets)
    : instantiated_shim_(instantiated_shim),
      intent_routing_targets_(std::move(intent_routing_targets)) {}

const InstantiatedShim::FlatSet<ShimTarget>& Shim::targets() const {
  if (instantiated_shim_ == nullptr) {
    return empty_shim_targets;
  }
  return instantiated_shim_->targets();
}

const InstantiatedShim::FlatSet<ShimReflectionTarget>& Shim::reflections()
    const {
  if (instantiated_shim_ == nullptr) {
    return empty_reflection_targets;
  }
  return instantiated_shim_->reflections();
}

const InstantiatedShim::FlatSet<ShimLifecycleTarget>& Shim::lifecycles() const {
  if (instantiated_shim_ == nullptr) {
    return empty_lifecycle_targets;
  }
  return instantiated_shim_->lifecycles();
}

std::ostream& operator<<(std::ostream& out, const ShimMethod& shim_method) {
  out << "ShimMethod(method=`";
  if (shim_method.method_ != nullptr) {
    out << shim_method.method_->show();
  }
  out << "`)";
  return out;
}

std::ostream& operator<<(std::ostream& out, const ShimTargetPortMapping& map) {
  out << "infer_from_types=`";
  out << (map.infer_from_types() ? "true" : "false") << "`, ";
  out << "parameters_map={";
  for (const auto& [parameter, shim_parameter] : map.map_) {
    out << " " << parameter << ":";
    out << " " << shim_parameter << ",";
  }
  out << " }";
  if (map.return_to_.has_value()) {
    out << ", return_to=`" << *map.return_to_ << "`";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const ShimTarget& shim_target) {
  out << "ShimTarget(type=`";
  out << show(shim_target.method_spec_.cls);
  out << "`, method_name=`";
  out << show(shim_target.method_spec_.name);
  out << "`, proto=`";
  out << show(shim_target.method_spec_.proto);
  out << "`";
  out << ", " << shim_target.port_mapping_;
  return out << ")";
}

std::ostream& operator<<(
    std::ostream& out,
    const ShimReflectionTarget& shim_reflection_target) {
  out << "ShimReflectionTarget(method_name=`";
  out << show(shim_reflection_target.method_spec_.name);
  out << "`, proto=`";
  out << show(shim_reflection_target.method_spec_.proto);
  out << "`, " << shim_reflection_target.port_mapping_;
  out << ", is_resolved=`" << shim_reflection_target.is_resolved_;
  return out << "`)";
}

std::ostream& operator<<(
    std::ostream& out,
    const ShimLifecycleTarget& shim_lifecycle_target) {
  out << "ShimLifecycleTarget(method_name=`";
  out << shim_lifecycle_target.method_name_;
  out << "`, is_reflection=`";
  out << (shim_lifecycle_target.is_reflection_ ? "true" : "false") << "`";
  out << ", is_resolved=`"
      << (shim_lifecycle_target.is_resolved_ ? "true" : "false") << "`";
  out << ", " << shim_lifecycle_target.port_mapping_;
  return out << ")";
}

std::ostream& operator<<(std::ostream& out, const InstantiatedShim& shim) {
  out << "InstantiatedShim(method=`";
  if (auto* method = shim.method()) {
    out << method->show();
  }
  out << "`";

  if (!shim.targets().empty()) {
    out << ",\n  targets=[\n";
    for (const auto& target : shim.targets()) {
      out << "    " << target << ",\n";
    }
    out << "  ]";
  }

  if (!shim.reflections().empty()) {
    out << ",\n  reflections=[\n";
    for (const auto& target : shim.reflections()) {
      out << "    " << target << ",\n";
    }
    out << "  ]";
  }

  if (!shim.lifecycles().empty()) {
    out << ",\n  lifecycles=[\n";
    for (const auto& target : shim.lifecycles()) {
      out << "    " << target << ",\n";
    }
    out << "  ]";
  }

  return out << ")";
}

std::ostream& operator<<(std::ostream& out, const Shim& shim) {
  out << "Shim(shim=`";
  if (shim.instantiated_shim_) {
    out << *(shim.instantiated_shim_);
  }
  out << "`";

  if (!shim.intent_routing_targets().empty()) {
    out << ",\n  intent_routing_targets=[\n";
    for (const auto& target : shim.intent_routing_targets()) {
      out << "    " << target << ",\n";
    }
    out << "  ]";
  }
  return out << ")";
}

} // namespace marianatrench
