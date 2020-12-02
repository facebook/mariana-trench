// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <boost/functional/hash.hpp>
#include <json/json.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>

namespace marianatrench {

class Position final {
 public:
  Position(const std::string* MT_NULLABLE path, int line);

  Position(const Position&) = default;
  Position(Position&&) = default;
  Position& operator=(const Position&) = default;
  Position& operator=(Position&&) = default;
  ~Position() = default;

  bool operator==(const Position& other) const;
  bool operator!=(const Position& other) const;

  const std::string* MT_NULLABLE path() const {
    return path_;
  }

  int line() const {
    return line_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Position& kind);

  static const Position* from_json(const Json::Value& value, Context& context);
  Json::Value to_json(bool with_path = true) const;

 private:
  friend struct std::hash<Position>;

  const std::string* MT_NULLABLE path_;
  int line_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::Position> {
  std::size_t operator()(const marianatrench::Position& position) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, position.path_);
    boost::hash_combine(seed, position.line_);
    return seed;
  }
};
