/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <re2/re2.h>

#include <sparta/WorkQueue.h>

#include <GraphUtil.h>
#include <IRInstruction.h>
#include <Resolver.h>
#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KotlinHeuristics.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

namespace {

bool is_static_invoke(const IRInstruction* instruction) {
  return opcode::is_invoke_static(instruction->opcode());
}

bool is_virtual_invoke(const IRInstruction* instruction) {
  switch (instruction->opcode()) {
    case OPCODE_INVOKE_VIRTUAL:
    case OPCODE_INVOKE_INTERFACE:
      return true;
    default:
      return false;
  }
}

/* Return the resolved base callee. */
const DexMethod* MT_NULLABLE resolve_call(
    const Types& types,
    const Method* caller,
    const IRInstruction* instruction) {
  mt_assert(caller != nullptr);
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  DexMethodRef* dex_method_reference = instruction->get_method();
  mt_assert_log(
      dex_method_reference != nullptr,
      "invoke instruction has no method reference");

  DexMethod* method = nullptr;
  switch (instruction->opcode()) {
    case OPCODE_INVOKE_DIRECT:
    case OPCODE_INVOKE_STATIC:
    case OPCODE_INVOKE_SUPER: {
      // No need to consider the runtime type.
      method = resolve_method_deprecated(
          dex_method_reference,
          opcode_to_search(instruction->opcode()),
          caller->dex_method());
      break;
    }
    case OPCODE_INVOKE_VIRTUAL:
    case OPCODE_INVOKE_INTERFACE: {
      // Use the inferred runtime type to refine the call.
      const DexType* type = types.receiver_type(caller, instruction);
      const DexClass* klass = type ? type_class(type) : nullptr;
      if (!klass) {
        method = resolve_method_deprecated(
            dex_method_reference, MethodSearch::Virtual);
      } else {
        method = resolve_method_deprecated(
            klass,
            dex_method_reference->get_name(),
            dex_method_reference->get_proto(),
            MethodSearch::Virtual);
      }

      if (!method) {
        // `MethodSearch::Virtual` returns null for interface methods.
        method = resolve_method_deprecated(
            dex_method_reference, MethodSearch::InterfaceVirtual);
      }
      break;
    }
    default:
      mt_assert_log(false, "unexpected opcode");
  }

  return method;
}

const DexField* MT_NULLABLE
resolve_field_access(const Method* caller, const IRInstruction* instruction) {
  mt_assert(caller != nullptr);
  mt_assert(
      opcode::is_an_iget(instruction->opcode()) ||
      opcode::is_an_sget(instruction->opcode()) ||
      opcode::is_an_iput(instruction->opcode()) ||
      opcode::is_an_sput(instruction->opcode()));

  const DexFieldRef* dex_field_reference = instruction->get_field();
  mt_assert_log(
      dex_field_reference != nullptr,
      "Field access (iget, sget, iput) instruction has no field reference");

  if (opcode::is_an_sget(instruction->opcode()) ||
      opcode::is_an_sput(instruction->opcode())) {
    return resolve_field(dex_field_reference, FieldSearch::Static);
  }
  return resolve_field(dex_field_reference, FieldSearch::Instance);
}

bool is_anonymous_class(const DexType* type) {
  static constexpr std::array<std::string_view, 2> patterns = {
      // https://r8.googlesource.com/r8/+/refs/tags/8.9.31/src/main/java/com/android/tools/r8/synthesis/SyntheticNaming.java#419
      "$$ExternalSyntheticLambda",
      // Desugared lambda classes from older versions of D8.
      "$$Lambda$",
  };

  auto type_name = type->str();
  auto pos = type_name.rfind('$');
  if (pos == std::string::npos) {
    return false;
  }

  pos++;
  return (pos < type_name.size() && type_name[pos] >= '0' &&
          type_name[pos] <= '9') ||
      std::any_of(
             patterns.begin(),
             patterns.end(),
             [&type_name](const std::string_view& pattern) {
               return type_name.find(pattern) != std::string::npos;
             });
}

using TextualOrderIndex = marianatrench::TextualOrderIndex;

struct InstructionCallGraphInformation {
  std::optional<CallTarget> callee;
  ArtificialCallees artificial_callees = {};
  std::optional<FieldTarget> field_access;
};

TextualOrderIndex update_index(
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    const std::string& sink_signature) {
  auto lookup = sink_textual_order_index.find(sink_signature);
  if (lookup == sink_textual_order_index.end()) {
    sink_textual_order_index.emplace(std::make_pair(sink_signature, 0));
    return 0;
  }
  auto new_count = lookup->second + 1;
  sink_textual_order_index.insert_or_assign(sink_signature, new_count);
  return new_count;
}

// Return mapping of argument index to argument type for all anonymous class
// arguments.
ParameterTypeOverrides anonymous_class_arguments(
    const Types& types,
    const Method* caller,
    const IRInstruction* instruction,
    const DexMethod* callee) {
  mt_assert(callee != nullptr);
  ParameterTypeOverrides parameters;
  const auto& environment = types.environment(caller, instruction);
  for (std::size_t source_position = 0, sources_size = instruction->srcs_size();
       source_position < sources_size;
       source_position++) {
    auto parameter_position = source_position;
    if (!is_static(callee)) {
      if (source_position == 0) {
        // Do not override `this`.
        continue;
      } else {
        // Do not count the `this` parameter
        parameter_position--;
      }
    }
    auto found = environment.find(instruction->src(source_position));
    if (found == environment.end()) {
      continue;
    }
    const auto* type = found->second.singleton_type();
    if (type && is_anonymous_class(type)) {
      parameters.emplace(parameter_position, type);
    }
  }
  return parameters;
}

ArtificialCallees anonymous_class_artificial_callees(
    const Methods& method_factory,
    const IRInstruction* instruction,
    const DexType* anonymous_class_type,
    Register register_id,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    const FeatureSet& features = {}) {
  if (!is_anonymous_class(anonymous_class_type)) {
    return {};
  }

  const DexClass* anonymous_class = type_class(anonymous_class_type);

  if (!anonymous_class) {
    return {};
  }

  ArtificialCallees callees;

  for (const auto* dex_method : anonymous_class->get_vmethods()) {
    const auto* method = method_factory.get(dex_method);
    mt_assert(!method->is_constructor());
    mt_assert(!method->is_static());

    auto call_index =
        update_index(sink_textual_order_index, method->signature());
    callees.push_back(
        ArtificialCallee{
            /* call_target */
            CallTarget::direct_call(
                instruction,
                method,
                anonymous_class_type,
                CallTarget::CallKind::AnonymousClass,
                call_index),
            /* root_registers */
            {{Root(Root::Kind::Argument, 0), register_id}},
            /* features */ features,
        });
  }

  return callees;
}

ArtificialCallees artificial_callees_from_arguments(
    const Methods& method_factory,
    const FeatureFactory& feature_factory,
    const IRInstruction* instruction,
    const DexMethod* callee,
    const ParameterTypeOverrides& parameter_type_overrides,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index) {
  ArtificialCallees callees;

  // For each anonymous class parameter, simulate calls to all its methods.
  for (auto [parameter, anonymous_class_type] : parameter_type_overrides) {
    auto artificial_callees_from_parameter = anonymous_class_artificial_callees(
        method_factory,
        instruction,
        anonymous_class_type,
        /* register */
        instruction->src(parameter + (is_static(callee) ? 0 : 1)),
        sink_textual_order_index,
        /* features */
        FeatureSet{feature_factory.get("via-anonymous-class-to-obscure")});
    callees.insert(
        callees.end(),
        std::make_move_iterator(artificial_callees_from_parameter.begin()),
        std::make_move_iterator(artificial_callees_from_parameter.end()));
  }

  return callees;
}

/*
 * Given the DexMethod representing the callee of an instruction, get or create
 * the Method corresponding to the call
 */
const Method* get_callee_from_resolved_call(
    const DexMethod* dex_callee,
    const IRInstruction* instruction,
    const ParameterTypeOverrides& parameter_type_overrides,
    const Options& options,
    const FeatureFactory& feature_factory,
    Methods& method_factory,
    MethodMappings& method_mappings,
    ArtificialCallees& artificial_callees,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index) {
  const Method* callee = nullptr;
  if (dex_callee->get_code() == nullptr) {
    // When passing an anonymous class into an external callee (no code), add
    // artificial calls to all methods of the anonymous class.
    auto artificial_callees_for_instruction = artificial_callees_from_arguments(
        method_factory,
        feature_factory,
        instruction,
        dex_callee,
        parameter_type_overrides,
        sink_textual_order_index);
    if (!artificial_callees_for_instruction.empty()) {
      artificial_callees = std::move(artificial_callees_for_instruction);
    }

    // No need to use type overrides since we don't have the code.
    callee = method_factory.get(dex_callee);
  } else if (
      options.disable_parameter_type_overrides() ||
      KotlinHeuristics::skip_parameter_type_overrides(dex_callee)) {
    callee = method_factory.get(dex_callee);
  } else {
    // Analyze the callee with these particular types.
    callee = method_factory.create(dex_callee, parameter_type_overrides);
    method_mappings.create_mappings_for_method(callee);
  }
  mt_assert(callee != nullptr);
  return callee;
}

void process_shim_target(
    const Method* caller,
    const Method* callee,
    const ShimTarget& shim_target,
    const IRInstruction* instruction,
    CallTarget::CallKind call_kind,
    const Methods& method_factory,
    const Types& types,
    const Overrides& override_factory,
    const ClassHierarchies& class_hierarchies,
    const FeatureFactory& feature_factory,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    std::vector<ArtificialCallee>& artificial_callees,
    const FeatureSet& extra_features) {
  const auto& method_spec = shim_target.method_spec();

  const DexType* receiver_type = nullptr;
  const std::unordered_set<const DexType*>* receiver_local_extends = nullptr;
  if (auto receiver_register = shim_target.receiver_register(instruction)) {
    receiver_type =
        types.register_type(caller, instruction, *receiver_register);
    receiver_local_extends =
        &types.register_local_extends(caller, instruction, *receiver_register);
  }
  if (receiver_type == nullptr) {
    receiver_type = method_spec.cls;
  }

  const auto* dex_method = resolve_method_deprecated(
      type_class(receiver_type),
      method_spec.name,
      method_spec.proto,
      MethodSearch::Any);

  if (!dex_method) {
    EventLogger::log_event(
        "shim_method_not_found",
        fmt::format(
            "Could not resolve method for shim target: {} at instruction {} in caller: {}",
            shim_target,
            show(instruction),
            caller->show()),
        /* verbosity_level */ 1);
    return;
  }

  const auto* method = method_factory.get(dex_method);
  auto root_registers = shim_target.root_registers(instruction);

  auto call_index = update_index(sink_textual_order_index, method->signature());
  auto features = FeatureSet{feature_factory.get_via_shim_feature(callee)}.join(
      extra_features);

  if (method->is_static()) {
    mt_assert(shim_target.is_static());
    artificial_callees.push_back(
        ArtificialCallee{
            /* call_target */
            CallTarget::static_call(instruction, method, call_kind, call_index),
            /* root_registers */ root_registers,
            /* features */ features,
        });
    return;
  }

  artificial_callees.push_back(
      ArtificialCallee{
          /* call_target */
          CallTarget::virtual_call(
              instruction,
              method,
              receiver_type,
              receiver_local_extends,
              class_hierarchies,
              override_factory,
              call_kind,
              call_index),
          /* root_registers */ root_registers,
          /* features */ features,
      });
}

void process_shim_reflection(
    const Method* caller,
    const Method* callee,
    const ShimReflectionTarget& shim_reflection,
    const IRInstruction* instruction,
    const Methods& method_factory,
    const Types& types,
    const Overrides& override_factory,
    const ClassHierarchies& class_hierarchies,
    const FeatureFactory& FeatureFactory,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    std::vector<ArtificialCallee>& artificial_callees) {
  const auto& method_spec = shim_reflection.method_spec();
  const auto* reflection_type = types.register_const_class_type(
      caller, instruction, shim_reflection.receiver_register(instruction));

  if (reflection_type == nullptr) {
    EventLogger::log_event(
        "shim_reflection_type_resolution_failure",
        fmt::format(
            "Could not resolve receiver type for shim reflection target: {} at instruction: {} in caller: {}",
            shim_reflection,
            show(instruction),
            caller->show()),
        /* verbosity_level */ 1);
    return;
  }

  auto dex_reflection_method = resolve_method_deprecated(
      type_class(reflection_type),
      method_spec.name,
      method_spec.proto,
      MethodSearch::Any);

  if (!dex_reflection_method) {
    EventLogger::log_event(
        "shim_reflection_method_not_found",
        fmt::format(
            "Could not resolve method for shim reflection target: {} at instruction {} in caller: {}",
            shim_reflection,
            show(instruction),
            caller->show()),
        /* verbosity_level */ 1);
    return;
  }

  const auto* reflection_method = method_factory.get(dex_reflection_method);
  auto root_registers =
      shim_reflection.root_registers(reflection_method, instruction);
  auto call_index =
      update_index(sink_textual_order_index, reflection_method->signature());

  artificial_callees.push_back(
      ArtificialCallee{
          /* call_target */
          CallTarget::virtual_call(
              instruction,
              reflection_method,
              reflection_type,
              /* receiver_local_extends */ nullptr,
              class_hierarchies,
              override_factory,
              CallTarget::CallKind::Shim,
              call_index),
          /* root_registers */ root_registers,
          /* features */
          FeatureSet{FeatureFactory.get_via_shim_feature(callee)},
      });
}

void process_shim_lifecycle(
    const Method* caller,
    const Method* callee,
    const ShimLifecycleTarget& shim_lifecycle,
    const IRInstruction* instruction,
    const Types& types,
    const LifecycleMethods& lifecycle_methods,
    const ClassHierarchies& class_hierarchies,
    const FeatureFactory& feature_factory,
    const Heuristics& heuristics,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    std::vector<ArtificialCallee>& artificial_callees) {
  const auto& method_name = shim_lifecycle.method_name();
  auto receiver_register = shim_lifecycle.receiver_register(instruction);

  const auto* receiver_type = shim_lifecycle.is_reflection()
      ? types.register_const_class_type(caller, instruction, receiver_register)
      : types.register_type(caller, instruction, receiver_register);
  if (receiver_type == nullptr) {
    EventLogger::log_event(
        "shim_lifecycle_receiver_type_resolution_failure",
        fmt::format(
            "Could not resolve receiver type for shim lifecycle target: {} at instruction: {} in caller: {}",
            shim_lifecycle,
            show(instruction),
            caller->show()),
        /* verbosity_level */ 1);
    return;
  }

  auto result = lifecycle_methods.methods().find(method_name);
  if (result == lifecycle_methods.methods().end()) {
    // This indicates an error in the user configuration, e.g. incorrect method
    // name, or not providing life-cycles JSON, etc.
    EventLogger::log_event(
        "shim_lifecycle_method_not_found",
        fmt::format("Specified lifecycle method not found: `{}`", method_name),
        /* verbosity_level */ 1);
    return;
  }

  const auto& local_extends =
      types.register_local_extends(caller, instruction, receiver_register);
  std::vector<std::string> local_extends_string;
  for (const auto& local_extends_type : local_extends) {
    local_extends_string.push_back(show(local_extends_type));
  }
  const auto types_logging = fmt::format(
      "Receiver type: `{}`, Local extends: {}",
      show(receiver_type),
      boost::join(local_extends_string, ","));

  auto target_lifecycle_methods = result->second.get_methods_for_type(
      receiver_type, local_extends, class_hierarchies);
  if (target_lifecycle_methods.size() == 0) {
    EventLogger::log_event(
        "shim_lifecycle_target_method_not_found",
        fmt::format(
            "Could not resolve any method for shim lifecycle target: `{}` at instruction: `{}` in caller: `{}`. {}",
            shim_lifecycle,
            show(instruction),
            caller->show(),
            types_logging),
        /* verbosity_level */ 1);
    return;
  } else if (
      target_lifecycle_methods.size() >= heuristics.join_override_threshold()) {
    // Although this is not a join, shimming to the derived life-cycle methods
    // simulates the joining the models of these as if they were virtual
    // overrides. Besides, if there is a large number of overrides, there will
    // likely be many false positives as well.
    EventLogger::log_event(
        "shim_lifecycle_target_too_many_overrides",
        fmt::format(
            "Shim lifecycle target: `{}` resolved to {} methods at instruction: `{}` in caller: `{}` "
            "which exceeds the join override threshold of {}. Shim not created. {}",
            method_name,
            target_lifecycle_methods.size(),
            show(instruction),
            caller->show(),
            heuristics.join_override_threshold(),
            types_logging),
        /* verbosity_level */ 1);
    return;
  } else {
    EventLogger::log_event(
        "shim_lifecycle_target_found",
        fmt::format(
            "Shim lifecycle target: `{}` resolved to `{}` methods at instruction `{}` in caller: `{}`. {}",
            method_name,
            target_lifecycle_methods.size(),
            show(instruction),
            caller->show(),
            types_logging),
        /* verbosity_level */ 1);
  }

  for (const auto* lifecycle_method : target_lifecycle_methods) {
    auto root_registers =
        shim_lifecycle.root_registers(callee, lifecycle_method, instruction);
    auto call_index =
        update_index(sink_textual_order_index, lifecycle_method->signature());

    artificial_callees.push_back(
        ArtificialCallee{
            /* call_target */
            CallTarget::direct_call(
                instruction,
                lifecycle_method,
                receiver_type,
                CallTarget::CallKind::Shim,
                call_index),
            /* root_registers */ root_registers,
            /* features */
            FeatureSet{feature_factory.get_via_shim_feature(callee)},
        });
  }
}

void add_shim_artificial_callees(
    const Method* caller,
    const Method* callee,
    const IRInstruction* instruction,
    const Methods& method_factory,
    const Types& types,
    const LifecycleMethods& lifecycle_methods,
    const Overrides& override_factory,
    const ClassHierarchies& class_hierarchies,
    const FeatureFactory& feature_factory,
    const Shim& shim,
    const Heuristics& heuristics,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index,
    std::vector<ArtificialCallee>& artificial_callees) {
  for (const auto& shim_target : shim.targets()) {
    process_shim_target(
        caller,
        callee,
        shim_target,
        instruction,
        CallTarget::CallKind::Shim,
        method_factory,
        types,
        override_factory,
        class_hierarchies,
        feature_factory,
        sink_textual_order_index,
        artificial_callees,
        /* extra_features */ {});
  }

  for (const auto& shim_reflection : shim.reflections()) {
    process_shim_reflection(
        caller,
        callee,
        shim_reflection,
        instruction,
        method_factory,
        types,
        override_factory,
        class_hierarchies,
        feature_factory,
        sink_textual_order_index,
        artificial_callees);
  }

  for (const auto& shim_lifecycle : shim.lifecycles()) {
    process_shim_lifecycle(
        caller,
        callee,
        shim_lifecycle,
        instruction,
        types,
        lifecycle_methods,
        class_hierarchies,
        feature_factory,
        heuristics,
        sink_textual_order_index,
        artificial_callees);
  }

  for (const auto& shim_target : shim.intent_routing_targets()) {
    process_shim_target(
        caller,
        callee,
        shim_target,
        instruction,
        CallTarget::CallKind::IntentRouting,
        method_factory,
        types,
        override_factory,
        class_hierarchies,
        feature_factory,
        sink_textual_order_index,
        artificial_callees,
        /* extra_features */
        FeatureSet{feature_factory.get_intent_routing_feature()});
  }
}

bool is_field_instruction(const IRInstruction* instruction) {
  return opcode::is_an_iget(instruction->opcode()) ||
      opcode::is_an_sget(instruction->opcode()) ||
      opcode::is_an_iput(instruction->opcode()) ||
      opcode::is_an_sput(instruction->opcode());
}

bool is_field_sink_instruction(const IRInstruction* instruction) {
  return opcode::is_an_iput(instruction->opcode()) ||
      opcode::is_an_sput(instruction->opcode());
}

bool is_field_or_invoke_instruction(const IRInstruction* instruction) {
  return is_field_instruction(instruction) ||
      opcode::is_an_invoke(instruction->opcode());
}

InstructionCallGraphInformation process_instruction(
    const Method* caller,
    const IRInstruction* instruction,
    const Options& options,
    const Types& types,
    const ClassHierarchies& class_hierarchies,
    const LifecycleMethods& lifecycle_methods,
    const Shims& shims,
    const FeatureFactory& feature_factory,
    const Heuristics& heuristics,
    ConcurrentSet<const Method*>& worklist,
    ConcurrentSet<const Method*>& processed,
    Methods& method_factory,
    Fields& field_factory,
    Overrides& override_factory,
    MethodMappings& method_mappings,
    std::unordered_map<std::string, TextualOrderIndex>&
        sink_textual_order_index) {
  InstructionCallGraphInformation instruction_information;

  if (is_field_instruction(instruction)) {
    const auto* field = resolve_field_access(caller, instruction);
    if (field != nullptr) {
      TextualOrderIndex index = 0;
      if (is_field_sink_instruction(instruction)) {
        index = update_index(
            sink_textual_order_index, ::show(instruction->get_field()));
      }
      instruction_information.field_access = {field_factory.get(field), index};
    }
    if (!opcode::is_an_iput(instruction->opcode())) {
      return instruction_information;
    }

    const auto* iput_type =
        types.source_type(caller, instruction, /* source_position */ 0);
    if (iput_type && is_anonymous_class(iput_type)) {
      auto artificial_callees_for_instruction =
          anonymous_class_artificial_callees(
              method_factory,
              instruction,
              iput_type,
              /* register */ instruction->src(0),
              sink_textual_order_index,
              /* features */
              FeatureSet{feature_factory.get("via-anonymous-class-to-field")});

      if (!artificial_callees_for_instruction.empty()) {
        instruction_information.artificial_callees =
            std::move(artificial_callees_for_instruction);
      }
    }
    return instruction_information;
  }

  if (!opcode::is_an_invoke(instruction->opcode())) {
    return instruction_information;
  }

  const DexMethod* dex_callee = resolve_call(types, caller, instruction);
  if (!dex_callee) {
    return instruction_information;
  }

  const Method* original_callee = method_factory.get(dex_callee);
  ParameterTypeOverrides parameter_type_overrides =
      anonymous_class_arguments(types, caller, instruction, dex_callee);

  const auto* resolved_callee = get_callee_from_resolved_call(
      dex_callee,
      instruction,
      parameter_type_overrides,
      options,
      feature_factory,
      method_factory,
      method_mappings,
      instruction_information.artificial_callees,
      sink_textual_order_index);

  if (auto shim = shims.get_shim_for_caller(original_callee, caller)) {
    add_shim_artificial_callees(
        caller,
        resolved_callee,
        instruction,
        method_factory,
        types,
        lifecycle_methods,
        override_factory,
        class_hierarchies,
        feature_factory,
        *shim,
        heuristics,
        sink_textual_order_index,
        instruction_information.artificial_callees);
  }

  auto call_index =
      update_index(sink_textual_order_index, ::show(instruction->get_method()));
  if (resolved_callee->parameter_type_overrides().empty() ||
      processed.count(resolved_callee) != 0) {
    instruction_information.callee = CallTarget::from_call_instruction(
        caller,
        instruction,
        resolved_callee,
        call_index,
        types,
        class_hierarchies,
        override_factory);
    return instruction_information;
  }
  // This is a newly introduced method with parameter type
  // overrides. We need to generate it's method overrides,
  // and compute callees for them.
  std::unordered_set<const Method*> original_methods =
      override_factory.get(original_callee);
  original_methods.insert(original_callee);

  for (const Method* original_method : original_methods) {
    const Method* method = method_factory.create(
        original_method->dex_method(),
        resolved_callee->parameter_type_overrides());
    method_mappings.create_mappings_for_method(method);

    std::unordered_set<const Method*> overrides;
    for (const Method* original_override :
         override_factory.get(original_method)) {
      const Method* override_method = method_factory.create(
          original_override->dex_method(),
          resolved_callee->parameter_type_overrides());
      method_mappings.create_mappings_for_method(override_method);
      overrides.insert(override_method);
    }

    if (!overrides.empty()) {
      override_factory.set(method, std::move(overrides));
    }

    if (processed.count(method) == 0) {
      worklist.insert(method);
    }
  }
  instruction_information.callee = CallTarget::from_call_instruction(
      caller,
      instruction,
      resolved_callee,
      call_index,
      types,
      class_hierarchies,
      override_factory);
  return instruction_information;
}

} // namespace

CallTarget::CallTarget(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    CallKind call_kind,
    TextualOrderIndex call_index,
    const DexType* MT_NULLABLE receiver_type,
    const std::unordered_set<const DexType*>* MT_NULLABLE
        receiver_local_extends,
    const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends,
    const std::unordered_set<const Method*>* MT_NULLABLE overrides)
    : instruction_(instruction),
      resolved_base_callee_(resolved_base_callee),
      call_kind_(call_kind),
      call_index_(call_index),
      receiver_type_(receiver_type),
      receiver_local_extends_(receiver_local_extends),
      receiver_extends_(receiver_extends),
      overrides_(overrides) {}

CallTarget CallTarget::static_call(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE callee,
    CallKind call_kind,
    TextualOrderIndex call_index) {
  return CallTarget::direct_call(
      instruction,
      callee,
      /* receiver_type */ nullptr,
      call_kind,
      call_index);
}

CallTarget CallTarget::direct_call(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE callee,
    const DexType* MT_NULLABLE receiver_type,
    CallKind call_kind,
    TextualOrderIndex call_index) {
  return CallTarget(
      instruction,
      /* resolved_base_callee */ callee,
      call_kind,
      call_index,
      /* receiver_type */ receiver_type,
      /* receiver_local_extends */ nullptr,
      /* receiver_extends */ nullptr,
      /* overrides */ nullptr);
}

CallTarget CallTarget::virtual_call(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    const DexType* MT_NULLABLE receiver_type,
    const std::unordered_set<const DexType*>* MT_NULLABLE
        receiver_local_extends,
    const ClassHierarchies& class_hierarchies,
    const Overrides& override_factory,
    CallKind call_kind,
    TextualOrderIndex call_index) {
  // All overrides are potential callees.
  const std::unordered_set<const Method*>* overrides = nullptr;
  if (resolved_base_callee != nullptr) {
    overrides = &override_factory.get(resolved_base_callee);
  } else {
    overrides = &override_factory.empty_method_set();
  }

  // If the receiver type does not define the method, `resolved_base_callee`
  // will reference a method on a parent class. Taking all overrides of
  // `resolved_base_callee` can be imprecise since it would include overrides
  // that don't extend the receiver type. Filtering overrides based on classes
  // extending the receiver type fixes the problem.
  //
  // For instance:
  // ```
  // class A { void f() { ... } }
  // class B implements A {}
  // class C extends B { void f() { ... } }
  // class D implements A { void f() { ... } }
  // ```
  // A virtual call to `B::f` has a resolved base callee of `A::f`. Overrides
  // of `A::f` includes `D::f`, but `D::f` cannot be called since `D` does not
  // extend `B`.
  const std::unordered_set<const DexType*>* receiver_extends = nullptr;
  if (receiver_type != nullptr && receiver_type != type::java_lang_Object()) {
    receiver_extends = &class_hierarchies.extends(receiver_type);
  }

  return CallTarget(
      instruction,
      resolved_base_callee,
      call_kind,
      call_index,
      receiver_type,
      receiver_local_extends,
      receiver_extends,
      overrides);
}

CallTarget CallTarget::from_call_instruction(
    const Method* caller,
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    TextualOrderIndex call_index,
    const Types& types,
    const ClassHierarchies& class_hierarchies,
    const Overrides& override_factory) {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));
  if (is_static_invoke(instruction)) {
    return CallTarget::static_call(
        instruction, resolved_base_callee, CallKind::Normal, call_index);
  } else if (is_virtual_invoke(instruction)) {
    return CallTarget::virtual_call(
        instruction,
        resolved_base_callee,
        types.receiver_type(caller, instruction),
        &types.receiver_local_extends(caller, instruction),
        class_hierarchies,
        override_factory,
        CallKind::Normal,
        call_index);
  } else {
    return CallTarget::direct_call(
        instruction,
        resolved_base_callee,
        types.receiver_type(caller, instruction),
        CallKind::Normal,
        call_index);
  }
}

bool CallTarget::FilterOverrides::operator()(const Method* method) const {
  return extends == nullptr || extends->count(method->get_class()) > 0;
}

CallTarget::OverridesRange CallTarget::overrides() const {
  mt_assert(resolved());
  mt_assert(is_virtual());

  const auto* extends = receiver_extends_;
  if (receiver_local_extends_ && receiver_local_extends_->size() > 0) {
    extends = receiver_local_extends_;
  }

  return boost::make_iterator_range(
      boost::make_filter_iterator(
          FilterOverrides{extends}, overrides_->cbegin()),
      boost::make_filter_iterator(
          FilterOverrides{extends}, overrides_->cend()));
}

bool CallTarget::operator==(const CallTarget& other) const {
  return instruction_ == other.instruction_ &&
      resolved_base_callee_ == other.resolved_base_callee_ &&
      call_kind_ == other.call_kind_ && call_index_ == other.call_index_ &&
      receiver_type_ == other.receiver_type_ &&
      receiver_local_extends_ == other.receiver_local_extends_ &&
      receiver_extends_ == other.receiver_extends_ &&
      overrides_ == other.overrides_;
}

namespace {

std::string call_kind_to_string(CallTarget::CallKind kind) {
  switch (kind) {
    case CallTarget::CallKind::Normal:
      return "normal";
    case CallTarget::CallKind::AnonymousClass:
      return "anonymous_class";
    case CallTarget::CallKind::Shim:
      return "shim";
    case CallTarget::CallKind::IntentRouting:
      return "intent_routing";
  }
  mt_unreachable();
}

} // namespace

std::ostream& operator<<(std::ostream& out, const CallTarget& call_target) {
  out << "CallTarget(instruction=`" << show(call_target.instruction())
      << "`, resolved_base_callee=`" << show(call_target.resolved_base_callee())
      << "`, call_kind=`" << call_kind_to_string(call_target.call_kind_) << "`"
      << "`, call_index=`" << call_target.call_index_ << "`";
  if (const auto* receiver_type = call_target.receiver_type()) {
    out << ", receiver_type=`" << show(receiver_type) << "`";
  }
  if (call_target.is_virtual()) {
    out << ", overrides={";
    for (const auto* method : call_target.overrides()) {
      out << "`" << method->show() << "`, ";
    }
    out << "}";
  }
  return out << ")";
}

bool ArtificialCallee::operator==(const ArtificialCallee& other) const {
  return call_target == other.call_target &&
      root_registers == other.root_registers && features == other.features;
}

std::ostream& operator<<(std::ostream& out, const ArtificialCallee& callee) {
  out << "ArtificialCallee(call_target=" << callee.call_target
      << ", root_registers={";
  for (const auto& [root, register_id] : callee.root_registers) {
    out << " " << root << ": v" << register_id << ",";
  }
  return out << "}, features=" << callee.features << ")";
}

bool FieldTarget::operator==(const FieldTarget& other) const {
  return field == other.field && field_sink_index == other.field_sink_index;
}

std::ostream& operator<<(std::ostream& out, const FieldTarget& field_target) {
  out << "FieldTarget(field=" << field_target.field;
  return out << ", field_sink_index=" << field_target.field_sink_index << ")";
}

CallGraph::CallGraph(
    const Options& options,
    const Types& types,
    const ClassHierarchies& class_hierarchies,
    const FeatureFactory& feature_factory,
    const Heuristics& heuristics,
    Methods& method_factory,
    Fields& field_factory,
    Overrides& override_factory,
    MethodMappings& method_mappings,
    LifecycleMethods lifecycle_methods,
    Shims shims)
    : types_(types),
      class_hierarchies_(class_hierarchies),
      overrides_(override_factory) {
  ConcurrentSet<const Method*> worklist;
  ConcurrentSet<const Method*> processed;
  for (const Method* method : method_factory) {
    worklist.insert(method);
  }

  std::atomic<std::size_t> method_iteration(0);
  std::size_t number_methods = 0;

  while (worklist.size() > 0) {
    auto queue = sparta::work_queue<const Method*>(
        [&](const Method* caller) {
          method_iteration++;
          if (method_iteration % 10000 == 0) {
            LOG_IF_INTERACTIVE(
                1,
                "Processed {}/{} methods.",
                method_iteration.load(),
                number_methods);
          }

          auto* code = caller->get_code();
          if (!code) {
            return;
          }

          mt_assert(code->cfg_built());

          std::unordered_map<const IRInstruction*, CallTarget> callees;
          std::unordered_map<const IRInstruction*, ArtificialCallees>
              artificial_callees;
          std::unordered_map<const IRInstruction*, FieldTarget> field_accesses;
          std::unordered_map<const IRInstruction*, TextualOrderIndex>
              indexed_returns;
          std::unordered_map<const IRInstruction*, TextualOrderIndex>
              indexed_array_allocations;
          std::unordered_map<std::string, TextualOrderIndex>
              sink_textual_order_index;
          TextualOrderIndex next_return = 0;
          TextualOrderIndex next_array_allocation = 0;

          auto reverse_postordered_blocks =
              graph::postorder_sort<cfg::GraphInterface>(code->cfg());
          std::reverse(
              reverse_postordered_blocks.begin(),
              reverse_postordered_blocks.end());
          for (const auto* block : reverse_postordered_blocks) {
            for (const auto& entry : *block) {
              if (entry.type != MFLOW_OPCODE) {
                continue;
              }
              const auto* instruction = entry.insn;
              if (opcode::is_a_return(instruction->opcode())) {
                indexed_returns.emplace(std::pair(instruction, next_return++));
              } else if (
                  opcode::is_filled_new_array(instruction->opcode()) ||
                  opcode::is_new_array(instruction->opcode())) {
                indexed_array_allocations.emplace(
                    std::pair(instruction, next_array_allocation++));
              } else if (!is_field_or_invoke_instruction(instruction)) {
                continue;
              }

              auto instruction_information = process_instruction(
                  caller,
                  instruction,
                  options,
                  types,
                  class_hierarchies,
                  lifecycle_methods,
                  shims,
                  feature_factory,
                  heuristics,
                  worklist,
                  processed,
                  method_factory,
                  field_factory,
                  override_factory,
                  method_mappings,
                  sink_textual_order_index);
              if (instruction_information.artificial_callees.size() > 0) {
                artificial_callees.emplace(
                    instruction, instruction_information.artificial_callees);
              }
              if (instruction_information.callee) {
                callees.emplace(instruction, *(instruction_information.callee));
              } else if (instruction_information.field_access) {
                field_accesses.emplace(
                    instruction, *(instruction_information.field_access));
              }
            }
          }

          if (!callees.empty()) {
            resolved_base_callees_.insert_or_assign(
                std::make_pair(caller, std::move(callees)));
          }
          if (!artificial_callees.empty()) {
            artificial_callees_.insert_or_assign(
                std::make_pair(caller, std::move(artificial_callees)));
          }
          if (!field_accesses.empty()) {
            resolved_fields_.insert_or_assign(
                std::make_pair(caller, std::move(field_accesses)));
          }
          if (!indexed_array_allocations.empty()) {
            indexed_array_allocations_.insert_or_assign(
                std::make_pair(caller, std::move(indexed_array_allocations)));
          }
          if (!indexed_returns.empty()) {
            indexed_returns_.insert_or_assign(
                std::make_pair(caller, std::move(indexed_returns)));
          }
        },
        sparta::parallel::default_num_threads());
    for (const auto* method : UnorderedIterable(worklist)) {
      queue.add_item(method);
      processed.insert(method);
    }
    worklist.clear();
    number_methods = method_factory.size();
    queue.run_all();
  }

  if (options.dump_call_graph()) {
    dump_call_graph(options.call_graph_output_path());
  }

  log_call_graph_stats(heuristics);
}

std::vector<CallTarget> CallGraph::callees(const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_base_callees_` is read-only after the constructor completed.
  auto callees = resolved_base_callees_.find(caller);
  if (callees == resolved_base_callees_.end()) {
    return {};
  }

  std::vector<CallTarget> call_targets;
  for (auto [instruction, call_target] : callees->second) {
    call_targets.push_back(call_target);
  }
  return call_targets;
}

CallTarget CallGraph::callee(
    const Method* caller,
    const IRInstruction* instruction) const {
  auto empty_resolved_callee = CallTarget::from_call_instruction(
      caller,
      instruction,
      /* resolved_base_callee */ nullptr,
      /* call_index */ 0,
      types_,
      class_hierarchies_,
      overrides_);
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_base_callees_` is read-only after the constructor completed.
  auto callees = resolved_base_callees_.find(caller);
  if (callees == resolved_base_callees_.end()) {
    return empty_resolved_callee;
  }

  auto callee = callees->second.find(instruction);
  if (callee == callees->second.end()) {
    return empty_resolved_callee;
  }

  return callee->second;
}

const std::unordered_map<const IRInstruction*, ArtificialCallees>&
CallGraph::artificial_callees(const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `artificial_callees_` is read-only after the constructor completed.
  auto artificial_callees_map = artificial_callees_.find(caller);
  if (artificial_callees_map == artificial_callees_.end()) {
    return empty_artificial_callees_map_;
  } else {
    return artificial_callees_map->second;
  }
}

const ArtificialCallees& CallGraph::artificial_callees(
    const Method* caller,
    const IRInstruction* instruction) const {
  const auto& artificial_callees_map = this->artificial_callees(caller);
  auto artificial_callees = artificial_callees_map.find(instruction);
  if (artificial_callees == artificial_callees_map.end()) {
    return empty_artificial_callees_;
  } else {
    return artificial_callees->second;
  }
}

const std::optional<FieldTarget> CallGraph::resolved_field_access(
    const Method* caller,
    const IRInstruction* instruction) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_fields_` is read-only after the constructor completed.
  auto fields = resolved_fields_.find(caller);
  if (fields == resolved_fields_.end()) {
    return std::nullopt;
  }

  auto field = fields->second.find(instruction);
  if (field == fields->second.end()) {
    return std::nullopt;
  }

  return field->second;
}

const std::vector<FieldTarget> CallGraph::resolved_field_accesses(
    const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_fields_` is read-only after the constructor completed.
  auto fields = resolved_fields_.find(caller);
  if (fields == resolved_fields_.end()) {
    return {};
  }

  std::vector<FieldTarget> field_targets;
  for (const auto& [_, field_target] : fields->second) {
    field_targets.push_back(field_target);
  }
  return field_targets;
}

TextualOrderIndex CallGraph::return_index(
    const Method* caller,
    const IRInstruction* instruction) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `indexed_returns_` is read-only after the constructor completed.
  auto returns = indexed_returns_.find(caller);
  mt_assert(returns != indexed_returns_.end());

  auto index = returns->second.find(instruction);
  if (index == returns->second.end()) {
    return 0;
  }
  return index->second;
}

const std::vector<TextualOrderIndex> CallGraph::return_indices(
    const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `indexed_returns_` is read-only after the constructor completed.
  auto returns = indexed_returns_.find(caller);
  mt_assert(returns != indexed_returns_.end());

  std::vector<TextualOrderIndex> return_indices;
  for (const auto& [_, index] : returns->second) {
    return_indices.push_back(index);
  }
  return return_indices;
}

TextualOrderIndex CallGraph::array_allocation_index(
    const Method* caller,
    const IRInstruction* instruction) const {
  auto array_allocations = indexed_array_allocations_.find(caller);
  mt_assert(array_allocations != indexed_returns_.end());

  auto index = array_allocations->second.find(instruction);
  if (index == array_allocations->second.end()) {
    return 0;
  }
  return index->second;
}

const std::vector<TextualOrderIndex> CallGraph::array_allocation_indices(
    const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `indexed_array_allocations_` is read-only after the constructor completed.
  auto array_allocations = indexed_array_allocations_.find(caller);
  mt_assert(array_allocations != indexed_array_allocations_.end());

  std::vector<TextualOrderIndex> array_allocation_indices;
  for (const auto& [_, index] : array_allocations->second) {
    array_allocation_indices.push_back(index);
  }
  return array_allocation_indices;
}

bool CallGraph::has_callees(const Method* caller) {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_base_callees_` and `artificial_callees_` are read-only
  // after the constructor completed.
  auto base_callees = resolved_base_callees_.find(caller);
  if (base_callees != resolved_base_callees_.end() &&
      !base_callees->second.empty()) {
    return true;
  }
  auto artificial_callees = artificial_callees_.find(caller);
  if (artificial_callees != artificial_callees_.end() &&
      !artificial_callees->second.empty()) {
    return true;
  }

  return false;
}

Json::Value CallGraph::to_json(const Method* method) const {
  auto method_value = Json::Value(Json::objectValue);

  auto resolved_callee = resolved_base_callees_.find(method);
  if (resolved_callee != resolved_base_callees_.end()) {
    std::unordered_set<const Method*> static_callees;
    std::unordered_set<const Method*> virtual_callees;
    for (const auto& [instruction, call_target] : resolved_callee->second) {
      if (!call_target.resolved()) {
        continue;
      } else if (call_target.is_virtual()) {
        virtual_callees.insert(call_target.resolved_base_callee());
        const auto& overrides = call_target.overrides();
        if (std::distance(overrides.begin(), overrides.end()) >
            Heuristics::singleton().join_override_threshold()) {
          continue;
        }
        for (const auto* override : overrides) {
          virtual_callees.insert(override);
        }

      } else {
        static_callees.insert(call_target.resolved_base_callee());
      }
    }

    if (!static_callees.empty()) {
      auto static_callees_value = Json::Value(Json::arrayValue);
      for (const auto* callee : static_callees) {
        static_callees_value.append(Json::Value(show(callee)));
      }
      method_value["static"] = static_callees_value;
    }

    if (!virtual_callees.empty()) {
      auto virtual_callees_value = Json::Value(Json::arrayValue);
      for (const auto* callee : virtual_callees) {
        virtual_callees_value.append(Json::Value(show(callee)));
      }
      method_value["virtual"] = virtual_callees_value;
    }
  }

  auto instruction_artificial_callees = artificial_callees_.find(method);
  if (instruction_artificial_callees != artificial_callees_.end()) {
    std::unordered_set<const Method*> anonymous_classes;
    std::unordered_set<const Method*> shims;
    std::unordered_set<const Method*> intent_routing;
    for (const auto& [instruction, artificial_callees] :
         instruction_artificial_callees->second) {
      for (const auto& artificial_callee : artificial_callees) {
        auto* resolved = artificial_callee.call_target.resolved_base_callee();
        auto call_kind = artificial_callee.call_target.call_kind();
        mt_assert(resolved != nullptr);

        switch (call_kind) {
          case CallTarget::CallKind::Shim:
            shims.insert(resolved);
            break;

          case CallTarget::CallKind::AnonymousClass:
            anonymous_classes.insert(resolved);
            break;

          case CallTarget::CallKind::IntentRouting:
            intent_routing.insert(resolved);
            break;

          case CallTarget::CallKind::Normal:
            mt_unreachable();
        }
      }
    }

    if (!anonymous_classes.empty()) {
      auto callees_value = Json::Value(Json::arrayValue);
      for (const auto* callee : anonymous_classes) {
        callees_value.append(Json::Value(show(callee)));
      }
      method_value["anonymous_class"] = callees_value;
    }

    if (!shims.empty()) {
      auto shims_value = Json::Value(Json::arrayValue);
      for (const auto* callee : shims) {
        shims_value.append(Json::Value(show(callee)));
      }
      method_value["shim"] = shims_value;
    }

    if (!intent_routing.empty()) {
      auto intent_routing_value = Json::Value(Json::arrayValue);
      for (const auto* callee : intent_routing) {
        intent_routing_value.append(Json::Value(show(callee)));
      }
      method_value["intent_routing"] = intent_routing_value;
    }
  }

  JsonValidation::validate_object(method_value);

  return method_value;
}

Json::Value CallGraph::to_json() const {
  auto value = Json::Value(Json::objectValue);
  for (const auto& [method, _callees] :
       UnorderedIterable(resolved_base_callees_)) {
    value[method->show()] = to_json(method);
  }

  // Add methods that only have artificial callees
  for (const auto& [method, _callees] :
       UnorderedIterable(artificial_callees_)) {
    if (resolved_base_callees_.find(method) == resolved_base_callees_.end()) {
      value[method->show()] = to_json(method);
    }
  }

  return value;
}

void CallGraph::dump_call_graph(
    const std::filesystem::path& output_directory,
    const std::size_t batch_size) const {
  LOG(1, "Writing call graph to `{}`", output_directory.native());

  // Collect all methods in the callgraph
  std::vector<const Method*> methods;
  methods.reserve(resolved_base_callees_.size());
  for (const auto& [method, _callees] :
       UnorderedIterable(resolved_base_callees_)) {
    methods.push_back(method);
  }

  // Add methods that only have artificial callees
  for (const auto& [method, _callees] :
       UnorderedIterable(artificial_callees_)) {
    if (resolved_base_callees_.find(method) == resolved_base_callees_.end()) {
      methods.push_back(method);
    }
  }
  std::size_t total_elements = methods.size();

  auto get_json_line = [&](std::size_t i) -> Json::Value {
    auto value = Json::Value(Json::objectValue);
    const auto* method = methods.at(i);
    value[method->show()] = to_json(method);
    return value;
  };

  JsonWriter::write_sharded_json_files(
      output_directory,
      batch_size,
      total_elements,
      "call-graph@",
      get_json_line);
}

namespace {

CallGraphStats::StatTypes compute_stat_types(
    std::vector<std::size_t>&& histogram,
    std::size_t threshold) {
  CallGraphStats::StatTypes stats;
  stats.total = histogram.size();
  if (histogram.size() == 0) {
    // Can't do math with a 0 denominator.
    return stats;
  }

  std::sort(histogram.begin(), histogram.end());
  auto total_sum = std::accumulate(histogram.begin(), histogram.end(), 0);
  stats.average = total_sum / (double)stats.total;

  // Note: Percentile indices are rounded down for convenience. Total is
  // typically large enough (>1000) that this doesn't matter.
  std::size_t p50_index = stats.total / 2;
  stats.p50 = histogram[p50_index];
  std::size_t p90_index = stats.total * 0.9;
  stats.p90 = histogram[p90_index];
  std::size_t p99_index = stats.total * 0.99;
  stats.p99 = histogram[p99_index];
  stats.min = histogram.front();
  stats.max = histogram.back();

  // Value should be 0% if nothing is above the threshold.
  stats.percentage_above_threshold = 0;
  for (std::size_t i = 0; i < histogram.size(); i++) {
    if (histogram[i] > threshold) {
      stats.percentage_above_threshold =
          (1 - (i / (double)histogram.size())) * 100;
      break;
    }
  }

  return stats;
}

CallGraphStats::StatTypes compute_virtual_callsite_stats(
    const ConcurrentMap<
        const Method*,
        std::unordered_map<const IRInstruction*, CallTarget>>&
        resolved_base_callees,
    std::size_t join_override_threshold) {
  std::vector<std::size_t> num_resolved_targets_per_virtual_callsite;
  for (const auto& [method, instruction_target] :
       UnorderedIterable(resolved_base_callees)) {
    for (const auto& [_instruction, call_target] : instruction_target) {
      if (!call_target.resolved() || !call_target.is_virtual()) {
        continue;
      }
      auto overrides = call_target.overrides();
      auto num_overrides = std::distance(overrides.begin(), overrides.end());
      // Note: The resolved callee is always one of the targets. Hence +1.
      std::size_t num_resolved_targets =
          1 + static_cast<std::size_t>(num_overrides);
      num_resolved_targets_per_virtual_callsite.emplace_back(
          num_resolved_targets);
    }
  }
  return compute_stat_types(
      std::move(num_resolved_targets_per_virtual_callsite),
      join_override_threshold);
}

CallGraphStats::StatTypes compute_artificial_callee_stats(
    const ConcurrentMap<
        const Method*,
        std::unordered_map<const IRInstruction*, ArtificialCallees>>&
        artificial_callees,
    std::size_t join_override_threshold) {
  std::vector<std::size_t> num_artificial_callees_per_callsite;
  for (const auto& [_method, instruction_target] :
       UnorderedIterable(artificial_callees)) {
    for (const auto& [_instruction, callees] : instruction_target) {
      std::size_t num_resolved_targets = 0;
      for (const auto& artificial_callee : callees) {
        if (!artificial_callee.call_target.resolved()) {
          continue;
        }
        ++num_resolved_targets; // The resolved_callee is one of the targets.
        if (artificial_callee.call_target.is_virtual()) {
          auto overrides = artificial_callee.call_target.overrides();
          auto num_overrides =
              std::distance(overrides.begin(), overrides.end());
          num_resolved_targets += static_cast<std::size_t>(num_overrides);
        }
      }
      mt_assert_log(
          num_resolved_targets != 0,
          "Expected shims to resolve to at least 1 target");
      num_artificial_callees_per_callsite.emplace_back(num_resolved_targets);
    }
  }
  return compute_stat_types(
      std::move(num_artificial_callees_per_callsite), join_override_threshold);
}

} // namespace

CallGraphStats::CallGraphStats(
    const ConcurrentMap<
        const Method*,
        std::unordered_map<const IRInstruction*, CallTarget>>&
        resolved_base_callees,
    ConcurrentMap<
        const Method*,
        std::unordered_map<const IRInstruction*, ArtificialCallees>>
        artificial_callees,
    int join_override_threshold) {
  virtual_callsites_stats = compute_virtual_callsite_stats(
      resolved_base_callees, join_override_threshold);
  artificial_callsites_stats = compute_artificial_callee_stats(
      artificial_callees, join_override_threshold);
}

CallGraphStats CallGraph::compute_stats(
    std::size_t join_override_threshold) const {
  CallGraphStats stats(
      resolved_base_callees_, artificial_callees_, join_override_threshold);
  return stats;
}

void CallGraph::log_call_graph_stats(const Heuristics& heuristics) const {
  auto stats = compute_stats(heuristics.join_override_threshold());

  EventLogger::log_event(
      "call_graph_num_virtual_callsites",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.total,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_average_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.average,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p50_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.p50,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p90_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.p90,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p99_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.p99,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_min_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.min,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_max_targets_per_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.max,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_pct_above_threshold_for_virtual_callsite",
      /* message */ "",
      /* value */ stats.virtual_callsites_stats.percentage_above_threshold,
      /* verbosity_level */ 1);

  EventLogger::log_event(
      "call_graph_num_artificial_callsites",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.total,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_average_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.average,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p50_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.p50,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p90_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.p90,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_p99_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.p99,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_min_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.min,
      /* verbosity_level */ 1);
  EventLogger::log_event(
      "call_graph_max_targets_per_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.max,
      /* verbosity_level */ 1);
  // NOTE: This stat might be meaningless for artificial callsites since we
  // explicitly disallow creating artifical callees when it exceeds the
  // threshold. It would be more meaningful to count the "too_many_overrides"
  // event.
  EventLogger::log_event(
      "call_graph_pct_above_threshold_for_artificial_callsite",
      /* message */ "",
      /* value */ stats.artificial_callsites_stats.percentage_above_threshold,
      /* verbosity_level */ 1);
}

} // namespace marianatrench
