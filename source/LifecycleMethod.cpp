/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/WorkQueue.h>

#include <DexAccess.h>
#include <Resolver.h>
#include <Show.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

namespace {

void map_argument_type_to_index(
    const LifecycleMethodCall& callee,
    LifecycleMethod::TypeIndexMap& type_index_map) {
  const auto* type_list = callee.get_argument_types();
  if (type_list == nullptr) {
    ERROR(1, "Callee `{}` has invalid argument types.", callee.to_string());
    return;
  }
  for (auto* type : *type_list) {
    type_index_map.emplace(type, type_index_map.size() + 1);
  }
}

/**
 * Lifecycle method creation for linear state transitions
 */
const DexMethod* MT_NULLABLE create_dex_method_from_callees(
    DexClass* dex_klass,
    const LifecycleMethod::TypeIndexMap& type_index_map,
    MethodCreator method_creator,
    const std::vector<LifecycleMethodCall>& callees) {
  auto this_location = method_creator.get_local(0);
  auto* main_block = method_creator.get_main_block();
  mt_assert(main_block != nullptr);

  int callee_count = 0;

  for (const auto& callee : callees) {
    auto* dex_method = callee.get_dex_method(dex_klass);
    if (!dex_method) {
      // Dex method does not apply for current APK.
      // See `LifecycleMethod::validate()`.
      continue;
    }

    ++callee_count;

    std::vector<Location> invoke_with_registers{this_location};
    auto* type_list = callee.get_argument_types();
    // This should have been verified at the start of `create_methods`
    mt_assert(type_list != nullptr);
    for (auto* type : *type_list) {
      auto argument_register =
          method_creator.get_local(type_index_map.at(type));
      invoke_with_registers.push_back(argument_register);
    }
    main_block->invoke(
        IROpcode::OPCODE_INVOKE_VIRTUAL, dex_method, invoke_with_registers);
  }

  if (callee_count < 2) {
    // The point of life-cycle methods is to find flows where tainted member
    // variables flow from one callee into another. If only one (or no) method
    // is overridden, there is no need to create the artificial method. If
    // this happens, it is likely the life-cycle configuration is incorrect.
    WARNING(
        1,
        "Skipped creating life-cycle method for class `{}`. Reason: Insufficient callees.",
        show(dex_klass));
    return nullptr;
  }

  // Add return statement
  main_block->ret_void();

  // The CFG needs to be built for the call graph to be constructed later.
  auto* new_method = method_creator.create();
  mt_assert(new_method != nullptr && new_method->get_code() != nullptr);
  IRCode* code = new_method->get_code();
  code->build_cfg();
  code->cfg().calculate_exit_block();

  // Add method to the class
  dex_klass->add_method(new_method);

  return new_method;
}

/**
 * Lifecycle method creation for graph like state transitions
 *
 * We will perform the code generation in two passes:
 * First pass: creates one basic block for each graph node and add the
 * invocations
 * Second pass: performs the graph traversal and connect
 * the basic blocks with each other
 */
const DexMethod* MT_NULLABLE create_dex_method_from_graph(
    DexClass* dex_klass,
    const LifecycleMethod::TypeIndexMap& type_index_map,
    MethodCreator method_creator,
    const LifeCycleMethodGraph& graph) {
  // First create the method, and then we can work on its CFG
  auto* new_method = method_creator.create();
  mt_assert(new_method != nullptr && new_method->get_code() != nullptr);
  new_method->rstate.set_no_optimizations();
  new_method->rstate.set_generated();
  IRCode* code = new_method->get_code();
  code->build_cfg();
  auto& cfg = code->cfg();

  int callee_count = 0;

  // First pass
  // Create a basic block for each graph node.
  // The entry node is treated specially -- we want to directly reuse the
  // existing entry block for it.
  std::unordered_map<std::string, cfg::Block*> node_to_block;
  for (const auto& [name, node] : graph.get_nodes()) {
    auto* block = cfg.create_block();

    // Construct method calls in this basic block
    for (const auto& callee : node.method_calls()) {
      auto* dex_method = callee.get_dex_method(dex_klass);
      if (!dex_method) {
        // Dex method does not apply for current APK.
        // See `LifecycleMethod::validate()`.
        continue;
      }

      ++callee_count;

      auto* type_list = callee.get_argument_types();
      // This should have been verified at the start of `create_methods`
      mt_assert(type_list != nullptr);
      auto* invoke_insn =
          (new IRInstruction(IROpcode::OPCODE_INVOKE_VIRTUAL))
              ->set_srcs_size(type_list->size() + 1)
              ->set_method(dex_method)
              ->set_src(/* this parameter index */ 0, /* register */ 0);
      size_t idx = 1;
      for (auto* type : *type_list) {
        // MethodCreator by default allocates param registers starting from 0.
        // It does remap param registers to the end of the register frame when
        // MethodCreator::creator is called. Technically we should handle
        // that, but we are lucky here, the method we just created is empty,
        // which means there are no other registers, and consequently the
        // param registers get mapped back to themselves. This means the param
        // register equals to the index of the corresponding parameter in the
        // function proto, i.e., type_index_map
        invoke_insn->set_src(idx++, type_index_map.at(type));
      }

      block->push_back(invoke_insn);
    }

    node_to_block.emplace(name, block);
  }

  if (callee_count < 2) {
    // The point of life-cycle methods is to find flows where tainted member
    // variables flow from one callee into another. If only one (or no) method
    // is overridden, there is no need to create the artificial method. If
    // this happens, it is likely the life-cycle configuration is incorrect.
    WARNING(
        1,
        "Skipped creating life-cycle method for class `{}`. Reason: Insufficient callees.",
        show(dex_klass));

    // Clean up most of the resources used by DexMethod. Note, this does *NOT*
    // directly free the `DexMethod`, the method will be fully freed at some
    // later point
    DexMethodRef::delete_method(new_method);

    return nullptr;
  }

  // Second pass: perform the graph traversal and connect the basic blocks

  // Connect the entry block with the block of the entry node of this graph
  // using an unconditional jump. The entry code cannot directly use the entry
  // block because the entry block already contains instructions that loads
  // the registers. If we were to use it, any execution following a graph edge
  // to the entry block would unconditionally rerun these instructions, which
  // is not ideal.
  cfg.add_edge(
      cfg.entry_block(), node_to_block["entry"], cfg::EdgeType::EDGE_GOTO);

  // create an exit block
  auto* exit_block = cfg.create_block();
  exit_block->push_back(new IRInstruction(OPCODE_RETURN_VOID));

  for (auto& [name, node] : graph.get_nodes()) {
    auto* curr_block = node_to_block[name];

    const auto& successors = node.successors();
    std::vector<std::pair<int32_t, cfg::Block*>> edges;

    // Construct case -> block maps for all successors
    for (size_t i = 0, size = successors.size(); i < size; i++) {
      const auto& successor = successors[i];
      edges.emplace_back(i, node_to_block[successor]);
    }

    cfg::Block* default_block;

    if (name == "exit") {
      // In case the current node may exit the lifecycle (currently set to
      // the onStop and onDestroy nodes), we map the default case to the exit
      // block
      default_block = exit_block;
    } else {
      // Otherwise, we map the default case to the block of the last successor
      mt_assert(!edges.empty());
      std::tie(std::ignore, default_block) = edges.back();
      edges.pop_back();
    }

    // Create the switch in the current basic block to connect all successors
    auto* switch_insn =
        (new IRInstruction(OPCODE_SWITCH))->set_src(0, cfg.allocate_temp());
    cfg.create_branch(curr_block, switch_insn, default_block, edges);
  }

  cfg.calculate_exit_block();

  // Add method to the class
  dex_klass->add_method(new_method);

  return new_method;
}

} // namespace

bool LifecycleGraphNode::operator==(const LifecycleGraphNode& other) const {
  return method_calls_ == other.method_calls_ &&
      successors_ == other.successors_;
}

void LifeCycleMethodGraph::add_node(
    const std::string& node_name,
    std::vector<LifecycleMethodCall> method_calls,
    std::vector<std::string> successors) {
  nodes_.emplace(
      node_name,
      LifecycleGraphNode(std::move(method_calls), std::move(successors)));
}

bool LifeCycleMethodGraph::operator==(const LifeCycleMethodGraph& other) const {
  return nodes_ == other.nodes_;
}

const LifecycleGraphNode* MT_NULLABLE
LifeCycleMethodGraph::get_node(const std::string& node_name) const {
  auto it = nodes_.find(node_name);
  if (it != nodes_.end()) {
    return &it->second;
  }
  return nullptr;
}

LifeCycleMethodGraph LifeCycleMethodGraph::from_json(const Json::Value& value) {
  // Make sure the entry node is always present
  if (!value.isMember("entry")) {
    throw JsonValidationError(
        value, std::nullopt, "an entry point defined for the lifecycle graph");
  }

  LifeCycleMethodGraph graph;

  for (const auto& node_name : value.getMemberNames()) {
    const auto& node = value[node_name];

    std::vector<LifecycleMethodCall> method_calls;
    for (const auto& instruction :
         JsonValidation::null_or_array(node, "instructions")) {
      method_calls.push_back(LifecycleMethodCall::from_json(instruction));
    }

    std::vector<std::string> successors;
    for (const auto& successor :
         JsonValidation::null_or_array(node, "successors")) {
      successors.push_back(JsonValidation::string(successor));
    }

    // Make sure non-exit nodes always have some successors
    if (node_name != "exit" && successors.empty()) {
      throw JsonValidationError(
          node, "successors", "non-empty successor list for a non-exit node");
    }

    graph.add_node(node_name, std::move(method_calls), std::move(successors));
  }

  return graph;
}

LifecycleMethodCall LifecycleMethodCall::from_json(const Json::Value& value) {
  auto method_name = JsonValidation::string(value, "method_name");
  auto return_type = JsonValidation::string(value, "return_type");
  std::vector<std::string> argument_types;
  for (const auto& argument_type :
       JsonValidation::null_or_array(value, "argument_types")) {
    argument_types.emplace_back(JsonValidation::string(argument_type));
  }

  std::optional<std::string> defined_in_derived_class = std::nullopt;
  if (JsonValidation::has_field(value, "defined_in_derived_class")) {
    defined_in_derived_class =
        JsonValidation::string(value, "defined_in_derived_class");
  }

  return LifecycleMethodCall(
      method_name,
      return_type,
      argument_types,
      std::move(defined_in_derived_class));
}

void LifecycleMethodCall::validate(
    const DexClass* base_class,
    const ClassHierarchies& class_hierarchies) const {
  if (!defined_in_derived_class_) {
    if (get_dex_method(base_class) == nullptr) {
      // Callee does not exist within the base class. Likely an invalid config
      // (e.g. typo).
      ERROR(
          1,
          "Callee `{}` is not in base class type `{}`. Check spelling, or add \"defined_in_derived_class\" if the method belongs to a derived class.",
          to_string(),
          base_class->str());
    }
    return;
  }

  const auto* derived_type = DexType::get_type(*defined_in_derived_class_);
  if (!derived_type) {
    // Either a mis-spelt type, or a type that belongs to another APK whose
    // life-cycle config is shared with this one.
    WARNING(
        1,
        "Could not find type `{}` for callee `{}`",
        *defined_in_derived_class_,
        to_string());
    return;
  }

  const auto* derived_class = type_class(derived_type);
  if (!derived_class) {
    // Either derived type is not a class (e.g. primitive ), or the JAR
    // containing the class definition is not loaded. This is a warning and not
    // an error as the type may not be relevant to the current APK.
    WARNING(
        1,
        "Could not convert derived class type `{}` into DexClass.",
        derived_type->str());
    return;
  }

  auto derived_types = class_hierarchies.extends(base_class->get_type());
  if (derived_types.find(derived_type) == derived_types.end()) {
    throw LifecycleMethodValidationError(fmt::format(
        "Derived class `{}` is not derived from base class `{}`.",
        derived_class->str(),
        base_class->str()));
  }

  if (get_dex_method(derived_class) == nullptr) {
    throw LifecycleMethodValidationError(fmt::format(
        "Callee `{}` is not in derived class type `{}`.",
        to_string(),
        derived_class->str()));
  }
}

DexMethodRef* MT_NULLABLE
LifecycleMethodCall::get_dex_method(const DexClass* klass) const {
  const auto* return_type =
      DexType::get_type(DexString::make_string(return_type_));
  if (return_type == nullptr) {
    ERROR(1, "Could not find return type `{}`.", return_type_);
    return nullptr;
  }

  const auto* argument_types = get_argument_types();
  if (argument_types == nullptr) {
    return nullptr;
  }

  return resolve_method(
      /* type */ klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(return_type, argument_types),
      /* search */ MethodSearch::Any);
}

const DexTypeList* MT_NULLABLE LifecycleMethodCall::get_argument_types() const {
  DexTypeList::ContainerType argument_types;
  for (const auto& argument_type : argument_types_) {
    auto* type = DexType::get_type(argument_type);
    if (type == nullptr) {
      ERROR(1, "Could not find argument type `{}`.", argument_type);
      return nullptr;
    }
    argument_types.push_back(type);
  }

  return DexTypeList::make_type_list(std::move(argument_types));
}

bool LifecycleMethodCall::operator==(const LifecycleMethodCall& other) const {
  return method_name_ == other.method_name_ &&
      return_type_ == other.return_type_ &&
      argument_types_ == other.argument_types_ &&
      defined_in_derived_class_ == other.defined_in_derived_class_;
}

LifecycleMethod LifecycleMethod::from_json(const Json::Value& value) {
  std::string base_class_name =
      JsonValidation::string(value, "base_class_name");
  std::string method_name = JsonValidation::string(value, "method_name");
  if (JsonValidation::has_field(value, "callees")) {
    std::vector<LifecycleMethodCall> callees;
    for (const auto& callee :
         JsonValidation::nonempty_array(value, "callees")) {
      callees.push_back(LifecycleMethodCall::from_json(callee));
    }
    return LifecycleMethod(base_class_name, method_name, std::move(callees));
  } else if (JsonValidation::has_field(value, "control_flow_graph")) {
    JsonValidation::validate_object(value, "control_flow_graph");
    LifeCycleMethodGraph graph = LifeCycleMethodGraph::from_json(
        JsonValidation::object(value, "control_flow_graph"));
    return LifecycleMethod(base_class_name, method_name, std::move(graph));
  }
  throw JsonValidationError(
      value,
      /* field */ std::nullopt,
      "key `callees` or `control_flow_graph`");
}

void LifeCycleMethodGraph::validate(
    const DexClass* base_class,
    const ClassHierarchies& class_hierarchies) const {
  for (const auto& [node_name, node] : get_nodes()) {
    for (const auto& method_call : node.method_calls()) {
      method_call.validate(base_class, class_hierarchies);
    }

    for (const auto& successor_name : node.successors()) {
      if (get_node(successor_name) == nullptr) {
        throw LifecycleMethodValidationError(fmt::format(
            "Node `{}` has a successor `{}` that is not a valid node in the lifecycle graph.",
            node_name,
            successor_name));
      }
    }
  }

  std::unordered_set<std::string> visited;
  std::stack<std::string> stack;
  stack.push("entry");

  while (!stack.empty()) {
    const std::string current_node = stack.top();
    stack.pop();

    if (visited.count(current_node) == 0) {
      visited.insert(current_node);
      const auto* node = get_node(current_node);
      mt_assert(node != nullptr);
      for (const auto& successor : node->successors()) {
        stack.push(successor);
      }
    }
  }

  // Check if all nodes were visited
  if (visited.size() != get_nodes().size()) {
    throw LifecycleMethodValidationError(
        "Not all nodes are reachable from the entry point entry in the lifecycle graph.");
  }
}

bool LifecycleMethod::validate(
    const ClassHierarchies& class_hierarchies) const {
  const auto* base_class_type = DexType::get_type(base_class_name_);
  if (!base_class_type) {
    // Base type is not found in the APK. Config may still be valid, e.g. when
    // re-using configs across different APKs.
    WARNING(
        1,
        "Could not find type for base class name `{}`. Will skip creating life-cycle methods.",
        base_class_name_);
    return false;
  }

  const auto* base_class = type_class(base_class_type);
  if (!base_class) {
    // Base class is not a class, e.g. primitive type. Possibly an invalid
    // config, or the class definition doesn't exist in the APK, but the type
    // does. Loading the corresponding JAR helps with the latter, and it is
    // required for resolving callees to the right `DexMethod`.
    ERROR(
        1,
        "Could not convert base class type `{}` into DexClass.",
        base_class_type->str());
    return false;
  }

  if (const auto* callees =
          std::get_if<std::vector<LifecycleMethodCall>>(&body_)) {
    for (const auto& callee : *callees) {
      callee.validate(base_class, class_hierarchies);
    }
  } else {
    const auto& graph = std::get<LifeCycleMethodGraph>(body_);
    graph.validate(base_class, class_hierarchies);
  }

  return true;
}

void LifecycleMethod::create_methods(
    const ClassHierarchies& class_hierarchies,
    Methods& methods) {
  if (!validate(class_hierarchies)) {
    // Invalid life-cycle method. Do not create methods. Relevant warnings
    // should be logged by `validate()`.
    return;
  }

  // All DexMethods created by `LifecycleMethod` have the same signature:
  //   void <method_name_>(<arguments>)
  // The arguments are determined by the callees' arguments. This creates the
  // map of argument type -> location/position (first argument is at index 1).
  // The position corresponds to the register location containing the argument
  // in the DexMethod's code. The register location will be used to create the
  // invoke operation for methods that take a given DexType* as its argument.
  TypeIndexMap type_index_map;
  if (const auto* callees =
          std::get_if<std::vector<LifecycleMethodCall>>(&body_)) {
    for (const auto& callee : *callees) {
      map_argument_type_to_index(callee, type_index_map);
    }
  } else {
    const auto& graph = std::get<LifeCycleMethodGraph>(body_);
    for (const auto& [_, node] : graph.get_nodes()) {
      for (const auto& callee : node.method_calls()) {
        map_argument_type_to_index(callee, type_index_map);
      }
    }
  }

  auto* base_class_type = DexType::get_type(base_class_name_);
  // Base class should exist. See validate().
  mt_assert(base_class_type != nullptr);

  std::atomic<int> methods_created_count = 0;

  const auto& children = class_hierarchies.extends(base_class_type);
  std::unordered_set<const DexType*> final_children;
  for (const auto* child : children) {
    const auto& grandchildren = class_hierarchies.extends(child);
    if (!grandchildren.empty()) {
      continue;
    }
    // `type` can be `nullptr` if child type is not defined in the current
    // APK, i.e. type information not available. In such cases, the class also
    // does not exist in the current APK and there is no need to create a
    // life-cycle method for it.
    const auto* type = type_class(child);
    if (type && !::is_abstract(type)) {
      final_children.insert(child);
    }
  }
  LOG(3,
      "Found {} child(ren) for type `{}`. Creating life-cycle methods for {} leaf children",
      children.size(),
      base_class_name_,
      final_children.size());

  auto queue = sparta::work_queue<DexType*>([&](DexType* child) {
    if (const auto* dex_method = create_dex_method(child, type_index_map)) {
      ++methods_created_count;
      const auto* method = methods.create(dex_method);
      class_to_lifecycle_method_.emplace(child, method);
    }
  });
  for (const auto* final_child : final_children) {
    queue.add_item(const_cast<DexType*>(final_child));
  }
  queue.run_all();

  LOG(1,
      "Created {} life-cycle methods for classes inheriting from `{}`",
      methods_created_count,
      base_class_name_);
}

std::vector<const Method*> LifecycleMethod::get_methods_for_type(
    const DexType* base_receiver_type,
    const std::unordered_set<const DexType*>& local_derived_receiver_types,
    const ClassHierarchies& class_hierarchies) const {
  std::vector<const Method*> methods;

  // If the receiver type itself implements the method, return it. It is the
  // most-derived class, and should not have any children.
  // Note that `find` is not thread-safe, but `class_to_lifecycle_method_` is
  // read-only after the constructor completed.
  auto lifecycle_method = class_to_lifecycle_method_.find(base_receiver_type);
  if (lifecycle_method != class_to_lifecycle_method_.end()) {
    methods.push_back(lifecycle_method->second);
    return methods;
  }

  if (!local_derived_receiver_types.empty()) {
    for (const auto* child : local_derived_receiver_types) {
      auto child_lifecycle_method = class_to_lifecycle_method_.find(child);
      if (child_lifecycle_method != class_to_lifecycle_method_.end()) {
        methods.push_back(child_lifecycle_method->second);
      }
    }
  } else {
    // Intermediate receiver_type. Look for most-derived/final classes.
    const auto& children = class_hierarchies.extends(base_receiver_type);
    for (const auto* child : children) {
      auto child_lifecycle_method = class_to_lifecycle_method_.find(child);
      if (child_lifecycle_method != class_to_lifecycle_method_.end()) {
        methods.push_back(child_lifecycle_method->second);
      }
    }
  }

  return methods;
}

bool LifecycleMethod::operator==(const LifecycleMethod& other) const {
  return base_class_name_ == other.base_class_name_ &&
      method_name_ == other.method_name_ && body_ == other.body_;
}

const DexMethod* MT_NULLABLE LifecycleMethod::create_dex_method(
    DexType* klass,
    const TypeIndexMap& type_index_map) {
  auto method_creator = MethodCreator(
      /* class */ klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(type::_void(), get_argument_types(type_index_map)),
      /* access */ DexAccessFlags::ACC_PUBLIC);

  auto* dex_klass = type_class(klass);
  mt_assert(dex_klass != nullptr);

  // Create the lifecycle method depending on the state transition model
  const DexMethod* MT_NULLABLE new_method;
  if (const auto* callees =
          std::get_if<std::vector<LifecycleMethodCall>>(&body_)) {
    new_method = create_dex_method_from_callees(
        dex_klass, type_index_map, std::move(method_creator), *callees);
  } else {
    const auto& graph = std::get<LifeCycleMethodGraph>(body_);
    new_method = create_dex_method_from_graph(
        dex_klass, type_index_map, std::move(method_creator), graph);
  }

  // Directly return if method creation was skipped
  if (new_method == nullptr) {
    return new_method;
  }

  LOG(5,
      "Created life-cycle method `{}` for class: `{}`",
      show(new_method),
      show(klass));
  LOG(5,
      "Generated method body:\n{}",
      Method::show_control_flow_graph(new_method->get_code()->cfg()));

  return new_method;
}

const DexTypeList* LifecycleMethod::get_argument_types(
    const TypeIndexMap& type_index_map) {
  // While the register locations for the arguments start at 1, the actual
  // argument index for the method's prototype start at index 0.
  DexTypeList::ContainerType argument_types(type_index_map.size(), nullptr);
  for (const auto& [type, pos] : type_index_map) {
    argument_types.at(pos - 1) = type;
  }

  return DexTypeList::make_type_list(std::move(argument_types));
}

} // namespace marianatrench
