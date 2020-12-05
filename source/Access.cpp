/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/JsonValidation.h>
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

bool Path::operator==(const Path& other) const {
  return elements_ == other.elements_;
}

void Path::append(Element element) {
  mt_assert(element != nullptr);
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
    elements_.resize(max_size);
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

std::ostream& operator<<(std::ostream& out, const Root& root) {
  switch (root.kind()) {
    case Root::Kind::Argument:
      return out << "Argument(" << root.parameter_position() << ")";
    case Root::Kind::Return:
      return out << "Return";
    case Root::Kind::Leaf:
      return out << "Leaf";
    case Root::Kind::Anchor:
      return out << "Anchor";
    case Root::Kind::Producer:
      return out << "Producer";
    default:
      mt_unreachable();
  }
}

bool AccessPath::operator==(const AccessPath& other) const {
  return root_ == other.root_ && path_ == other.path_;
}

bool AccessPath::leq(const AccessPath& other) const {
  return root_ == other.root_ && other.path_.is_prefix_of(path_);
}

void AccessPath::join_with(const AccessPath& other) {
  mt_assert(root_ == other.root_);
  path_.reduce_to_common_prefix(other.path_);
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
  auto root = Root(Root::Kind::Return);
  const auto& root_string = elements.front();

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
    root = Root(Root::Kind::Argument, *parameter);
  } else if (root_string == "Return") {
    root = Root(Root::Kind::Return);
  } else if (root_string == "Leaf") {
    root = Root(Root::Kind::Leaf);
  } else if (root_string == "Anchor") {
    root = Root(Root::Kind::Anchor);
  } else if (root_string == "Producer") {
    root = Root(Root::Kind::Producer);
  } else {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        fmt::format(
            "valid access path root (`Return`, `Argument(...)`, `Leaf`, `Anchor` or `Producer`), got `{}`",
            root_string));
  }

  Path path;
  for (auto iterator = std::next(elements.begin()), end = elements.end();
       iterator != end;
       ++iterator) {
    path.append(DexString::make_string(*iterator));
  }

  return AccessPath(root, path);
}

Json::Value AccessPath::to_json() const {
  // We could return a json array containing path elements, but this would break
  // all our tests since we sort all json arrays before comparing them.

  std::string value;

  switch (root_.kind()) {
    case Root::Kind::Argument: {
      value.append(fmt::format("Argument({})", root_.parameter_position()));
      break;
    }
    case Root::Kind::Return: {
      value.append("Return");
      break;
    }
    case Root::Kind::Leaf: {
      value.append("Leaf");
      break;
    }
    case Root::Kind::Anchor: {
      value.append("Anchor");
      break;
    }
    case Root::Kind::Producer: {
      value.append("Producer");
      break;
    }
    default: {
      mt_unreachable();
    }
  }

  for (auto* field : path_) {
    value.append(".");
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
