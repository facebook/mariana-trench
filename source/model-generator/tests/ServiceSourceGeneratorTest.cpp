/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ServiceSourceGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class ServiceSourceGeneratorTest : public test::Test {};

} // namespace

TEST_F(ServiceSourceGeneratorTest, ServiceMessengerHandler) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/oculus/vrshell/ShellEnvOverlayService$ShellEnvIncomingHandler;",
      /* method */ "handleMessage",
      /* parameter_type */ "Landroid/os/Message;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      ServiceSourceGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::NoJoinVirtualOverrides,
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            generator::source(
                context,
                /* method */ method,
                /* kind */ "ServiceUserInput")}})));
}

TEST_F(ServiceSourceGeneratorTest, GenericMessengerHandler) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/oculus/vrshell/RandomClass;",
      /* method */ "handleMessage",
      /* parameter_type */ "Landroid/os/Message;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      ServiceSourceGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(ServiceSourceGeneratorTest, AndroidXMessengerHandler) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Landroidx/mediarouter/media/MediaRouteProviderService$ReceiveHandler;",
      /* method */ "handleMessage",
      /* parameter_type */ "Landroid/os/Message;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      ServiceSourceGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre());
}
