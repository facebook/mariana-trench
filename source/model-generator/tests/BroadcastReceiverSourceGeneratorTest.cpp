// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

const auto json_file_path = boost::filesystem::current_path() /
    "fbandroid/native/mariana-trench/shim/resources/model_generators/sources/BroadcastReceiverSourceGenerator.json";
class BroadcastReceiverSourceGeneratorTest : public test::Test {};

} // namespace

TEST_F(BroadcastReceiverSourceGeneratorTest, OverrideSourceMethod) {
  Scope scope;

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/BroadcastReceiver;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V");

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/xyz/SomeReceiver;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ dex_base_method->get_class());

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator(
          "BroadcastReceiverSourceGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(
          Model(
              /* method */ base_method,
              context,
              /* modes */ Model::Mode::NoJoinVirtualOverrides,
              /* generations */ {},
              /* parameter_sources */
              {{AccessPath(Root(Root::Kind::Argument, 2)),
                generator::source(
                    context,
                    /* method */ base_method,
                    /* kind */ "ReceiverUserInput")}}),
          Model(
              /* method */ method,
              context,
              /* modes */ Model::Mode::NoJoinVirtualOverrides,
              /* generations */ {},
              /* parameter_sources */
              {{AccessPath(Root(Root::Kind::Argument, 2)),
                generator::source(
                    context,
                    /* method */ method,
                    /* kind */ "ReceiverUserInput")}})));
}

TEST_F(BroadcastReceiverSourceGeneratorTest, NoOverrideMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/xyz/SomeReceiver;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "BroadcastReceiverSourceGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}
