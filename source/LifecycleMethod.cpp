/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/WorkQueue.h>

#include <Creators.h>
#include <Resolver.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

LifecycleMethodCall LifecycleMethodCall::from_json(const Json::Value& value) {
  auto method_name = JsonValidation::string(value, "method_name");
  auto return_type = JsonValidation::string(value, "return_type");
  std::vector<std::string> argument_types;
  for (const auto& argument_type :
       JsonValidation::null_or_array(value, "argument_types")) {
    argument_types.emplace_back(JsonValidation::string(argument_type));
  }

  return LifecycleMethodCall(method_name, return_type, argument_types);
}

DexMethodRef* MT_NULLABLE
LifecycleMethodCall::get_dex_method(DexType* klass) const {
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

  const auto* dex_klass = type_class(klass);
  mt_assert(dex_klass != nullptr);
  return resolve_virtual(
      /* type */ dex_klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(return_type, argument_types));
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
      argument_types_ == other.argument_types_;
}

LifecycleMethod LifecycleMethod::from_json(const Json::Value& value) {
  auto base_class_name = JsonValidation::string(value, "base_class_name");
  auto method_name = JsonValidation::string(value, "method_name");
  std::vector<LifecycleMethodCall> callees;
  for (const auto& callee : JsonValidation::nonempty_array(value, "callees")) {
    callees.emplace_back(LifecycleMethodCall::from_json(callee));
  }

  return LifecycleMethod(base_class_name, method_name, callees);
}

void LifecycleMethod::create_methods(
    const ClassHierarchies& class_hierarchies,
    Methods& methods) {
  // All DexMethods created by `LifecycleMethod` have the same signature:
  //   void <method_name_>(<arguments>)
  // The arguments are determined by the callees' arguments. This creates the
  // map of argument type -> location/position (first argument is at index 1).
  // The position corresponds to the register location containing the argument
  // in the DexMethod's code. The register location will be used to create the
  // invoke operation for methods that take a given DexType* as its argument.
  TypeIndexMap type_index_map;
  for (const auto& callee : callees_) {
    const auto* type_list = callee.get_argument_types();
    if (type_list == nullptr) {
      ERROR(1, "Callee `{}` has invalid argument types.", callee.to_string());
      continue;
    }
    for (auto* type : *type_list) {
      type_index_map.emplace(type, type_index_map.size() + 1);
    }
  }

  auto* MT_NULLABLE base_class_type = DexType::get_type(base_class_name_);
  if (!base_class_type) {
    WARNING(
        1,
        "Could not find type for base class name `{}`. Will skip creating life-cycle methods.",
        base_class_name_);
    return;
  }

  std::atomic<int> methods_created_count = 0;

  const auto& children = class_hierarchies.extends(base_class_type);
  std::unordered_set<const DexType*> final_children;
  for (const auto* child : children) {
    const auto& grandchildren = class_hierarchies.extends(child);
    if (grandchildren.empty()) {
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
      method_name_ == other.method_name_ && callees_ == other.callees_;
}

const DexMethod* MT_NULLABLE LifecycleMethod::create_dex_method(
    DexType* klass,
    const TypeIndexMap& type_index_map) {
  auto method = MethodCreator(
      /* class */ klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(type::_void(), get_argument_types(type_index_map)),
      /* access */ DexAccessFlags::ACC_PRIVATE);

  auto this_location = method.get_local(0);
  auto* main_block = method.get_main_block();
  mt_assert(main_block != nullptr);

  int callee_count = 0;
  for (const auto& callee : callees_) {
    auto* dex_method = callee.get_dex_method(klass);
    if (!dex_method) {
      WARNING(
          1,
          "Could not find method `{}` within ancestor class hierarchy of `{}`.",
          callee.to_string(),
          klass->str());
      continue;
    }

    ++callee_count;

    std::vector<Location> invoke_with_registers{this_location};
    auto* type_list = callee.get_argument_types();
    // This should have been verified at the start of `create_methods`
    mt_assert(type_list != nullptr);
    for (auto* type : *type_list) {
      auto argument_register = method.get_local(type_index_map.at(type));
      invoke_with_registers.push_back(argument_register);
    }
    main_block->invoke(
        IROpcode::OPCODE_INVOKE_VIRTUAL, dex_method, invoke_with_registers);
  }

  if (callee_count < 2) {
    // The point of life-cycle methods is to find flows where tainted member
    // variables flow from one callee into another. If only one (or no) method
    // is overridden, there is no need to create the artificial method. If this
    // happens, it is likely the life-cycle configuration is incorrect.
    WARNING(
        1,
        "Skipped creating life-cycle method for class `{}`. Reason: Insufficient callees.",
        klass->str());
    return nullptr;
  }

  // The CFG needs to be built for the call graph to be constructed later.
  auto* new_method = method.create();
  mt_assert(new_method != nullptr && new_method->get_code() != nullptr);
  IRCode* code = new_method->get_code();
  code->build_cfg();
  code->cfg().calculate_exit_block();
  LOG(5, "Created life-cycle method `{}`", new_method->str());

  return new_method;
}

const DexTypeList* LifecycleMethod::get_argument_types(
    const TypeIndexMap& type_index_map) {
  // While the register locations for the arguments start at 1, the actual
  // argument index for the method's prototype start at index 0.
  int num_args = type_index_map.size();
  DexTypeList::ContainerType argument_types(num_args, nullptr);
  for (const auto& [type, pos] : type_index_map) {
    mt_assert(pos > 0); // 0 is for "this", should not be in the map.
    mt_assert(static_cast<size_t>(pos) - 1 < argument_types.max_size());
    argument_types[pos - 1] = type;
  }

  return DexTypeList::make_type_list(std::move(argument_types));
}

} // namespace marianatrench
