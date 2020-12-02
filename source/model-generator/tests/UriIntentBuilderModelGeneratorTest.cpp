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
    "fbandroid/native/mariana-trench/shim/resources/model_generators/propagations/UriIntentBuilderModelGenerator.json";

class UriIntentBuilderModelGeneratorTest : public test::Test {};

} // namespace

TEST_F(UriIntentBuilderModelGeneratorTest, UriIntentBuilderModel) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/common/uri/UriIntentBuilder;",
      /* method */ "getIntentForUri",
      /* parameter_type */ "Landroid/content/Context;Ljava/lang/String;",
      /* return_type */ "Landroid/content/Intent;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::NoJoinVirtualOverrides,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {{Propagation(
                /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
                /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                /* user_features */
                FeatureSet{context.features->get("via-uri-intent-builder")}),
            /* output */ AccessPath(Root(Root::Kind::Return))}})));
}

TEST_F(UriIntentBuilderModelGeneratorTest, UriIntentMapperModel) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/common/uri/UriIntentMapper;",
      /* method */ "getIntentForUri",
      /* parameter_type */ "Landroid/content/Context;Ljava/lang/String;",
      /* return_type */ "Landroid/content/Intent;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::NoJoinVirtualOverrides,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {{Propagation(
                /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
                /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                /* user_features */
                FeatureSet{context.features->get("via-uri-intent-builder")}),
            /* output */ AccessPath(Root(Root::Kind::Return))}})));
}

TEST_F(UriIntentBuilderModelGeneratorTest, NativeThirdPartyUriHelperModel) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/intent/thirdparty/NativeThirdPartyUriHelper;",
      /* method */ "getIntent",
      /* parameter_type */ "Landroid/content/Context;Landroid/net/Uri;",
      /* return_type */ "Landroid/content/Intent;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::NoJoinVirtualOverrides,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {{Propagation(
                /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
                /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                /* user_features */
                FeatureSet{context.features->get("via-uri-intent-builder")}),
            /* output */ AccessPath(Root(Root::Kind::Return))}})));
}

TEST_F(
    UriIntentBuilderModelGeneratorTest,
    NativeThirdPartyUriHelperStaticModel) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/intent/thirdparty/NativeThirdPartyUriHelper;",
      /* method */ "getFallbackIntentFromFbrpcUri",
      /* parameter_type */ "Landroid/content/Context;Landroid/net/Uri;",
      /* return_type */ "Landroid/content/Intent;",
      /* super */ nullptr,
      /* is_static */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::NoJoinVirtualOverrides,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {{Propagation(
                /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
                /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                /* user_features */
                FeatureSet{context.features->get("via-uri-intent-builder")}),
            /* output */ AccessPath(Root(Root::Kind::Return))}})));
}

TEST_F(UriIntentBuilderModelGeneratorTest, NoModelForOtherClass) {
  Scope scope;

  redex::create_void_method(
      scope, /* class_name */ "Lcom/Example;", /* method */ "foo");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}

TEST_F(UriIntentBuilderModelGeneratorTest, NoModelForNonIntentMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/intent/thirdparty/NativeThirdPartyUriHelper;",
      /* method */ "logIfAppIntent",
      /* parameter_type */ "Landroid/content/Intent;",
      /* return_type */ "V");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}

TEST_F(UriIntentBuilderModelGeneratorTest, NoModelForConstructor) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/intent/thirdparty/NativeThirdPartyUriHelper;",
      /* method */ "<init>",
      /* parameter_type */ "V",
      /* return_type */ "V",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}

TEST_F(UriIntentBuilderModelGeneratorTest, NoModelForPrivateMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/facebook/intent/thirdparty/NativeThirdPartyUriHelper;",
      /* method */ "parseScheme",
      /* parameter_type */ "Ljava/lang/String;",
      /* return_type */ "Ljava/lang/String;",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ true);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "UriIntentBuilderModelGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}
