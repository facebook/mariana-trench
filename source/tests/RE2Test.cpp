/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/RE2.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class RE2Test : public test::Test {};

TEST_F(RE2Test, AsLiteralString) {
  EXPECT_EQ(
      as_literal_string(re2::RE2("Foo")), std::optional<std::string>("Foo"));
  EXPECT_EQ(
      as_literal_string(re2::RE2("Landroid/util/Foo;\\.bar:\\(\\)V")),
      std::optional<std::string>("Landroid/util/Foo;.bar:()V"));
  EXPECT_EQ(
      as_literal_string(re2::RE2("\\.\\+\\?\\(\\)\\[\\]\\-")),
      std::optional<std::string>(".+?()[]-"));
  EXPECT_EQ(as_literal_string(re2::RE2("Foo.")), std::nullopt);
  EXPECT_EQ(as_literal_string(re2::RE2("Foo.*")), std::nullopt);
  EXPECT_EQ(as_literal_string(re2::RE2(".*Foo")), std::nullopt);
  EXPECT_EQ(as_literal_string(re2::RE2("\\d")), std::nullopt);
  EXPECT_EQ(as_literal_string(re2::RE2("(?i)Foo")), std::nullopt);
  EXPECT_EQ(
      as_literal_string(re2::RE2("[F]oo")), std::optional<std::string>("Foo"));
}

} // namespace marianatrench
