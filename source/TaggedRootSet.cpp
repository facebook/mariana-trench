/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <json/value.h>

#include <DexClass.h>
#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/TaggedRootSet.h>

namespace marianatrench {

TaggedRoot TaggedRoot::from_json(const Json::Value& value) {
  if (value.isObject()) {
    auto root = Root::from_json(value["port"]);
    const DexString* tag = nullptr;
    if (value.isMember("tag")) {
      tag = DexString::make_string(JsonValidation::string(value, "tag"));
    }
    return TaggedRoot(root, tag);
  }

  // TODO(T183199267): Otherwise, the entire value represents the root object.
  // This format is deprecated. Remove once configs have been migrated.
  WARNING(
      1,
      "Using deprecated TaggedRoot string: `{}`. Please update to use {{ \"port\": \"{}\" }} instead",
      value.asString(),
      value.asString());
  return TaggedRoot(Root::from_json(value), /* tag */ nullptr);
}

Json::Value TaggedRoot::to_json() const {
  auto result = Json::Value(Json::objectValue);
  result["port"] = root_.to_json();

  if (tag_ != nullptr) {
    result["tag"] = tag_->str_copy();
  }

  return result;
}

std::ostream& operator<<(std::ostream& out, const TaggedRoot& tagged_root) {
  out << "TaggedRoot(port=" << show(tagged_root.root());
  auto tag = tagged_root.tag();
  if (tag != nullptr) {
    out << ", tag=" << tag->c_str();
  }
  return out << ")";
}

} // namespace marianatrench
