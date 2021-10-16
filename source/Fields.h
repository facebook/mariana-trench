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
#include <DexStore.h>

#include <mariana-trench/Field.h>

namespace marianatrench {

/**
 * The Field factory.
 */
class Fields final {
 private:
  using Set = InsertOnlyConcurrentSet<Field>;

  struct GetPointer {
    const Field* operator()(const Field& field) const {
      return &field;
    }
  };

 public:
  using Iterator =
      boost::transform_iterator<GetPointer, typename Set::const_iterator>;

 public:
  Fields();

  explicit Fields(const DexStoresVector& stores);

  Fields(const Fields&) = delete;
  Fields(Fields&&) = delete;
  Fields& operator=(const Fields&) = delete;
  Fields& operator=(Fields&&) = delete;
  ~Fields() = default;

  const Field* get(const DexField* field) const;

  Iterator begin() const;

  Iterator end() const;

  std::size_t size() const;

 private:
  Set set_;
};

} // namespace marianatrench
