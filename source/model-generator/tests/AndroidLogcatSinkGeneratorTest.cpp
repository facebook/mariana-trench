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
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

boost::filesystem::path json_file_path() {
  return test::find_repository_root() /
      "shim/resources/model_generators/sinks/AndroidLogcatSinkGenerator.json";
}

} // namespace

class AndroidLogcatSinkGeneratorTest : public test::Test {};

TEST_F(AndroidLogcatSinkGeneratorTest, SinkForLogcatV) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Landroid/util/Log;",
      /* method */ "v",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "AndroidLogcatSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(AndroidLogcatSinkGeneratorTest, SinkForLogcatD) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Landroid/util/Log;",
      /* method */ "d",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "AndroidLogcatSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(AndroidLogcatSinkGeneratorTest, SinkForLogcatI) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Landroid/util/Log;",
      /* method */ "i",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "AndroidLogcatSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(AndroidLogcatSinkGeneratorTest, SinkForLogcatW) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Landroid/util/Log;",
      /* method */ "w",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator(
          "AndroidLogcatSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::SkipAnalysis,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "Logcat")}})));
}

TEST_F(AndroidLogcatSinkGeneratorTest, NoSinkForNonLog) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Landroid/util/Log;",
      /* method */ "isLoggable");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "AndroidLogcatSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}
