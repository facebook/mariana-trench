/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

/**
 * Tuple of Root and a string label. This is only used to assign labels to
 * via-value features, as documented in section "Via-value Features" in
 * documentation/website/documentation/models.md.
 */
class LabelledRoot final {
 public:
  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(LabelledRoot)

  /**
   * Used to store in a sparta::HashedSetAbstractDomain in
   * Frame::via_value_of_ports.
   */
  std::size_t hash() const;
  bool operator==(const LabelledRoot& other) const;

  const Root& root() const;

  std::optional<std::string_view> label() const;

  static LabelledRoot from_json(const Json::Value& value);
  Json::Value to_json() const;

 private:
  /** Creates an unlabelled root. */
  explicit LabelledRoot(Root root);

  /** Creates a root with the given label. */
  explicit LabelledRoot(Root root, const std::string* label);

  friend std::ostream& operator<<(std::ostream& out, const LabelledRoot& root);
  std::string to_string() const;

  /**
   * Used for all label strings. This allows thus to perform pointer rather
   * than string comparisons in hash and operator==.
   */
  static const UniquePointerFactory<std::string, std::string>& label_factory();

  /** Configured root access being labelled. */
  Root root_;

  /** Text label for root(). */
  const std::string* label_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::LabelledRoot> {
  std::size_t operator()(const marianatrench::LabelledRoot& labelled_root) const {
    return labelled_root.hash();
  }
};
