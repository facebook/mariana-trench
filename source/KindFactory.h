/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <boost/iterator/transform_iterator.hpp>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/LocalArgumentKind.h>
#include <mariana-trench/LocalReturnKind.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/TransformKind.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/TriggeredPartialKind.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

/**
 * The kind factory.
 */
class KindFactory final {
 private:
  template <typename... Ts>
  struct TupleHash {
    std::size_t operator()(const std::tuple<Ts...>& tuple) const {
      return boost::hash_value(tuple);
    }
  };

 public:
  KindFactory();

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(KindFactory)

  const NamedKind* get(const std::string& name) const;

  const PartialKind* get_partial(
      const std::string& name,
      const std::string& label) const;

  const TriggeredPartialKind* get_triggered(
      const PartialKind* partial_kind,
      const MultiSourceMultiSinkRule* rule) const;

  const LocalReturnKind* local_return() const;

  const LocalArgumentKind* local_receiver() const;

  const LocalArgumentKind* local_argument(ParameterPosition parameter) const;

  const TransformKind* transform_kind(
      const Kind* base_kind,
      const TransformList* MT_NULLABLE local_transforms,
      const TransformList* MT_NULLABLE global_transforms) const;

  std::vector<const Kind*> kinds() const;

  static const KindFactory& singleton();

 private:
  UniquePointerFactory<std::string, NamedKind> named_;
  std::unique_ptr<LocalReturnKind> local_return_;
  UniquePointerFactory<ParameterPosition, LocalArgumentKind> local_argument_;
  const LocalArgumentKind* local_receiver_;
  UniquePointerFactory<
      std::tuple<std::string, std::string>,
      PartialKind,
      TupleHash<std::string, std::string>>
      partial_;
  UniquePointerFactory<
      std::tuple<const PartialKind*, const MultiSourceMultiSinkRule*>,
      TriggeredPartialKind,
      TupleHash<const PartialKind*, const MultiSourceMultiSinkRule*>>
      triggered_partial_;
  UniquePointerFactory<
      std::tuple<
          const Kind*,
          const TransformList * MT_NULLABLE,
          const TransformList * MT_NULLABLE>,
      TransformKind,
      TupleHash<
          const Kind*,
          const TransformList * MT_NULLABLE,
          const TransformList * MT_NULLABLE>>
      transforms_;
};

} // namespace marianatrench
