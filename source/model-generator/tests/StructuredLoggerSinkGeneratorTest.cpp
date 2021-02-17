/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/StructuredLoggerSinkGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class StructuredLoggerSinkGeneratorTest : public test::Test {};

} // namespace

TEST_F(StructuredLoggerSinkGeneratorTest, StructuredLoggerModel) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/analytics/structuredlogger/events/SemDeepLink;",
      /* method */ "setUrl",
      /* parameter_types */ "Ljava/lang/String;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      StructuredLoggerSinkGenerator(context).run(*context.methods),
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
                /* kind */ "Logging",
                /* features */ {},
                /* callee_port */ Root::Kind::Anchor)}})));
}

TEST_F(StructuredLoggerSinkGeneratorTest, InstagramLoggerModel) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/instagram/common/analytics/intf/AnalyticsEvent;",
      /* method */ "addExtra",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      StructuredLoggerSinkGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::SkipAnalysis,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "Logging",
                /* features */ {},
                /* callee_port */ Root::Kind::Anchor)}})));
}

TEST_F(StructuredLoggerSinkGeneratorTest, HoneyClientLoggerModel) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/analytics/logger/HoneyClientEvent;",
      /* method */ "addParameter",
      /* parameter_types */ "Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      StructuredLoggerSinkGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre(Model(
          /* method */ method,
          context,
          /* modes */ Model::Mode::SkipAnalysis,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            generator::sink(
                context,
                /* method */ method,
                /* kind */ "Logging",
                /* features */ {},
                /* callee_port */ Root::Kind::Anchor)}})));
}

TEST_F(StructuredLoggerSinkGeneratorTest, NoModelForImplClass) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/analytics/structuredlogger/events/SemDeepLinkImpl;",
      /* method */ "setUrl");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      StructuredLoggerSinkGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(StructuredLoggerSinkGeneratorTest, NoModelForNonLoggingMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/analytics/structuredlogger/events/SemDeepLink;",
      /* method */ "start");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      StructuredLoggerSinkGenerator(context).run(*context.methods),
      testing::UnorderedElementsAre());
}
