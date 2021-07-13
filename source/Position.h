/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <boost/functional/hash.hpp>
#include <json/json.h>

#include <IRInstruction.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>

namespace {

constexpr int k_unknown_start = -1;
constexpr int k_unknown_end = -1;
constexpr int k_unknown_line = -1;

} // namespace

namespace marianatrench {

class Position final {
 public:
  Position(
      const std::string* MT_NULLABLE path,
      int line,
      std::optional<Root> port = {},
      const IRInstruction* instruction = nullptr,
      int start = k_unknown_start,
      int end = k_unknown_end);

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

  std::optional<Root> port() const {
    return port_;
  }

  const IRInstruction* instruction() const {
    return instruction_;
  }

  int start() const {
    return start_;
  }

  int end() const {
    return end_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Position& kind);

  static const Position* from_json(const Json::Value& value, Context& context);
  Json::Value to_json(bool with_path = true) const;

  bool overlaps(const Position& other) const;

 private:
  friend struct std::hash<Position>;

  const std::string* MT_NULLABLE path_;
  int line_;
  // The return value or argument through which taint is flowing in the
  // IRInstruction on the given line
  std::optional<Root> port_;
  const IRInstruction* instruction_;
  // These describe the portion of the line (i.e, columns) of source code to
  // highlight in the UI
  int start_;
  int end_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::Position> {
  std::size_t operator()(const marianatrench::Position& position) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, position.path_);
    boost::hash_combine(seed, position.line_);
    if (position.port_) {
      boost::hash_combine(seed, position.port_->encode());
    }
    boost::hash_combine(seed, position.instruction_);
    boost::hash_combine(seed, position.start_);
    boost::hash_combine(seed, position.end_);
    return seed;
  }
};
