/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <re2/re2.h>

#include <IRInstruction.h>
#include <Resolver.h>
#include <Show.h>
#include <SpartaWorkQueue.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

namespace {

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
      method = resolve_method(
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
        method = resolve_method(dex_method_reference, MethodSearch::Virtual);
      } else {
        method = resolve_method(
            klass,
            dex_method_reference->get_name(),
            dex_method_reference->get_proto(),
            MethodSearch::Virtual);
      }

      if (!method) {
        // `MethodSearch::Virtual` returns null for interface methods.
        method = resolve_method(dex_method_reference, MethodSearch::Interface);
      }
      break;
    }
    default:
      mt_assert_log(false, "unexpected opcode");
  }

  return method;
}

bool is_anonymous_class(const DexType* type) {
  static const re2::RE2 regex("^.*\\$\\d+;$");
  return re2::RE2::FullMatch(show(type), regex);
}

ParameterTypeOverrides anonymous_class_parameters(
    const Types& types,
    const Method* caller,
    const IRInstruction* instruction,
    const DexMethod* callee) {
  mt_assert(callee != nullptr);
  ParameterTypeOverrides parameters;
  const auto& environment = types.environment(caller, instruction);
  auto sources = instruction->srcs_vec();
  for (std::size_t source_position = 0; source_position < sources.size();
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
    auto found = environment.find(sources[source_position]);
    if (found == environment.end()) {
      continue;
    }
    const auto* type = found->second;
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

    callees.push_back(ArtificialCallee{
        /* call_target */ CallTarget::static_call(instruction, method),
        /* register_parameters */ {register_id},
        /* features */ features,
    });
  }

  return callees;
}

ArtificialCallees parameter_types_artificial_callees(
    const Methods& method_factory,
    const Features& features,
    const IRInstruction* instruction,
    const DexMethod* callee,
    const ParameterTypeOverrides& parameter_type_overrides) {
  ArtificialCallees callees;

  // For each anonymous class parameter, simulate calls to all its methods.
  for (auto [parameter, anonymous_class_type] : parameter_type_overrides) {
    auto parameter_callees = anonymous_class_artificial_callees(
        method_factory,
        instruction,
        anonymous_class_type,
        /* register */
        instruction->src(parameter + (is_static(callee) ? 0 : 1)),
        /* features */
        FeatureSet{features.get("via-anonymous-class-to-obscure")});
    callees.insert(
        callees.end(),
        std::make_move_iterator(parameter_callees.begin()),
        std::make_move_iterator(parameter_callees.end()));
  }

  return callees;
}

} // namespace

CallTarget::CallTarget(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    const DexType* MT_NULLABLE receiver_type,
    const std::unordered_set<const Method*>* MT_NULLABLE overrides,
    const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends)
    : instruction_(instruction),
      resolved_base_callee_(resolved_base_callee),
      receiver_type_(receiver_type),
      overrides_(overrides),
      receiver_extends_(receiver_extends) {}

CallTarget CallTarget::static_call(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE callee) {
  return CallTarget(
      instruction,
      /* resolved_base_callee */ callee,
      /* receiver_type */ nullptr,
      /* overrides */ nullptr,
      /* receiver_extends */ nullptr);
}

CallTarget CallTarget::virtual_call(
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    const DexType* MT_NULLABLE receiver_type,
    const ClassHierarchies& class_hierarchies,
    const Overrides& override_factory) {
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
      receiver_type,
      overrides,
      receiver_extends);
}

CallTarget CallTarget::from_call_instruction(
    const Method* caller,
    const IRInstruction* instruction,
    const Method* MT_NULLABLE resolved_base_callee,
    const Types& types,
    const ClassHierarchies& class_hierarchies,
    const Overrides& override_factory) {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  if (is_virtual_invoke(instruction)) {
    return CallTarget::virtual_call(
        instruction,
        resolved_base_callee,
        types.receiver_type(caller, instruction),
        class_hierarchies,
        override_factory);
  } else {
    return CallTarget::static_call(instruction, resolved_base_callee);
  }
}

bool CallTarget::FilterOverrides::operator()(const Method* method) const {
  return extends == nullptr || extends->count(method->get_class()) > 0;
}

CallTarget::OverridesRange CallTarget::overrides() const {
  mt_assert(resolved());
  mt_assert(is_virtual());

  return boost::make_iterator_range(
      boost::make_filter_iterator(
          FilterOverrides{receiver_extends_}, overrides_->cbegin()),
      boost::make_filter_iterator(
          FilterOverrides{receiver_extends_}, overrides_->cend()));
}

bool CallTarget::operator==(const CallTarget& other) const {
  return instruction_ == other.instruction_ &&
      resolved_base_callee_ == other.resolved_base_callee_ &&
      receiver_type_ == other.receiver_type_ &&
      overrides_ == other.overrides_ &&
      receiver_extends_ == other.receiver_extends_;
}

std::ostream& operator<<(std::ostream& out, const CallTarget& call_target) {
  out << "CallTarget(instruction=`" << show(call_target.instruction())
      << "`, resolved_base_callee=`" << show(call_target.resolved_base_callee())
      << "`";
  if (call_target.is_virtual()) {
    out << ", receiver_type=`" << show(call_target.receiver_type())
        << "`, overrides={";
    for (const auto* method : call_target.overrides()) {
      out << "`" << show(method) << "`, ";
    }
    out << "}";
  }
  return out << ")";
}

bool ArtificialCallee::operator==(const ArtificialCallee& other) const {
  return call_target == other.call_target &&
      register_parameters == other.register_parameters &&
      features == other.features;
}

std::ostream& operator<<(std::ostream& out, const ArtificialCallee& callee) {
  out << "ArtificialCallee(call_target=" << callee.call_target
      << ", register_parameters=[";
  for (auto register_id : callee.register_parameters) {
    out << register_id << ", ";
  }
  return out << "], features=" << callee.features << ")";
}

CallGraph::CallGraph(
    const Options& options,
    Methods& method_factory,
    const Types& types,
    const ClassHierarchies& class_hierarchies,
    Overrides& override_factory,
    const Features& features)
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
            LOG(1,
                "Processed {}/{} methods.",
                method_iteration.load(),
                number_methods);
          }

          auto* code = caller->get_code();
          if (!code) {
            return;
          }

          mt_assert(code->cfg_built());

          std::unordered_map<const IRInstruction*, const Method*> callees;
          std::unordered_map<const IRInstruction*, ArtificialCallees>
              artificial_callees;

          for (const auto* block : code->cfg().blocks()) {
            for (const auto& entry : *block) {
              if (entry.type != MFLOW_OPCODE) {
                continue;
              }

              const IRInstruction* instruction = entry.insn;

              if (opcode::is_an_iput(instruction->opcode())) {
                // Add artificial calls to anonymous class methods.
                const auto* type = types.source_type(
                    caller, instruction, /* source_position */ 0);
                if (type && is_anonymous_class(type)) {
                  auto artificial_callees_for_instruction =
                      anonymous_class_artificial_callees(
                          method_factory,
                          instruction,
                          type,
                          /* register */ instruction->src(0),
                          /* features */
                          FeatureSet{
                              features.get("via-anonymous-class-to-field")});

                  if (!artificial_callees_for_instruction.empty()) {
                    artificial_callees.emplace(
                        instruction,
                        std::move(artificial_callees_for_instruction));
                  }
                }
                continue;
              }

              if (!opcode::is_an_invoke(instruction->opcode())) {
                continue;
              }

              const DexMethod* dex_callee =
                  resolve_call(types, caller, instruction);

              if (!dex_callee) {
                continue;
              }

              ParameterTypeOverrides parameter_type_overrides =
                  anonymous_class_parameters(
                      types, caller, instruction, dex_callee);

              const Method* callee = nullptr;
              if (dex_callee->get_code() == nullptr) {
                // Add artificial calls to anonymous class methods.
                auto artificial_callees_for_instruction =
                    parameter_types_artificial_callees(
                        method_factory,
                        features,
                        instruction,
                        dex_callee,
                        parameter_type_overrides);
                if (!artificial_callees_for_instruction.empty()) {
                  artificial_callees.emplace(
                      instruction,
                      std::move(artificial_callees_for_instruction));
                }

                // No need to use type overrides since we don't have the code.
                callee = method_factory.get(dex_callee);
              } else if (options.disable_parameter_type_overrides()) {
                callee = method_factory.get(dex_callee);
              } else {
                // Analyze the callee with these particular types.
                callee =
                    method_factory.create(dex_callee, parameter_type_overrides);
              }
              mt_assert(callee != nullptr);

              callees.emplace(instruction, callee);

              if (!callee->parameter_type_overrides().empty() &&
                  processed.count(callee) == 0) {
                // This is a newly introduced method with parameter type
                // overrides. We need to generate it's method overrides,
                // and compute callees for them.
                const Method* original_callee =
                    method_factory.get(callee->dex_method());
                std::unordered_set<const Method*> original_methods =
                    override_factory.get(original_callee);
                original_methods.insert(original_callee);

                for (const Method* original_method : original_methods) {
                  const Method* method = method_factory.create(
                      original_method->dex_method(),
                      callee->parameter_type_overrides());

                  std::unordered_set<const Method*> overrides;
                  for (const Method* original_override :
                       override_factory.get(original_method)) {
                    overrides.insert(method_factory.create(
                        original_override->dex_method(),
                        callee->parameter_type_overrides()));
                  }

                  if (!overrides.empty()) {
                    override_factory.set(method, std::move(overrides));
                  }

                  if (processed.count(method) == 0) {
                    worklist.insert(method);
                  }
                }
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
        },
        sparta::parallel::default_num_threads());
    for (const auto* method : worklist) {
      queue.add_item(method);
      processed.insert(method);
    }
    worklist.clear();
    number_methods = method_factory.size();
    queue.run_all();
  }

  if (options.dump_call_graph()) {
    LOG(1, "Writing call graph to `call_graph.json`");
    JsonValidation::write_json_file(
        "call_graph.json", to_json(/* with_overrides */ false));
  }
}

std::vector<CallTarget> CallGraph::callees(const Method* caller) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_base_callees_` is read-only after the constructor completed.
  auto callees = resolved_base_callees_.find(caller);
  if (callees == resolved_base_callees_.end()) {
    return {};
  }

  std::vector<CallTarget> call_targets;
  for (auto [instruction, resolved_base_callee] : callees->second) {
    call_targets.push_back(CallTarget::from_call_instruction(
        caller,
        instruction,
        resolved_base_callee,
        types_,
        class_hierarchies_,
        overrides_));
  }
  return call_targets;
}

CallTarget CallGraph::callee(
    const Method* caller,
    const IRInstruction* instruction) const {
  return CallTarget::from_call_instruction(
      caller,
      instruction,
      resolved_base_callee(caller, instruction),
      types_,
      class_hierarchies_,
      overrides_);
}

const Method* MT_NULLABLE CallGraph::resolved_base_callee(
    const Method* caller,
    const IRInstruction* instruction) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `resolved_base_callees_` is read-only after the constructor completed.
  auto callees = resolved_base_callees_.find(caller);
  if (callees == resolved_base_callees_.end()) {
    return nullptr;
  }

  auto callee = callees->second.find(instruction);
  if (callee == callees->second.end()) {
    return nullptr;
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

Json::Value CallGraph::to_json(bool with_overrides) const {
  auto value = Json::Value(Json::objectValue);
  for (const auto& [method, callees] : resolved_base_callees_) {
    auto method_value = Json::Value(Json::objectValue);

    std::unordered_set<const Method*> static_callees;
    std::unordered_set<const Method*> virtual_callees;
    for (const auto& [instruction, resolved_base_callee] : callees) {
      auto call_target = CallTarget::from_call_instruction(
          method,
          instruction,
          resolved_base_callee,
          types_,
          class_hierarchies_,
          overrides_);
      if (!call_target.resolved()) {
        continue;
      } else if (call_target.is_virtual()) {
        virtual_callees.insert(call_target.resolved_base_callee());
        if (with_overrides) {
          for (const auto* override : call_target.overrides()) {
            virtual_callees.insert(override);
          }
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

    value[show(method)] = method_value;
  }
  for (const auto& [method, instruction_artificial_callees] :
       artificial_callees_) {
    std::unordered_set<const Method*> callees;
    for (const auto& [instruction, artificial_callees] :
         instruction_artificial_callees) {
      for (const auto& artificial_callee : artificial_callees) {
        callees.insert(artificial_callee.call_target.resolved_base_callee());
      }
    }

    auto callees_value = Json::Value(Json::arrayValue);
    for (const auto* callee : callees) {
      callees_value.append(Json::Value(show(callee)));
    }
    value[show(method)]["artificial"] = callees_value;
  }
  return value;
}

} // namespace marianatrench
