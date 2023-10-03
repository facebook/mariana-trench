/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LabelledRoot.h>

namespace marianatrench {

LabelledRoot::LabelledRoot(Root root) : root_(std::move(root)), label_(nullptr) { }

LabelledRoot::LabelledRoot(Root root, const std::string* label) : root_(std::move(root)), label_(label) {}

std::size_t LabelledRoot::hash() const {
  std::size_t seed = 0;
  boost::hash_combine(seed, std::hash<Root>{}(root_));
  boost::hash_combine(seed, label_);
  return seed;
}

bool LabelledRoot::operator==(const LabelledRoot& other) const {
  return root_ == other.root_ && label_ == other.label_;
}

const Root& LabelledRoot::root() const {
  return root_;
}

std::optional<std::string_view> LabelledRoot::label() const {
  return label_ ? *label_ : std::optional<std::string_view>();
}

std::string LabelledRoot::to_string() const {
  std::string value{root_.to_string()};
  if (label_) {
    value += '@';
    value += *label_;
  }
  return value;
}

LabelledRoot LabelledRoot::from_json(const Json::Value& value) {
  if (!value.isObject()) {
    return LabelledRoot(Root::from_json(value));
  }

  Root root = Root::from_json(value["port"]);
  const auto& label = value["label"];
  if (label.isNull()) {
    return LabelledRoot(std::move(root));
  }

  return LabelledRoot(std::move(root), label_factory().create(JsonValidation::string(label)));
}

Json::Value LabelledRoot::to_json() const {
  auto root = root_.to_json();
  if (!label_) {
    return root;
  }

  auto result = Json::Value(Json::objectValue);
  result["port"] = std::move(root);
  result["label"] = *label_;
  return result;
}

const UniquePointerFactory<std::string, std::string>& LabelledRoot::label_factory() {
  static UniquePointerFactory<std::string, std::string> instance;
  return instance;
}

std::ostream& operator<<(std::ostream& out, const LabelledRoot& root) {
  return out << root.to_string();
}

} // namespace marianatrench
