/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <boost/iterator/transform_iterator.hpp>

#include <mariana-trench/Kind.h>
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
  struct StringPairHash {
    std::size_t operator()(
        const std::pair<std::string, std::string>& string_tuple) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, string_tuple.first);
      boost::hash_combine(seed, string_tuple.second);
      return seed;
    }
  };

 public:
  Kinds() = default;
  Kinds(const Kinds&) = delete;
  Kinds(Kinds&&) = delete;
  Kinds& operator=(const Kinds&) = delete;
  Kinds& operator=(Kinds&&) = delete;
  ~Kinds() = default;

  const NamedKind* get(const std::string& name) const;

  const PartialKind* get_partial(
      const std::string& name,
      const std::string& label) const;

  const TriggeredPartialKind* get_triggered(
      const PartialKind* partial_kind) const;

  std::vector<const Kind*> kinds() const;

  static const Kind* artificial_source();

 private:
  UniquePointerFactory<std::string, NamedKind> named_;
  UniquePointerFactory<
      std::pair<std::string, std::string>,
      PartialKind,
      StringPairHash>
      partial_;
  UniquePointerFactory<const PartialKind*, TriggeredPartialKind>
      triggered_partial_;
};

} // namespace marianatrench
