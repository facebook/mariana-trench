/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <json/json.h>

#include <DexLoader.h>
#include <DexStore.h>
#include <RedexContext.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>
#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;
using namespace marianatrench::local_flow;

namespace {

struct IntegrationTest : public test::ContextGuard,
                         public testing::TestWithParam<std::string> {};

std::filesystem::path root_directory() {
  return std::filesystem::path(__FILE__).parent_path() / "code";
}

std::string load_expected(
    const std::filesystem::path& directory,
    const std::string& filename) {
  try {
    std::string loaded;
    filesystem::load_string_file(directory / filename, loaded);
    return loaded;
  } catch (std::exception& error) {
    LOG(1, "Unable to load `{}`: {}", filename, error.what());
    return "";
  }
}

void compare_expected(
    const std::filesystem::path& directory,
    const std::string& filename,
    const std::string& expected,
    const std::string& actual) {
  if (expected != actual) {
    filesystem::save_string_file(
        directory / fmt::format("{}.actual", filename), actual);
  }
  EXPECT_EQ(actual, expected);
}

/**
 * Filter methods to only those belonging to test case classes (exclude
 * java.lang.Object and other standard library methods).
 */
bool is_test_class_method(const Method* method) {
  auto class_name = std::string(method->get_class()->str());
  return class_name.find("Lcom/facebook/marianatrench/integrationtests/") !=
      std::string::npos;
}

} // namespace

TEST_P(IntegrationTest, CompareLocalFlows) {
  std::filesystem::path name = GetParam();
  LOG(1, "Test case `{}`", name);
  std::filesystem::path directory = root_directory() / name;

  std::string expected_output =
      load_expected(directory, "expected_output.json");

  // Build context from DEX
  Context context;

  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);

  // Load test Java classes from DEX
  std::filesystem::path dex_path = test::find_dex_path(directory);
  LOG(3, "Dex path is `{}`", dex_path);

  DexMetadata dexmetadata;
  dexmetadata.set_id("classes");
  DexStore root_store(dexmetadata);
  root_store.add_classes(load_classes_from_dex(
      DexLocation::make_location("dex", dex_path.c_str())));
  context.stores.push_back(root_store);

  // Initialize infrastructure
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  // Positions must be created before ControlFlowGraphs because CFG
  // building destroys the raw method instructions needed for line indexing.
  context.positions =
      std::make_unique<Positions>(*context.options, context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.options->analysis_mode(), context.stores);
  context.class_intervals = std::make_unique<ClassIntervals>(
      *context.options, context.options->analysis_mode(), context.stores);

  const auto* positions = context.positions.get();

  // Collect all methods from test classes, sorted for deterministic output
  std::vector<const Method*> test_methods;
  for (const auto* method : *context.methods) {
    if (is_test_class_method(method)) {
      test_methods.push_back(method);
    }
  }
  std::sort(
      test_methods.begin(),
      test_methods.end(),
      [](const Method* a, const Method* b) {
        return a->signature() < b->signature();
      });

  // Run method analysis
  auto results = Json::Value(Json::arrayValue);
  for (const auto* method : test_methods) {
    auto method_result = LocalFlowMethodAnalyzer::analyze(
        method, /* max_structural_depth */ 10, positions);
    if (method_result.has_value()) {
      results.append(test::sorted_json(method_result->to_json()));
    }
  }

  // Run class analysis
  auto class_results = LocalFlowClassAnalyzer::analyze_classes(context);
  std::sort(
      class_results.begin(),
      class_results.end(),
      [](const LocalFlowClassResult& a, const LocalFlowClassResult& b) {
        return a.class_name < b.class_name;
      });
  for (const auto& class_result : class_results) {
    // Only include test classes
    if (class_result.class_name.find(
            "Lcom/facebook/marianatrench/integrationtests/") !=
        std::string::npos) {
      results.append(test::sorted_json(class_result.to_json()));
    }
  }

  // Build output
  auto value = Json::Value(Json::objectValue);
  auto metadata = Json::Value(Json::objectValue);
  metadata
      ["@"
       "generated"] = Json::Value(true);
  value["metadata"] = metadata;
  value["results"] = results;

  auto actual_output = JsonWriter::to_styled_string(value);
  // Normalize whitespace
  while (actual_output.find(" \n") != std::string::npos) {
    auto pos = actual_output.find(" \n");
    actual_output.replace(pos, 2, "\n");
  }
  actual_output += "\n";

  compare_expected(
      directory, "expected_output.json", expected_output, actual_output);
}

MT_INSTANTIATE_TEST_SUITE_P(
    LocalFlowE2E,
    IntegrationTest,
    testing::ValuesIn(test::sub_directories(root_directory())));
