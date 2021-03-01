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
      "facebook/internal-configuration/model-generators/sinks/IntentLaunchingSinkGenerator.json";
}

} // namespace

class IntentLaunchingSinkGeneratorTest : public test::Test {};

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForContext) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/Context;",
      /* method */ "startActivity",
      /* parameter_type */ "Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForStaticMethod) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/secure/context/SecureContext;",
      /* method */ "launchInternalActivity",
      /* parameter_type */ "Landroid/content/Intent;Landroid/content/Context;",
      /* return_type */ "Z",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 0)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingInternalComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForActivity) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/app/Activity;",
      /* method */ "startIntentSenderFromChild",
      /* parameter_type */
      "Landroid/app/Activity;Landroid/content/IntentSender;ILandroid/content/Intent;IIILandroid/os/Bundle;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 4)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForFragment) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/app/Fragment;",
      /* method */ "startIntentSenderForResult",
      /* parameter_type */
      "Landroid/content/IntentSender;ILandroid/content/Intent;IIILandroid/os/Bundle;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 3)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForIntentLauncher) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/secure/context/IntentLauncher;",
      /* method */ "launchActivity",
      /* parameter_type */
      "Landroid/content/Intent;Landroid/content/Context;",
      /* return_type */ "Z",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForSecureContextHelper) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/content/SecureContextHelper;",
      /* method */ "startFacebookActivity",
      /* parameter_type */
      "Landroid/content/Intent;Landroid/content/Context;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingInternalComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForAndroidxFragment) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroidx/fragment/app/Fragment;",
      /* method */ "startActivity",
      /* parameter_type */
      "Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForNativeBroadcast) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/oculus/vrshell/ShellApplication;",
      /* method */ "nativeBroadcastIntent",
      /* parameter_type */ "J[Ljava/lang/String;",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForContextWrapper) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/ContextWrapper;",
      /* method */ "startActivity",
      /* parameter_type */ "Landroid/content/Intent;Landroid/os/Bundle;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, SinkForTaskStackBuilder) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroidx/core/app/TaskStackBuilder;",
      /* method */ "startActivities",
      /* parameter_type */ "Landroid/os/Bundle;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_sink(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::sink(
          context,
          method,
          /* kind */ "LaunchingComponent"));

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(IntentLaunchingSinkGeneratorTest, NoSinkForNonStartMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/Context;",
      /* method */ "<init>",
      /* parameter_type */ "Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(IntentLaunchingSinkGeneratorTest, NoSinkForNonIntentParamMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/Context;",
      /* method */ "<init>",
      /* parameter_type */
      "Landroid/content/ComponentName;Ljava/lang/String;Landroid/os/Bundle;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}

TEST_F(IntentLaunchingSinkGeneratorTest, NoSinkForArrayOfIntentAsParam) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/Context;",
      /* method */ "<init>",
      /* parameter_type */ "Landroid/content/Intent[];Landroid/os/Bundle;",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "IntentLaunchingSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}
