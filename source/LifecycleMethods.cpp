/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Creators.h>
#include <SpartaWorkQueue.h>

#include <fmt/format.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

namespace {

/**
 * Represents an invoke operation in `LifecycleMethod` to a specific callee.
 * These are typically methods in the base class that can be overridden by
 * children.
 */
class LifecycleMethodCall {
 public:
  LifecycleMethodCall(
      std::string method_name,
      std::string return_type,
      std::vector<std::string> argument_types)
      : method_name_(std::move(method_name)),
        return_type_(std::move(return_type)),
        argument_types_(std::move(argument_types)) {}

  LifecycleMethodCall(const LifecycleMethodCall&) = default;
  LifecycleMethodCall(LifecycleMethodCall&&) = delete;
  LifecycleMethodCall& operator=(const LifecycleMethodCall&) = default;
  LifecycleMethodCall& operator=(LifecycleMethodCall&&) = delete;
  ~LifecycleMethodCall() = default;

  /**
   * Gets the DexMethod for the overridden call in `klass`. Returns `nullptr`
   * if `klass` does not override this method, or if its definition is invalid,
   * e.g. unrecognized types.
   */
  DexMethodRef* MT_NULLABLE get_dex_method(DexType* klass) const;

  const DexTypeList* MT_NULLABLE get_argument_types() const;

  std::string to_string() const {
    return fmt::format(
        "{}({}){}", method_name_, fmt::join(argument_types_, ""), return_type_);
  }

 private:
  std::string method_name_;
  std::string return_type_;
  std::vector<std::string> argument_types_;
};

/**
 * A life-cycle method represents a collection of artificial DexMethods that
 * simulate the life-cycle of a class.
 *
 * An example would be the base_class_name: Landroid/app/Activity;
 * To represent that life-cycle, the callees should be calls to:
 *   this.onCreate(), this.onStart() ... this.onStop(), this.onDestroy().
 *
 * Fields:
 *
 * `base_class_name`: The class whose life-cycle needs to be simulated.
 * Derived classes that do not have children will have an artificial class
 * method created that invokes the relevant life-cycle methods (if the derived
 * class overrides them).
 *
 * `method_name`: Name of the artificial method, e.g. "artificial_method". If an
 * issue is found, this will be part of the issue's full callable name (which
 * will also include other things like the class name and args).
 *
 * `callees`: List of life-cycle methods to be called, given in the order
 * which they are called. See `LifecycleMethodCall`. If any callee accepts
 * an argument of some type `T`, the artificial method will be created to accept
 * an argument of type `T` and will pass that argument into the corresponding
 * callee. The return value of the callees are currently ignored.
 */
class LifecycleMethod {
 public:
  explicit LifecycleMethod(
      std::string base_class_name,
      std::string method_name,
      std::vector<LifecycleMethodCall> callees)
      : base_class_name_(std::move(base_class_name)),
        method_name_(std::move(method_name)),
        callees_(std::move(callees)) {}

  LifecycleMethod(const LifecycleMethod&) = default;
  LifecycleMethod(LifecycleMethod&&) = delete;
  LifecycleMethod& operator=(const LifecycleMethod&) = default;
  LifecycleMethod& operator=(LifecycleMethod&&) = delete;
  ~LifecycleMethod() = default;

  /**
   * Creates the relevant dex methods. These methods are returned and also
   * added to context.methods.
   */
  ConcurrentSet<const DexMethod*> create_methods(Context& context);

 private:
  const DexMethod* MT_NULLABLE
  create_dex_method(Context& context, DexType* klass) const;

  const DexTypeList* get_argument_types() const;

  std::string base_class_name_;
  std::string method_name_;
  std::vector<LifecycleMethodCall> callees_;
  std::unordered_map<DexType*, int> type_index_map_;
};

} // namespace

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

  return DexMethod::get_method(
      /* type */ klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(return_type, argument_types));
}

const DexTypeList* MT_NULLABLE LifecycleMethodCall::get_argument_types() const {
  std::deque<DexType*> argument_types;
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

ConcurrentSet<const DexMethod*> LifecycleMethod::create_methods(
    Context& context) {
  // Initialize `type_index_map_`.
  // All DexMethods created by `LifecycleMethod` have the same signature:
  //   void <method_name_>(<arguments>)
  // The arguments are determined by the callees' arguments. This creates the
  // map of argument type -> location/position (first argument is at index 1).
  // The position corresponds to the register location containing the argument
  // in the DexMethod's code. The register location will be used to create the
  // invoke operation for methods that take a given DexType* as its argument.
  for (const auto& callee : callees_) {
    const auto* type_list = callee.get_argument_types();
    if (type_list == nullptr) {
      ERROR(1, "Callee `{}` has invalid argument types.", callee.to_string());
      continue;
    }
    for (auto* type : type_list->get_type_list()) {
      type_index_map_.emplace(type, type_index_map_.size() + 1);
    }
  }

  ConcurrentSet<const DexMethod*> result;

  auto* MT_NULLABLE base_class_type = DexType::get_type(base_class_name_);
  if (!base_class_type) {
    WARNING(
        1,
        "Could not find type for base class name `{}`. Will skip creating life-cycle methods.",
        base_class_name_);
    return result;
  }

  const auto& children = context.class_hierarchies->extends(base_class_type);
  std::unordered_set<const DexType*> final_children;
  for (const auto* child : children) {
    const auto& grandchildren = context.class_hierarchies->extends(child);
    if (grandchildren.empty()) {
      final_children.insert(child);
    }
  }
  LOG(3,
      "Found {} child(ren) for type `{}`. Creating life-cycle methods for {} leaf child(ren)",
      children.size(),
      base_class_name_,
      final_children.size());

  auto queue = sparta::work_queue<DexType*>([&](DexType* child) {
    const auto* method = create_dex_method(context, child);
    if (method != nullptr) {
      result.insert(method);
    }
  });
  for (const auto* final_child : final_children) {
    queue.add_item(const_cast<DexType*>(final_child));
  }
  queue.run_all();

  LOG(1,
      "Created {} life-cycle methods for classes inheriting from `{}`",
      result.size(),
      base_class_name_);
  return result;
}

const DexMethod* MT_NULLABLE
LifecycleMethod::create_dex_method(Context& context, DexType* klass) const {
  auto method = MethodCreator(
      /* class */ klass,
      /* name */ DexString::make_string(method_name_),
      /* proto */
      DexProto::make_proto(type::_void(), get_argument_types()),
      /* access */ DexAccessFlags::ACC_PRIVATE);

  auto this_location = method.get_local(0);
  auto* main_block = method.get_main_block();
  mt_assert(main_block != nullptr);

  int num_callees_found = 0;
  for (const auto& callee : callees_) {
    auto* dex_method = callee.get_dex_method(klass);
    if (!dex_method) {
      // This can be null if `klass` does not override the method, in which
      // case, it will not be invoked.
      continue;
    }

    ++num_callees_found;

    std::vector<Location> invoke_with_registers{this_location};
    auto* type_list = callee.get_argument_types();
    // This should have been verified at the start of `create_methods`
    mt_assert(type_list != nullptr);
    for (auto* type : type_list->get_type_list()) {
      auto argument_register = method.get_local(type_index_map_.at(type));
      invoke_with_registers.push_back(argument_register);
    }
    main_block->invoke(
        IROpcode::OPCODE_INVOKE_DIRECT, dex_method, invoke_with_registers);
  }

  if (num_callees_found < 2) {
    // The point of life-cycle methods is to find flows where tainted member
    // variables flow from one callee into another. If only one (or no) method
    // is overridden, there is no need to create the artificial method.
    LOG(5,
        "Skipped creating life-cycle method for class `{}`. Reason: Insufficient callees.",
        klass->str());
    return nullptr;
  }

  // The CFG needs to be built for the call graph to be constructed later.
  auto* new_method = method.create();
  mt_assert(new_method != nullptr && new_method->get_code() != nullptr);
  new_method->get_code()->build_cfg();
  LOG(5, "Creating life-cycle method `{}`", new_method->str());
  context.methods->create(new_method);

  return new_method;
}

const DexTypeList* LifecycleMethod::get_argument_types() const {
  // While the register locations for the arguments start at 1, the actual
  // argument index for the method's prototype start at index 0.
  int num_args = type_index_map_.size();
  std::deque<DexType*> argument_types(num_args, nullptr);
  for (const auto& [type, pos] : type_index_map_) {
    mt_assert(pos > 0); // 0 is for "this", should not be in the map.
    mt_assert(static_cast<size_t>(pos) - 1 < argument_types.max_size());
    argument_types[pos - 1] = type;
  }

  return DexTypeList::make_type_list(std::move(argument_types));
}

void LifecycleMethods::run(Context& context) {
  // Hard-coded list of supported life-cycle methods. Consider making this
  // configurable via some JSON input.
  std::vector<LifecycleMethod> methods = {LifecycleMethod(
      /* base_class_name */ "Landroidx/fragment/app/FragmentActivity;",
      /* method_name */ "activity_lifecycle_wrapper",
      /* callees */
      {LifecycleMethodCall(
           /* method_name */ "onCreate",
           /* return_type */ "V", // DexType for void
           /* argument_types */ {"Landroid/os/Bundle;"}),
       LifecycleMethodCall(
           /* method_name */ "onStart",
           /* return_type */ "V",
           /* argument_types */ {})})};

  for (auto& method : methods) {
    method.create_methods(context);
  }
}

} // namespace marianatrench
