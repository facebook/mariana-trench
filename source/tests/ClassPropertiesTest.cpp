/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <gtest/gtest.h>

#include <Creators.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <RedexContext.h>
#include <RedexResources.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class ClassPropertiesTest : public test::Test {};

// Mock AndroidResources.
class MockAndroidResources : public AndroidResources {
 public:
  explicit MockAndroidResources() : AndroidResources("") {}
  ~MockAndroidResources() override {}

  boost::optional<int32_t> get_min_sdk() override {
    return boost::none;
  }

  boost::optional<std::string> get_manifest_package_name() override {
    return boost::none;
  }

  ManifestClassInfo get_manifest_class_info() override {
    ManifestClassInfo info;

    info.component_tags.emplace_back(ComponentTagInfo(
        /* tag */ ComponentTag::Activity,
        /* classname */ "LMainActivity;",
        /* is_exported */ BooleanXMLAttribute::True,
        /* permission */ "",
        /* protection_level */ ""));
    info.component_tags.emplace_back(ComponentTagInfo(
        /* tag */ ComponentTag::Activity,
        /* classname */ "LParentActivity;",
        /* is_exported */ BooleanXMLAttribute::False,
        /* permission */ "",
        /* protection_level */ ""));

    return info;
  }

  std::unordered_set<uint32_t> get_xml_reference_attributes(
      const std::string&) override {
    return {};
  }

  void collect_layout_classes_and_attributes_for_file(
      const std::string&,
      const std::unordered_set<std::string>&,
      std::unordered_set<std::string>*,
      std::unordered_multimap<std::string, std::string>*) override {}

  size_t remap_xml_reference_attributes(
      const std::string&,
      const std::map<uint32_t, uint32_t>&) override {
    return 0;
  }

  std::unique_ptr<ResourceTableFile> load_res_table() override {
    return nullptr;
  }

  std::unordered_set<std::string> find_all_xml_files() override {
    return {};
  }

  std::vector<std::string> find_resources_files() override {
    return {};
  }

  std::string get_base_assets_dir() override {
    return "";
  }

 protected:
  std::vector<std::string> find_res_directories() override {
    return {};
  }

  std::vector<std::string> find_lib_directories() override {
    return {};
  }

  bool rename_classes_in_layout(
      const std::string&,
      const std::map<std::string, std::string>&,
      size_t*) override {
    return false;
  }

  void obfuscate_xml_files(
      const std::unordered_set<std::string>& /* unused */,
      const std::unordered_set<std::string>& /* unused */) override {}
};

Context make_context(const Scope& scope) {
  DexStore store("test_store");
  store.add_classes(scope);

  Context context = test::make_context(store);
  context.class_properties = std::make_unique<ClassProperties>(
      *context.options,
      context.stores,
      *context.features,
      *context.dependencies,
      std::make_unique<MockAndroidResources>());
  return context;
}

} // anonymous namespace

TEST_F(ClassPropertiesTest, InvokeUtil) {
  Scope scope;

  // MainActivity[exported]::onCreate() --> Util::call()
  auto* dex_util = redex::create_void_method(scope, "LUtil;", "call");
  auto* dex_activity = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util = context.methods->get(dex_util);
  auto* activity = context.methods->get(dex_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_class = context.features->get("via-class:LMainActivity;");
  EXPECT_EQ(
      class_properties.issue_features(activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
}

TEST_F(ClassPropertiesTest, MultipleCallers) {
  Scope scope;

  // MainActivity[exported]::onCreate() --> Util::call()
  // ParentActivity[unexported]::onCreate() --> Util::call()
  // Other::onCreate() --> Util::call()
  auto* dex_util = redex::create_void_method(scope, "LUtil;", "call");
  auto* dex_other = redex::create_method(scope, "LOther;", R"(
    (method (public) "LOther;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");
  auto* dex_main_activity = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");
  auto* dex_parent_activity =
      redex::create_method(scope, "LParentActivity;", R"(
    (method (public) "LParentActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util = context.methods->get(dex_util);
  auto* other = context.methods->get(dex_other);
  auto* main_activity = context.methods->get(dex_main_activity);
  auto* parent_activity = context.methods->get(dex_parent_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_caller_unexported = context.features->get("via-caller-unexported");
  auto via_class = context.features->get("via-class:LMainActivity;");
  EXPECT_EQ(
      class_properties.issue_features(main_activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(parent_activity),
      FeatureMayAlwaysSet({via_caller_unexported}));
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
  EXPECT_EQ(class_properties.issue_features(other), FeatureMayAlwaysSet({}));
}

TEST_F(ClassPropertiesTest, MultipleCallersMultipleHops) {
  Scope scope;

  // MainActivity[exported]::onCreate() --> Util::firstHop()
  //   -->UtilInner::call()
  // ParentActivity[unexported]::onCreate() --> UtilInner::call()
  auto* dex_util_inner =
      redex::create_void_method(scope, "LUtilInner;", "call");
  auto* dex_util = redex::create_method(scope, "LUtil;", R"(
    (method (public) "LUtil;.firstHop:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtilInner;.call:()V")
      (return-void)
     )
    )
  )");
  auto* dex_main_activity = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.firstHop:()V")
      (return-void)
     )
    )
  )");
  auto* dex_parent_activity =
      redex::create_method(scope, "LParentActivity;", R"(
    (method (public) "LParentActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtilInner;.call:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util_inner = context.methods->get(dex_util_inner);
  auto* util = context.methods->get(dex_util);
  auto* main_activity = context.methods->get(dex_main_activity);
  auto* parent_activity = context.methods->get(dex_parent_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_caller_unexported = context.features->get("via-caller-unexported");
  auto via_class = context.features->get("via-class:LMainActivity;");
  EXPECT_EQ(
      class_properties.issue_features(main_activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
  EXPECT_EQ(
      class_properties.issue_features(util_inner),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
  EXPECT_EQ(
      class_properties.issue_features(parent_activity),
      FeatureMayAlwaysSet({via_caller_unexported}));
}

TEST_F(ClassPropertiesTest, UnexportedHop) {
  Scope scope;

  // MainActivity[exported]::onCreate()
  //   --> ParentActivity[unexported]::onCreate()
  //   --> Util::call()
  auto* dex_util = redex::create_void_method(scope, "LUtil;", "call");
  auto* dex_main_activity = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LParentActivity;.onCreate:()V")
      (return-void)
     )
    )
  )");
  auto* dex_parent_activity =
      redex::create_method(scope, "LParentActivity;", R"(
    (method (public) "LParentActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util = context.methods->get(dex_util);
  auto* main_activity = context.methods->get(dex_main_activity);
  auto* parent_activity = context.methods->get(dex_parent_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_caller_unexported = context.features->get("via-caller-unexported");
  auto via_class = context.features->get("via-class:LParentActivity;");
  EXPECT_EQ(
      class_properties.issue_features(main_activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(parent_activity),
      FeatureMayAlwaysSet({via_caller_unexported}));
  // Stop traversal at unexported as it may introduce FPs.
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_unexported, via_class}));
}

TEST_F(ClassPropertiesTest, Cyclic) {
  Scope scope;

  // MainActivity[exported]::onCreate()
  //   --> Activity1::onCreate()
  // Activity1::onCreate()
  //   --> Activity2::onCreate()
  //   --> Util::call()
  // Activity2::onCreate()
  //   --> MainActivity::onCreate()
  //   --> Util::call()
  auto* dex_util = redex::create_void_method(scope, "LUtil;", "call");
  auto* dex_activity1 = redex::create_method(scope, "LActivity1;", R"(
    (method (public) "LActivity1;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (invoke-direct (v0) "LActivity2;.onCreate:()V")
      (return-void)
     )
    )
  )");
  auto* dex_activity2 = redex::create_method(scope, "LActivity2;", R"(
    (method (public) "LActivity2;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (invoke-direct (v0) "LMainActivity;.onCreate:()V")
      (return-void)
     )
    )
  )");
  auto* dex_main_activity = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LActivity1;.onCreate:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util = context.methods->get(dex_util);
  auto* activity1 = context.methods->get(dex_activity1);
  auto* activity2 = context.methods->get(dex_activity2);
  auto* main_activity = context.methods->get(dex_main_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_class = context.features->get("via-class:LMainActivity;");
  EXPECT_EQ(
      class_properties.issue_features(main_activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(activity1),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
  EXPECT_EQ(
      class_properties.issue_features(activity2),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_class}));
}

TEST_F(ClassPropertiesTest, NestedClass) {
  Scope scope;

  // MainActivity[exported]
  //   MainActivity$NestedClass::call() --> Util::call()
  auto* dex_util = redex::create_void_method(scope, "LUtil;", "call");
  auto* dex_activity =
      redex::create_method(scope, "LMainActivity$NestedClass;", R"(
    (method (public) "LMainActivity$NestedClass;.call:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (return-void)
     )
    )
  )");

  auto context = make_context(scope);
  auto& class_properties = *context.class_properties;
  auto* util = context.methods->get(dex_util);
  auto* activity = context.methods->get(dex_activity);

  auto via_dependency_graph = context.features->get("via-dependency-graph");
  auto via_caller_exported = context.features->get("via-caller-exported");
  auto via_nested_class =
      context.features->get("via-class:LMainActivity$NestedClass;");
  EXPECT_EQ(
      class_properties.issue_features(activity),
      FeatureMayAlwaysSet({via_caller_exported}));
  EXPECT_EQ(
      class_properties.issue_features(util),
      FeatureMayAlwaysSet(
          {via_dependency_graph, via_caller_exported, via_nested_class}));
}
