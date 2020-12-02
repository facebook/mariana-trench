// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <boost/iterator/transform_iterator.hpp>

#include <mariana-trench/Kind.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

/**
 * The kind factory.
 */
class Kinds final {
 private:
  struct ExposeKind {
    const Kind* operator()(
        const std::pair<const std::string, const Kind*>& pair) const {
      return pair.second;
    }
  };

 public:
  // C++ container concept member types
  using iterator = boost::transform_iterator<
      ExposeKind,
      UniquePointerFactory<std::string, Kind>::const_iterator>;
  using const_iterator = iterator;
  using value_type = const Kind*;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Kind*;
  using const_pointer = typename iterator::pointer;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);

 public:
  Kinds() = default;
  Kinds(const Kinds&) = delete;
  Kinds(Kinds&&) = delete;
  Kinds& operator=(const Kinds&) = delete;
  Kinds& operator=(Kinds&&) = delete;
  ~Kinds() = default;

  const Kind* get(const std::string& name) const;

  static const Kind* artificial_source();

  iterator begin() const {
    return boost::make_transform_iterator(factory_.begin(), ExposeKind());
  }

  iterator end() const {
    return boost::make_transform_iterator(factory_.end(), ExposeKind());
  }

 private:
  UniquePointerFactory<std::string, Kind> factory_;
};

} // namespace marianatrench
