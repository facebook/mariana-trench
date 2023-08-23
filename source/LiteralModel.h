/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>

#include <re2/re2.h>

#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Model for string literals as sources. Literals are tainted if they match a
 * configured regular expression.
 */
class LiteralModel final {
 public:
  /**
   * Used to create a joined model of multiple literal models.
   * \a matches will not match anything for joined models. */
  explicit LiteralModel();

  /**
   * Creates a model applying \p sources to all string literals matching \p
   * pattern.
   *
   * @param pattern Regular expression defining literals to taint.
   * @param sources Sources to apply to matched literals.
   */
  explicit LiteralModel(
      std::string pattern,
      const std::vector<TaintConfig>& sources);

  /**
   * Explicit copy constructor. Necessary since \p re2::RE2 deletes its copy
   * constructor, so we create a new object using \p pattern_.
   *
   * @param other Model to copy.
   */
  LiteralModel(const LiteralModel& other);

  /** Original configured pattern. */
  std::optional<std::string> pattern() const;

  /** Configured source taints to apply. */
  const Taint& sources() const;

  /**
   * Indicates whether the given literal matches this model.
   *
   * @param literal String literal from const_string instruction.
   */
  bool matches(std::string_view literal) const;

  /**
   * Used to construct joined models if multiple patterns match a literal. Does
   * not modify \a pattern.
   *
   * @param other New taints to add.
   */
  void join_with(const LiteralModel& other);

  /**
   * Indicates whether model contains any taints.
   *
   * @return \a true iff \a sources_ is bottom.
   */
  bool empty() const;

  static LiteralModel from_json(const Json::Value& value, Context& context);
  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  /* Export the model to JSON with undefined literal position. */
  Json::Value to_json(Context& context) const;

 private:
  std::optional<re2::RE2> regex_;
  Taint sources_;
};

} // namespace marianatrench
