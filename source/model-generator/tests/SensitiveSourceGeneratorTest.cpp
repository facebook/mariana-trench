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
      "facebook/internal-configuration/model-generators/sources/SensitiveSourceGenerator.json";
}

} // namespace

class SensitiveSourceGeneratorTest : public test::Test {};

TEST_F(SensitiveSourceGeneratorTest, SensitiveSourceMethod1) {
  Scope scope;

  std::string sensitiveAnnotation =
      "Lcom/facebook/privacy/datacollection/Sensitive;";
  std::vector<std::string> annotations;
  annotations.push_back(sensitiveAnnotation);

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/Sensitive;",
      /* method */ "onSensitive",
      /* parameter_type */ "",
      /* return_type */ "Ljava/lang/String;",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ false,
      /* is_native */ false,
      /* annotations */ annotations);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);

  auto model = Model(base_method, context);
  model.add_generation(
      AccessPath(Root(Root::Kind::Return)),
      generator::source(
          context,
          /* method */ base_method,
          /* kind */ "SensitiveData"));
  EXPECT_THAT(
      JsonModelGenerator("SensitiveSourceGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(SensitiveSourceGeneratorTest, SensitiveSourceMethod2) {
  Scope scope;

  std::string sensitiveAnnotation =
      "Lcom/facebook/thrift/annotations/Sensitive;";
  std::vector<std::string> annotations;
  annotations.push_back(sensitiveAnnotation);

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/Sensitive;",
      /* method */ "onSensitive",
      /* parameter_type */ "",
      /* return_type */ "Ljava/lang/String;",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ false,
      /* is_native */ false,
      /* annotations */ annotations);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);

  auto model = Model(base_method, context);
  model.add_generation(
      AccessPath(Root(Root::Kind::Return)),
      generator::source(
          context,
          /* method */ base_method,
          /* kind */ "SensitiveData"));
  EXPECT_THAT(
      JsonModelGenerator("SensitiveSourceGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(SensitiveSourceGeneratorTest, NonSensitiveSourceMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/xyz/NonSensitive;",
      /* method */ "onNonSensitive",
      /* parameter_type */ "",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator("SensitiveSourceGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}
