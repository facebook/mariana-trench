/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

Position::Position(
    const std::string* path,
    int line,
    std::optional<Root> port,
    const IRInstruction* instruction,
    int start,
    int end)
    : path_(path),
      line_(line),
      port_(port),
      instruction_(instruction),
      start_(start),
      end_(end) {}

bool Position::operator==(const Position& other) const {
  return path_ == other.path_ && line_ == other.line_ && port_ == other.port_ &&
      instruction_ == other.instruction_ && start_ == other.start_ &&
      end_ == other.end_;
}

bool Position::operator!=(const Position& other) const {
  return !this->operator==(other);
}

std::string Position::to_string() const {
  std::string out = "Position(";
  if (path_ != nullptr) {
    out.append(fmt::format("path=`{}`,", show(path_)));
  }
  if (line_ != k_unknown_line) {
    out.append("line={}", line_);
  }
  out.append(")");

  return out;
}

std::ostream& operator<<(std::ostream& out, const Position& position) {
  return out << position.to_string();
}

const Position* Position::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  int line = k_unknown_line;
  if (value.isMember("line")) {
    line = JsonValidation::integer(value, /* field */ "line");
  }

  std::optional<std::string> path;
  if (value.isMember("path")) {
    path = JsonValidation::string(value, /* field */ "path");
  }

  int start = k_unknown_start;
  if (value.isMember("start")) {
    start = JsonValidation::integer(value, /* field */ "start");
  }

  int end = k_unknown_end;
  if (value.isMember("end")) {
    end = JsonValidation::integer(value, /* field */ "end");
  }

  return context.positions->get(
      /* path */ path,
      /* line */ line,
      /* port */ std::nullopt,
      /* instruction */ nullptr,
      /* start */ start,
      /* end */ end);
}

Json::Value Position::to_json(bool with_path) const {
  auto value = Json::Value(Json::objectValue);
  if (line_ != k_unknown_line) {
    value["line"] = Json::Value(line_);
  }
  if (with_path && path_) {
    value["path"] = Json::Value(*path_);
  }
  if (start_ != k_unknown_start) {
    value["start"] = Json::Value(start_);
  }
  if (end_ != k_unknown_end) {
    value["end"] = Json::Value(end_);
  }
  return value;
}

bool Position::overlaps(const Position& other) const {
  mt_assert(path_ != nullptr);
  mt_assert(other.path() != nullptr);
  if (path_ != other.path() || line_ != other.line()) {
    return false;
  }
  return start_ <= other.end() && other.start() <= end_;
}

} // namespace marianatrench
