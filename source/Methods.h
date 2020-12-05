/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/iterator/transform_iterator.hpp>

#include <ConcurrentContainers.h>
#include <DexClass.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

/**
 * The method factory.
 */
class Methods final {
 private:
  using Set = InsertOnlyConcurrentSet<Method>;

  struct GetPointer {
    const Method* operator()(const Method& method) const {
      return &method;
    }
  };

 public:
  using Iterator =
      boost::transform_iterator<GetPointer, typename Set::const_iterator>;

 public:
  Methods();

  explicit Methods(const DexStoresVector& stores);

  Methods(const Methods&) = delete;
  Methods(Methods&&) = delete;
  Methods& operator=(const Methods&) = delete;
  Methods& operator=(Methods&&) = delete;
  ~Methods() = default;

  /* Get or create a method with the given parameter type overrides */
  const Method* create(
      const DexMethod* method,
      ParameterTypeOverrides parameter_type_overrides = {});

  /**
   * Get the method with the given parameter type overrides.
   *
   * Throws an exception if the method does not exist.
   */
  const Method* get(
      const DexMethod* method,
      ParameterTypeOverrides parameter_type_overrides = {}) const;

  /**
   * Get the method with the given name.
   *
   * Returns `nullptr` if the method does not exist.
   */
  const Method* MT_NULLABLE get(const std::string& name) const;

  /**
   * Iterating on the container while calling `create` concurrently is unsafe.
   */
  Iterator begin() const;

  Iterator end() const;

  std::size_t size() const;

 private:
  Set set_;
};

} // namespace marianatrench
