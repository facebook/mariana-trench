/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

std::optional<ParameterPosition> parse_parameter_position(
    const std::string& string) {
  if (string.empty() || string[0] == '-') {
    // `std::stoul` would wrap around.
    return std::nullopt;
  }

  try {
    std::size_t processed = 0;
    ParameterPosition parameter = std::stoul(string, &processed);

    // Check that there are no remaining (non-digit) characters.
    if (processed != string.size()) {
      return std::nullopt;
    }

    return parameter;
  } catch (const std::invalid_argument&) {
    return std::nullopt;
  } catch (const std::out_of_range&) {
    return std::nullopt;
  }
}

PathElement::PathElement(PathElement::Kind kind, const DexString* element)
    : value_(Element{element, static_cast<KindEncoding>(kind)}) {}

PathElement PathElement::field(const DexString* name) {
  return PathElement(Kind::Field, name);
}

PathElement PathElement::field(std::string_view name) {
  return field(DexString::make_string(name));
}

PathElement PathElement::index(const DexString* name) {
  return PathElement(Kind::Index, name);
}

PathElement PathElement::index(std::string_view name) {
  return index(DexString::make_string(name));
}

PathElement PathElement::any_index() {
  return PathElement{Kind::AnyIndex, nullptr};
}

PathElement PathElement::index_from_value_of(Root root) {
  mt_assert(root.is_argument());
  return PathElement{
      Kind::IndexFromValueOf,
      DexString::make_string(std::to_string(root.parameter_position()))};
}

ParameterPosition PathElement::parameter_position() const {
  mt_assert(is_index_from_value_of());

  const auto* position_string = name();
  mt_assert(position_string != nullptr);

  auto position = parse_parameter_position(position_string->str_copy());
  mt_assert(position.has_value());

  return *position;
}

std::string PathElement::str() const {
  switch (kind()) {
    case PathElement::Kind::Field:
      return fmt::format(".{}", show(name()));

    case PathElement::Kind::Index:
      return fmt::format("[{}]", show(name()));

    case PathElement::Kind::AnyIndex:
      return "[*]";

    case PathElement::Kind::IndexFromValueOf:
      return fmt::format(
          "[<{}>]", Root(Root::Kind::Argument, parameter_position()));

    default:
      mt_unreachable();
  }
}

PathElement PathElement::resolve_index_from_value_of(
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  if (!is_index_from_value_of()) {
    return *this;
  }

  auto position = parameter_position();
  if (position < source_constant_arguments.size()) {
    if (auto value = source_constant_arguments[position]) {
      return PathElement::index(*value);
    }
  } else {
    WARNING(
        1,
        fmt::format(
            "Invalid argument index {} provided for index_from_value_of path element.",
            position));
  }

  return PathElement::any_index();
}

std::ostream& operator<<(std::ostream& out, const PathElement& path_element) {
  return out << path_element.str();
}

bool Path::operator==(const Path& other) const {
  return elements_ == other.elements_;
}

void Path::append(Element element) {
  elements_.push_back(element);
}

void Path::extend(const Path& path) {
  elements_.insert(
      elements_.end(), path.elements_.begin(), path.elements_.end());
}

void Path::pop_back() {
  mt_assert(!elements_.empty());
  elements_.pop_back();
}

void Path::truncate(std::size_t max_size) {
  if (elements_.size() > max_size) {
    elements_.erase(std::next(elements_.begin(), max_size), elements_.end());
  }
}

bool Path::is_prefix_of(const Path& other) const {
  auto result = std::mismatch(
      elements_.begin(),
      elements_.end(),
      other.elements_.begin(),
      other.elements_.end());
  return result.first == elements_.end();
}

void Path::reduce_to_common_prefix(const Path& other) {
  auto result = std::mismatch(
      elements_.begin(),
      elements_.end(),
      other.elements_.begin(),
      other.elements_.end());
  elements_.erase(result.first, elements_.end());
}

Path Path::resolve(const std::vector<std::optional<std::string>>&
                       source_constant_arguments) const {
  Path path;
  path.elements_.reserve(elements_.size());

  for (auto element : elements_) {
    path.append(element.resolve_index_from_value_of(source_constant_arguments));
  }

  return path;
}

std::ostream& operator<<(std::ostream& out, const Path& path) {
  out << "Path[";
  for (auto iterator = path.elements_.begin(), end = path.elements_.end();
       iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "]";
}

std::string Root::to_string() const {
  switch (kind()) {
    case Root::Kind::Argument:
      return fmt::format("Argument({})", parameter_position());
    case Root::Kind::Return:
      return "Return";
    case Root::Kind::Leaf:
      return "Leaf";
    case Root::Kind::Anchor:
      return "Anchor";
    case Root::Kind::Producer:
      return "Producer";
    case Root::Kind::CanonicalThis:
      return "Argument(-1)";
    case Root::Kind::CallEffect:
      return "CallEffect";
    default:
      mt_unreachable();
  }
}

Json::Value Root::to_json() const {
  return to_string();
}

Root Root::from_json(const Json::Value& value) {
  auto root_string = JsonValidation::string(value);
  if (boost::starts_with(root_string, "Argument(") &&
      boost::ends_with(root_string, ")") && root_string.size() >= 11) {
    auto parameter_string = root_string.substr(9, root_string.size() - 10);
    auto parameter = parse_parameter_position(parameter_string);
    if (!parameter) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          /* expected */
          fmt::format(
              "`Argument(<number>)` for access path root, got `{}`",
              root_string));
    }
    // Note: `Root::Kind::CanonicalThis` (Argument(-1)) cannot be specified in
    // JSON.
    return Root(Root::Kind::Argument, *parameter);
  } else if (root_string == "Return") {
    return Root(Root::Kind::Return);
  } else if (root_string == "Leaf") {
    return Root(Root::Kind::Leaf);
  } else if (root_string == "Anchor") {
    return Root(Root::Kind::Anchor);
  } else if (root_string == "Producer") {
    return Root(Root::Kind::Producer);
  } else if (root_string == "CallEffect") {
    return Root(Root::Kind::CallEffect);
  } else {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        fmt::format(
            "valid access path root (`Return`, `Argument(...)`, `Leaf`, `Anchor`, `Producer` or `CallEffect`), got `{}`",
            root_string));
  }
}

std::ostream& operator<<(std::ostream& out, const Root& root) {
  return out << root.to_string();
}

bool AccessPath::operator==(const AccessPath& other) const {
  return root_ == other.root_ && path_ == other.path_;
}

bool AccessPath::operator!=(const AccessPath& other) const {
  return !(*this == other);
}

bool AccessPath::leq(const AccessPath& other) const {
  return root_ == other.root_ && other.path_.is_prefix_of(path_);
}

void AccessPath::join_with(const AccessPath& other) {
  mt_assert(root_ == other.root_);
  path_.reduce_to_common_prefix(other.path_);
}

AccessPath AccessPath::canonicalize_for_method(const Method* method) const {
  // The canonical port takes the form anchor:<root>. Path is ignored.
  // For arguments, first argument starts at index 0. Non-static methods in
  // Mariana Trench have their arguments off-by-one and are shifted down.
  if (!root_.is_argument() || method->is_static()) {
    return AccessPath(
        Root(Root::Kind::Anchor), Path{PathElement::field(root_.to_string())});
  }

  auto position = root_.parameter_position();
  if (position == 0) {
    position = static_cast<Root::IntegerEncoding>(Root::Kind::CanonicalThis);
  } else {
    position = position - 1;
  }

  return AccessPath(
      Root(Root::Kind::Anchor),
      Path{PathElement::field(
          Root(Root::Kind::Argument, position).to_string())});
}

std::vector<std::string> AccessPath::split_path(const Json::Value& value) {
  auto string = JsonValidation::string(value);

  // Split the string by ".".
  std::vector<std::string> elements;
  std::string_view current = string;

  while (!current.empty()) {
    auto position = current.find('.');
    if (position == std::string::npos) {
      elements.push_back(std::string(current));
      break;
    } else {
      elements.push_back(std::string(current.substr(0, position)));
      current = current.substr(position + 1);
    }
  }

  return elements;
}

AccessPath AccessPath::from_json(const Json::Value& value) {
  auto elements = split_path(value);

  if (elements.empty()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, "non-empty string for access path");
  }

  // Parse the root.
  const auto& root_string = elements.front();
  auto root = Root::from_json(root_string);

  Path path;
  for (auto iterator = std::next(elements.begin()), end = elements.end();
       iterator != end;
       ++iterator) {
    path.append(PathElement::field(*iterator));
  }

  return AccessPath(root, path);
}

Json::Value AccessPath::to_json() const {
  // We could return a json array containing path elements, but this would break
  // all our tests since we sort all json arrays before comparing them.

  std::string value = root_.to_string();

  for (const auto& field : path_) {
    value.append(show(field));
  }

  return value;
}

std::ostream& operator<<(std::ostream& out, const AccessPath& access_path) {
  out << "AccessPath(" << access_path.root();
  if (!access_path.path().empty()) {
    out << ", " << access_path.path();
  }
  return out << ")";
}

} // namespace marianatrench
