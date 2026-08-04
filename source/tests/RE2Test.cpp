/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>
#include <vector>

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

TEST_F(RE2Test, RequiredSubstring) {
  EXPECT_EQ(
      required_substring(re2::RE2("Foo")), std::optional<std::string>("Foo"));
  EXPECT_EQ(
      required_substring(re2::RE2(".*okhttp3/Request;\\.post:.*")),
      std::optional<std::string>("okhttp3/Request;.post:"));
  EXPECT_EQ(
      required_substring(re2::RE2("^Landroid/util/Foo;$")),
      std::optional<std::string>("Landroid/util/Foo;"));

  // The longest literal run wins.
  EXPECT_EQ(
      required_substring(re2::RE2("Foobar.Baz")),
      std::optional<std::string>("Foobar"));

  // A quantified byte ends the run: what follows need not be adjacent to what
  // came before, and an optional byte is not required at all.
  EXPECT_EQ(
      required_substring(re2::RE2("abc+de")),
      std::optional<std::string>("abc"));
  EXPECT_EQ(
      required_substring(re2::RE2("a?bcde")),
      std::optional<std::string>("bcde"));

  // Anything that can make a literal run optional.
  EXPECT_EQ(required_substring(re2::RE2("get(Eager|Multibind)")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("get[EM]ager")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("getEager{2}")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("(?i)getEager")), std::nullopt);

  // Escapes we do not model: we cannot tell how many of the bytes that follow
  // belong to the escape, so no substring may be derived from the pattern.
  EXPECT_EQ(required_substring(re2::RE2("Foobar\\d+")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("\\dFoobar")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("\\x41Foobar")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2("Foobar\\")), std::nullopt);

  // Too short to pay for itself.
  EXPECT_EQ(required_substring(re2::RE2("Fo")), std::nullopt);
  EXPECT_EQ(required_substring(re2::RE2(".*")), std::nullopt);

  // Invalid regular expression.
  EXPECT_EQ(required_substring(re2::RE2("Foo(")), std::nullopt);
}

TEST_F(RE2Test, MatchPattern) {
  // The prefilter must never change the answer, whether a required substring
  // was derived from the pattern or not.
  const std::vector<std::string> patterns = {
      "Foo",
      "Fo",
      ".*",
      ".*okhttp3/Request;\\.post:.*",
      "Landroid/util/Foo;\\.bar:\\(\\)V",
      "^Landroid/util/Foo;$",
      "Foobar.Baz",
      "abc+de",
      "a?bcde",
      "get(Eager|Multibind)",
      "get[EM]ager",
      "getEager{2}",
      "(?i)getEager",
      "Foobar\\d+",
      "\\dFoobar",
      "Foo(",
  };
  const std::vector<std::string> texts = {
      "",
      "Fo",
      "Foo",
      "FooBar",
      "okhttp3/Request;.post:",
      "Lokhttp3/Request;.post:(Ljava/lang/String;)V",
      "Lokhttp3/Request;Xpost:(Ljava/lang/String;)V",
      "Landroid/util/Foo;.bar:()V",
      "Foobar/Baz",
      "FoobarBaz",
      "abcccdefg",
      "abdefg",
      "bcde",
      "abcde",
      "getEager",
      "getMultibind",
      "getMager",
      "getEagerEager",
      "GETEAGER",
      "Foobar123",
      "Foobar",
      "1Foobar",
      "Afoobar",
  };

  for (const auto& pattern : patterns) {
    MatchPattern match_pattern(pattern);
    re2::RE2 regular_expression(pattern);
    for (const auto& text : texts) {
      EXPECT_EQ(
          match_pattern.full_match(text),
          re2::RE2::FullMatch(text, regular_expression))
          << "full_match(\"" << text << "\") on pattern \"" << pattern << "\"";
      EXPECT_EQ(
          match_pattern.partial_match(text),
          re2::RE2::PartialMatch(text, regular_expression))
          << "partial_match(\"" << text << "\") on pattern \"" << pattern
          << "\"";
    }
  }
}

} // namespace marianatrench
