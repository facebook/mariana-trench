/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/RE2.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class RE2Test : public test::Test {};

TEST_F(RE2Test, AsStringLiteral) {
  EXPECT_EQ(
      as_string_literal(re2::RE2("Foo")), std::optional<std::string>("Foo"));
  EXPECT_EQ(
      as_string_literal(re2::RE2("Landroid/util/Foo;\\.bar:\\(\\)V")),
      std::optional<std::string>("Landroid/util/Foo;.bar:()V"));
  EXPECT_EQ(
      as_string_literal(re2::RE2("\\.\\+\\?\\(\\)\\[\\]\\-")),
      std::optional<std::string>(".+?()[]-"));
  EXPECT_EQ(as_string_literal(re2::RE2("Foo.")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2("Foo.*")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2(".*Foo")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2("\\d")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2("Foo\\")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2("(?i)Foo")), std::nullopt);

  // These are actually string literals, but not currently supported.
  EXPECT_EQ(as_string_literal(re2::RE2("\\x01")), std::nullopt);
  EXPECT_EQ(as_string_literal(re2::RE2("[F]oo")), std::nullopt);

  // All these characters are safe.
  EXPECT_EQ(
      as_string_literal(re2::RE2("!\"#%&',-/:;<=>@_`~")),
      std::optional<std::string>("!\"#%&',-/:;<=>@_`~"));

  // All these characters must be escaped.
  for (char c : std::string("$()*+.?[]^{|}")) {
    EXPECT_EQ(
        as_string_literal(re2::RE2(std::string("Foo") + c)), std::nullopt);
    EXPECT_EQ(
        as_string_literal(re2::RE2(std::string("Foo\\") + c)),
        std::optional<std::string>(std::string("Foo") + c));
  }
}

} // namespace marianatrench
