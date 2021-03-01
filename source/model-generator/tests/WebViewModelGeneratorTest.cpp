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

boost::filesystem::path client_override_source_json_file_path() {
  return test::find_repository_root() /
      "facebook/internal-configuration/model_generators/sources/WebViewSourceGenerator.json";
}

boost::filesystem::path client_webview_source_json_file_path() {
  return test::find_repository_root() /
      "facebook/internal-configuration/model_generators/sinks/WebViewModelGenerator.json";
}

} // namespace

class WebViewModelGeneratorTest : public test::Test {};

TEST_F(WebViewModelGeneratorTest, OverrideSourceMethod) {
  Scope scope;

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/webkit/WebViewClient;",
      /* method */ "onPageFinished",
      /* parameter_type */ "Landroid/webkit/WebView;Ljava/lang/String;",
      /* return_type */ "V");

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/instagram/simplewebview/SimpleWebViewFragment$2;",
      /* method */ "onPageFinished",
      /* parameter_type */ "Landroid/webkit/WebView;Ljava/lang/String;",
      /* return_type */ "V",
      /* super */ dex_base_method->get_class());

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator(
          "WebViewSourceGenerator",
          context,
          client_override_source_json_file_path())
          .run(*context.methods),
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
                    /* kind */ "WebViewUserInput")}}),
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
                    /* kind */ "WebViewUserInput")}})));
}

TEST_F(WebViewModelGeneratorTest, OverrideSinkMethod) {
  Scope scope;

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/webkit/WebView;",
      /* method */ "loadUrl",
      /* parameter_type */ "Ljava/lang/String;",
      /* return_type */ "V");

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/instagram/simplewebview/SimpleWebViewFragment$2;",
      /* method */ "loadUrl",
      /* parameter_type */ "Ljava/lang/String;",
      /* return_type */ "V",
      /* super */ dex_base_method->get_class());

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator(
          "WebViewModelGenerator",
          context,
          client_webview_source_json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre(
          Model(
              /* method */ base_method,
              context,
              /* modes */ Model::Mode::SkipAnalysis,
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */
              {{AccessPath(Root(Root::Kind::Argument, 1)),
                generator::sink(
                    context,
                    /* method */ base_method,
                    /* kind */ "WebViewLoadContent")}}),
          Model(
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
                    /* kind */ "WebViewLoadContent")}})));
}

TEST_F(WebViewModelGeneratorTest, NoOverrideMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Landroid/webkit/ServiceWorkerClient;",
      /* method */ "shouldInterceptRequest",
      /* parameter_type */ "Landroid/webkit/WebResourceRequest;",
      /* return_type */ "Landroid/webkit/WebResourceResponse;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator(
          "WebViewSourceGenerator",
          context,
          client_override_source_json_file_path())
          .run(*context.methods),
      testing::UnorderedElementsAre());
}
