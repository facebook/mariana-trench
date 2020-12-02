// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

const auto json_file_path = boost::filesystem::current_path() /
    "fbandroid/native/mariana-trench/shim/resources/model_generators/sources/JavaScriptInterfaceSourceGenerator.json";
class JavaScriptInterfaceSourceGeneratorTest : public test::Test {};

} // namespace

TEST_F(
    JavaScriptInterfaceSourceGeneratorTest,
    JavascriptInterfaceSourceMethod) {
  Scope scope;

  std::string javascriptInterfaceAnnotation =
      "Landroid/webkit/JavascriptInterface;";
  std::vector<std::string> annotations;
  annotations.push_back(javascriptInterfaceAnnotation);

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */
      "Lcom/instagram/business/instantexperiences/jsbridge/InstantExperiencesJSBridge;",
      /* method */ "paymentsCheckoutChargeRequestSuccessReturn",
      /* parameter_type */ "Ljava/lang/String;ILjava/lang/String;",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ false,
      /* is_private */ false,
      /* is_native */ false,
      /* annotations */ annotations);

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto model = Model(method, context);
  for (int i = 1; i <= 3; i++) {
    model.add_parameter_source(
        AccessPath(Root(Root::Kind::Argument, i)),
        generator::source(
            context,
            method,
            /* kind */ "JavascriptInterfaceUserInput"));
  }
  EXPECT_THAT(
      JsonModelGenerator(
          "JavaScriptInterfaceSourceGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(model));
}
