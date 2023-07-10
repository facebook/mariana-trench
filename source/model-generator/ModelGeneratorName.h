/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>

#include <boost/functional/hash.hpp>
#include <json/json.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/*
 * Unique name of a model generator.
 * It can also represent a sub-generator within the generator.
 */
class ModelGeneratorName final {
 public:
  explicit ModelGeneratorName(
      std::string identifier,
      std::optional<std::string> part);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ModelGeneratorName)

  bool operator==(const ModelGeneratorName& other) const;

  const std::string& identifier() const {
    return identifier_;
  }

  const std::optional<std::string>& part() const {
    return part_;
  }

  Json::Value to_json() const;

 private:
  friend struct std::hash<ModelGeneratorName>;
  friend std::ostream& operator<<(
      std::ostream& out,
      const ModelGeneratorName& name);

 private:
  std::string identifier_;
  std::optional<std::string> part_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::ModelGeneratorName> {
  std::size_t operator()(const marianatrench::ModelGeneratorName& name) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, name.identifier_);
    if (name.part_) {
      boost::hash_combine(seed, *name.part_);
    }
    return seed;
  }
};
