/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fmt/format.h>
#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

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
  LifecycleMethodCall(LifecycleMethodCall&&) = default;
  LifecycleMethodCall& operator=(const LifecycleMethodCall&) = default;
  LifecycleMethodCall& operator=(LifecycleMethodCall&&) = delete;
  ~LifecycleMethodCall() = default;

  static LifecycleMethodCall from_json(const Json::Value& value);

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

  bool operator==(const LifecycleMethodCall& other) const;

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
 private:
  using TypeIndexMap = std::unordered_map<DexType*, int>;

 public:
  explicit LifecycleMethod(
      std::string base_class_name,
      std::string method_name,
      std::vector<LifecycleMethodCall> callees)
      : base_class_name_(std::move(base_class_name)),
        method_name_(std::move(method_name)),
        callees_(std::move(callees)) {}

  LifecycleMethod(const LifecycleMethod&) = default;
  LifecycleMethod(LifecycleMethod&&) = default;
  LifecycleMethod& operator=(const LifecycleMethod&) = default;
  LifecycleMethod& operator=(LifecycleMethod&&) = delete;
  ~LifecycleMethod() = default;

  static LifecycleMethod from_json(const Json::Value& value);

  /**
   * Creates the relevant dex methods. These methods are returned and also
   * added to context.methods.
   */
  ConcurrentSet<const DexMethod*> create_methods(Context& context);

  bool operator==(const LifecycleMethod& other) const;

 private:
  const DexMethod* MT_NULLABLE create_dex_method(
      Context& context,
      DexType* klass,
      const TypeIndexMap& type_index_map) const;

  const DexTypeList* get_argument_types(const TypeIndexMap&) const;

  std::string base_class_name_;
  std::string method_name_;
  std::vector<LifecycleMethodCall> callees_;
};

} // namespace marianatrench
