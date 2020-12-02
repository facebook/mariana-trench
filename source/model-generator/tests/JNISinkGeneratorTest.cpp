// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <IRAssembler.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

const auto json_file_path = boost::filesystem::current_path() /
    "fbandroid/native/mariana-trench/shim/resources/model_generators/sinks/JNISinkGenerator.json";
class JNISinkGeneratorTest : public test::Test {};

} // namespace

TEST_F(JNISinkGeneratorTest, SinkForNative) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "LFoo;",
      /* method */ "bar",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ false,
      /* is_native */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator("JNISinkGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "JNI")},
           {AccessPath(Root(Root::Kind::Argument, 2)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "JNI")}})));
}

TEST_F(JNISinkGeneratorTest, SinkForStaticNative) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "LFoo;",
      /* method */ "bar",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true,
      /* is_private */ false,
      /* is_native */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator("JNISinkGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 0)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "JNI")},
           {AccessPath(Root(Root::Kind::Argument, 1)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "JNI")}})));
}

TEST_F(JNISinkGeneratorTest, NoSinkForNonNative) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "LFoo;",
      /* method */ "bar",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator("JNISinkGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}
