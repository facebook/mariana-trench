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
#include <mariana-trench/TaggedRootSet.h>

namespace marianatrench {

TaggedRoot TaggedRoot::from_json(const Json::Value& value) {
  if (value.isObject()) {
    auto root = Root::from_json(value["port"]);
    auto tag = JsonValidation::string(value, "tag");
    return TaggedRoot(root, DexString::make_string(tag));
  }

  // Otherwise, the entire value represents the root object.
  return TaggedRoot(Root::from_json(value), /* tag */ nullptr);
}

Json::Value TaggedRoot::to_json() const {
  if (tag_ != nullptr) {
    auto result = Json::Value(Json::objectValue);
    result["port"] = root_.to_json();
    result["tag"] = tag_->str_copy();
    return result;
  }

  // For backward compatibility and verbosity reasons, if the tag is not
  // specified, simply return the root's JSON (as a string).
  return root_.to_json();
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
