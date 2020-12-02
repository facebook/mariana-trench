// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <mariana-trench/Generator.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

const auto json_file_path = boost::filesystem::current_path() /
    "fbandroid/native/mariana-trench/shim/resources/model_generators/sinks/AndroidLogcatSinkGenerator.json";
class AndroidLogcatSinkGeneratorTest : public test::Test {};

} // namespace

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
      JsonModelGenerator("AndroidLogcatSinkGenerator", context, json_file_path)
          .run(context.stores),
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
      JsonModelGenerator("AndroidLogcatSinkGenerator", context, json_file_path)
          .run(context.stores),
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
      JsonModelGenerator("AndroidLogcatSinkGenerator", context, json_file_path)
          .run(context.stores),
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
      JsonModelGenerator("AndroidLogcatSinkGenerator", context, json_file_path)
          .run(context.stores),
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
      JsonModelGenerator("AndroidLogcatSinkGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}
