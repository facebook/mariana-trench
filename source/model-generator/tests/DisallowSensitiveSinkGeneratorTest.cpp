/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

boost::filesystem::path json_file_path() {
  return test::find_repository_root() /
      "facebook/internal-configuration/model-generators/sinks/DisallowSensitiveSinkGenerator.json";
}

} // namespace

class DisallowSensitiveSinkGeneratorTest : public test::Test {};

TEST_F(DisallowSensitiveSinkGeneratorTest, SinkForDisallowSensitive) {
  Scope scope;

  std::string disallowSensitiveAnnotation =
      "Lcom/facebook/privacy/datacollection/DisallowSensitive;";
  std::vector<std::string> annotations;
  annotations.push_back(disallowSensitiveAnnotation);

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/DisallowSensitive;",
      /* method */ "onDisallowSensitive",
      /* parameter_type */ "Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ false,
      /* is_native */ false,
      /* annotations */ annotations);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context, /* method */ method, /* kind */ "DisallowSensitive"));
  EXPECT_THAT(
      JsonModelGenerator(
          "DisallowSensitiveSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(DisallowSensitiveSinkGeneratorTest, NoSinkForDisallowSensitive) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/DisallowSensitive;",
      /* method */ "onNonDisallowSensitive");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "DisallowSensitiveSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}
