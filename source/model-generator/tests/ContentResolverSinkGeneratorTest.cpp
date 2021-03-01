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
      "facebook/internal-configuration/model_generators/sinks/ContentResolverSinkGenerator.json";
}

} // namespace

class ContentResolverSinkGeneratorTest : public test::Test {};

TEST_F(ContentResolverSinkGeneratorTest, SinkForQuery) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/ContentResolver;",
      /* method */ "query",
      /* parameter_type */
      "Landroid/net/Uri;Ljava/lang/String;Landroid/os/Bundle;Landroid/os/CancellationSignal;",
      /* return_type */ "Landroid/database/Cursor;",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  for (int i = 1; i <= 4; i++) {
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, i)),
        generator::sink(
            context,
            method,
            /* kind */ "ContentResolver"));
  }

  EXPECT_THAT(
      JsonModelGenerator(
          "ContentResolverSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(ContentResolverSinkGeneratorTest, SinkForDelete) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/ContentResolver;",
      /* method */ "delete",
      /* parameter_type */
      "Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;",
      /* return_type */ "I",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  for (int i = 1; i <= 3; i++) {
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, i)),
        generator::sink(
            context,
            method,
            /* kind */ "ContentResolver"));
  }

  EXPECT_THAT(
      JsonModelGenerator(
          "ContentResolverSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(ContentResolverSinkGeneratorTest, SinkForOpenAssetFile) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/ContentResolver;",
      /* method */ "openAssetFile",
      /* parameter_type */
      "Landroid/net/Uri;Ljava/lang/String;Landroid/os/CancellationSignal;",
      /* return_type */ "Lcontent/res/AssetFileDescriptor;",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  for (int i = 1; i <= 3; i++) {
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, i)),
        generator::sink(
            context,
            method,
            /* kind */ "ContentResolver"));
  }

  EXPECT_THAT(
      JsonModelGenerator(
          "ContentResolverSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}

TEST_F(ContentResolverSinkGeneratorTest, SinkForOpenInputStream) {
  Scope scope;

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/content/ContentResolver;",
      /* method */ "openInputStream",
      /* parameter_type */ "Landroid/net/Uri;",
      /* return_type */ "Ljava/io/InputStream;",
      /* super */ nullptr);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  model.add_mode(Model::Mode::SkipAnalysis, context);
  for (int i = 1; i <= 1; i++) {
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, i)),
        generator::sink(
            context,
            method,
            /* kind */ "ContentResolver"));
  }

  EXPECT_THAT(
      JsonModelGenerator(
          "ContentResolverSinkGenerator", context, json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(model));
}
