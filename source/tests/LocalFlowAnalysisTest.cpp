/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include <DexClass.h>
#include <DexStore.h>

#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/local_flow/LocalFlowAnalysis.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {
namespace local_flow {

namespace {

class LocalFlowAnalysisTest : public test::Test {
 protected:
  DexStoresVector stores_{DexStore("test_store")};
  Methods methods_{stores_};
};

const Method* make_external_method(
    Methods& methods,
    const std::string& signature,
    bool is_static = false) {
  // `make_method` creates a `DexMethod` but returns a `DexMethodRef`.
  auto* dex_method = static_cast<DexMethod*>(DexMethod::make_method(signature));
  dex_method->set_access(is_static ? ACC_PUBLIC | ACC_STATIC : ACC_PUBLIC);
  dex_method->set_external();
  return methods.create(dex_method);
}

std::vector<std::string> stub_flows(const LocalFlowMethodResult& stub) {
  std::vector<std::string> flows;
  for (const auto& constraint : stub.constraints.constraints()) {
    flows.push_back(
        constraint.source.to_string() + " -> " + constraint.target.to_string());
  }
  return flows;
}

} // namespace

TEST_F(LocalFlowAnalysisTest, ExternalStubOfNonVoidInstanceMethod) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/content/Intent;.putExtra:(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_FALSE(stub->is_static);
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre(
          "p0 -> result",
          "p0 -> this_out",
          "p1 -> result",
          "p1 -> this_out",
          "p2 -> result",
          "p2 -> this_out"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfGetter) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/content/Intent;.getStringExtra:(Ljava/lang/String;)Ljava/lang/String;");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre(
          "p0 -> result", "p0 -> this_out", "p1 -> result"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfResultWithoutArguments) {
  const auto* method =
      make_external_method(methods_, "Ljava/util/List;.size:()I");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub), testing::UnorderedElementsAre("p0 -> this_out"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfVoidInstanceMethod) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/os/Bundle;.putString:(Ljava/lang/String;Ljava/lang/String;)V");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre(
          "p0 -> this_out", "p1 -> this_out", "p2 -> this_out"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfConstructor) {
  const auto* method = make_external_method(
      methods_, "Landroid/content/Intent;.<init>:(Landroid/net/Uri;)V");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre("p0 -> this_out", "p1 -> this_out"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfStaticMethod) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/net/Uri;.parse:(Ljava/lang/String;)Landroid/net/Uri;",
      /* is_static */ true);

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_TRUE(stub->is_static);
  EXPECT_THAT(stub_flows(*stub), testing::UnorderedElementsAre("p0 -> result"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfBuilderMethod) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/app/Notification$Builder;.extend:(Landroid/app/Notification$Extender;)Landroid/app/Notification$Builder;");

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre(
          "p0 -> result", "p0 -> this_out", "p1 -> result", "p1 -> this_out"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfStaticSetterLikeMethod) {
  const auto* method = make_external_method(
      methods_,
      "Ljava/util/Collections;.addAll:(Ljava/util/Collection;[Ljava/lang/Object;)Z",
      /* is_static */ true);

  auto stub = LocalFlowAnalysis::external_stub(method);
  ASSERT_TRUE(stub.has_value());
  EXPECT_THAT(
      stub_flows(*stub),
      testing::UnorderedElementsAre("p0 -> result", "p1 -> result"));
}

TEST_F(LocalFlowAnalysisTest, ExternalStubOfObjectMethods) {
  for (const auto* signature :
       {"Ljava/lang/Object;.hashCode:()I",
        "Ljava/lang/Object;.getClass:()Ljava/lang/Class;"}) {
    auto stub = LocalFlowAnalysis::external_stub(
        make_external_method(methods_, signature));
    ASSERT_TRUE(stub.has_value()) << signature;
    EXPECT_THAT(
        stub_flows(*stub), testing::UnorderedElementsAre("p0 -> this_out"))
        << signature;
  }
}

TEST_F(LocalFlowAnalysisTest, NoExternalStubForMethodWithCode) {
  Scope scope;
  auto* dex_method =
      marianatrench::redex::create_void_method(scope, "LFoo;", "bar");

  EXPECT_FALSE(
      LocalFlowAnalysis::external_stub(methods_.create(dex_method))
          .has_value());
}

TEST_F(LocalFlowAnalysisTest, NoExternalStubWithoutFlows) {
  const auto* method = make_external_method(
      methods_,
      "Landroid/util/Log;.flush:()V",
      /* is_static */ true);

  EXPECT_FALSE(LocalFlowAnalysis::external_stub(method).has_value());
}

TEST_F(LocalFlowAnalysisTest, BundleMetadata) {
  EXPECT_EQ(LocalFlowAnalysis::bundle_metadata("abc"), test::parse_json(R"#({
        "tool": "mariana_trench",
        "filename_spec": "local_flow@*.json",
        "commit": "abc",
        "local_flow_format_version": 2
      })#"));
  EXPECT_EQ(
      LocalFlowAnalysis::bundle_metadata(std::nullopt)["commit"].asString(),
      "");
}

} // namespace local_flow
} // namespace marianatrench
