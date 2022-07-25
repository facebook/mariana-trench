/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <DexAccess.h>
#include <DexUtil.h>
#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

Method::Method(
    const DexMethod* method,
    ParameterTypeOverrides parameter_type_overrides)
    : method_(method),
      parameter_type_overrides_(std::move(parameter_type_overrides)),
      signature_(::show(method)),
      show_cached_(::show(this)) {
  mt_assert(method != nullptr);
}

bool Method::operator==(const Method& other) const {
  return method_ == other.method_ &&
      parameter_type_overrides_ == other.parameter_type_overrides_;
}

const IRCode* Method::get_code() const {
  return method_->get_code();
}

DexType* Method::get_class() const {
  return method_->get_class();
}

DexProto* Method::get_proto() const {
  return method_->get_proto();
}

std::string_view Method::get_name() const {
  return method_->get_name()->str();
}

DexAccessFlags Method::get_access() const {
  return method_->get_access();
}

bool Method::is_public() const {
  return ::is_public(method_);
}

bool Method::is_static() const {
  return ::is_static(method_);
}

bool Method::is_native() const {
  return ::is_native(method_);
}

bool Method::is_interface() const {
  return ::is_interface(method_);
}

bool Method::is_constructor() const {
  return method::is_init(method_);
}

bool Method::returns_void() const {
  return method_->get_proto()->get_rtype() == type::_void();
}

const std::string& Method::signature() const {
  return signature_;
}

const std::string& Method::show() const {
  return show_cached_;
}

ParameterPosition Method::number_of_parameters() const {
  return static_cast<ParameterPosition>(
             method_->get_proto()->get_args()->size()) +
      first_parameter_index();
}

DexType* MT_NULLABLE Method::parameter_type(ParameterPosition argument) const {
  const auto* dex_arguments = method_->get_proto()->get_args();

  // We treat "this/self" for instance methods as argument 0
  // This must be consistent with Method::number_of_parameters
  if (!is_static()) {
    if (argument == 0u) {
      return method_->get_class();
    } else {
      argument--;
    }
  }
  static_assert(std::is_unsigned_v<ParameterPosition>);
  return (argument < dex_arguments->size()) ? dex_arguments->at(argument)
                                            : nullptr;
}

DexType* Method::return_type() const {
  return method_->get_proto()->get_rtype();
}

ParameterPosition Method::first_parameter_index() const {
  return is_static() ? 0u : 1u;
}

const Method* Method::from_json(const Json::Value& value, Context& context) {
  if (value.isString()) {
    // Simpler form, less verbose.
    auto* dex_method = redex::get_method(value.asString());
    if (!dex_method) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          /* expected */ "existing method name");
    }
    return context.methods->create(dex_method);
  }

  if (!value.isObject()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "object or string");
  }

  auto method_name = JsonValidation::string(value, "name");

  auto* dex_method = redex::get_method(method_name);
  if (!dex_method) {
    throw JsonValidationError(
        value, /* field */ "name", /* expected */ "existing method name");
  }

  ParameterTypeOverrides parameter_type_overrides;
  for (auto parameter_type_override :
       JsonValidation::null_or_array(value, "parameter_type_overrides")) {
    auto parameter =
        JsonValidation::integer(parameter_type_override, "parameter");
    auto* type = JsonValidation::dex_type(parameter_type_override, "type");
    parameter_type_overrides.emplace(parameter, type);
  }

  return context.methods->create(dex_method, parameter_type_overrides);
}

Json::Value Method::to_json() const {
  if (parameter_type_overrides_.empty()) {
    // Use a simpler form to be less verbose.
    return Json::Value(signature_);
  }

  auto value = Json::Value(Json::objectValue);
  value["name"] = Json::Value(signature_);

  auto parameter_type_overrides = Json::Value(Json::arrayValue);
  for (auto [parameter, type] : parameter_type_overrides_) {
    auto parameter_type_override = Json::Value(Json::objectValue);
    parameter_type_override["parameter"] =
        Json::Value(static_cast<int>(parameter));
    parameter_type_override["type"] = Json::Value(::show(type));
    parameter_type_overrides.append(parameter_type_override);
  }
  value["parameter_type_overrides"] = parameter_type_overrides;

  return value;
}

std::string Method::show_control_flow_graph(const cfg::ControlFlowGraph& cfg) {
  std::string string;
  for (const auto* block : cfg.blocks()) {
    string.append(fmt::format("Block {}", block->id()));
    if (block == cfg.entry_block()) {
      string.append(" (entry)");
    }
    string.append(":\n");
    for (const auto& instruction : *block) {
      if (instruction.type == MFLOW_OPCODE) {
        string.append(fmt::format("  {}\n", ::show(instruction.insn)));
      }
    }
    const auto& successors = block->succs();
    if (!successors.empty()) {
      string.append("  Successors: {");
      for (auto iterator = successors.begin(), end = successors.end();
           iterator != end;) {
        string.append(fmt::format("{}", (*iterator)->target()->id()));
        ++iterator;
        if (iterator != end) {
          string.append(", ");
        }
      }
      string.append("}\n");
    }
  }
  return string;
}

std::ostream& operator<<(std::ostream& out, const Method& method) {
  out << method.signature_;
  if (!method.parameter_type_overrides_.empty()) {
    out << "[";
    for (auto iterator = method.parameter_type_overrides_.begin(),
              end = method.parameter_type_overrides_.end();
         iterator != end;) {
      out << iterator->first << ": " << show(iterator->second);
      ++iterator;
      if (iterator != end) {
        out << ",";
      }
    }
    out << "]";
  }
  return out;
}

} // namespace marianatrench
