/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <mariana-trench/IncludeMacros.h>

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
  Fields() = default;

  explicit Fields(const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Fields)

  const Field* get(const DexField* field) const;

  Iterator begin() const;

  Iterator end() const;

  std::size_t size() const;

 private:
  Set set_;
};

} // namespace marianatrench
