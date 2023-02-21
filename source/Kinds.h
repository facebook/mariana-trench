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
#include <mariana-trench/TriggeredPartialKind.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

/**
 * The kind factory.
 */
class Kinds final {
 private:
  template <class T1, class T2>
  struct PairHash {
    std::size_t operator()(const std::pair<T1, T2>& tuple) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, tuple.first);
      boost::hash_combine(seed, tuple.second);
      return seed;
    }
  };

 public:
  Kinds();

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Kinds)

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

  std::vector<const Kind*> kinds() const;

  static const Kind* artificial_source();

 private:
  UniquePointerFactory<std::string, NamedKind> named_;
  std::unique_ptr<LocalReturnKind> local_return_;
  UniquePointerFactory<ParameterPosition, LocalArgumentKind> local_argument_;
  const LocalArgumentKind* local_receiver_;
  UniquePointerFactory<
      std::pair<std::string, std::string>,
      PartialKind,
      PairHash<std::string, std::string>>
      partial_;
  UniquePointerFactory<
      std::pair<const PartialKind*, const MultiSourceMultiSinkRule*>,
      TriggeredPartialKind,
      PairHash<const PartialKind*, const MultiSourceMultiSinkRule*>>
      triggered_partial_;
};

} // namespace marianatrench
