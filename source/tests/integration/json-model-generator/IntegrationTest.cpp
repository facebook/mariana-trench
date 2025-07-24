/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <DexLoader.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <JarLoader.h>
#include <RedexContext.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

struct JsonModelGeneratorIntegrationTest
    : public test::ContextGuard,
      public testing::TestWithParam<std::string> {};

std::filesystem::path root_directory() {
  return std::filesystem::path(__FILE__).parent_path() / "code";
}

} // namespace

namespace marianatrench {

TEST_P(JsonModelGeneratorIntegrationTest, CompareModels) {
  std::filesystem::path name = GetParam();
  LOG(1, "Test case `{}`", name);
  std::filesystem::path directory = root_directory() / name;

  Context context;

  // Read the configuration for this test case.
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);

  // Read from the expected generated models
  std::string expected_output;
  try {
    filesystem::load_string_file(
        directory / "expected_output.json", expected_output);
    expected_output = test::normalize_json_lines(expected_output);
  } catch (std::exception& error) {
    LOG(1, "Unable to load expected models: {}", error.what());
  }

  // Load test Java classes
  std::filesystem::path dex_path = test::find_dex_path(directory);
  LOG(3, "Dex path is `{}`", dex_path);

  // Properly initialize context
  DexMetadata dexmetadata;
  dexmetadata.set_id("classes");
  DexStore root_store(dexmetadata);
  root_store.add_classes(load_classes_from_dex(
      DexLocation::make_location("dex", dex_path.c_str())));
  context.stores.push_back(root_store);
  const auto& options = *context.options;
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  MethodMappings method_mappings{*context.methods};
  context.fields = std::make_unique<Fields>(context.stores);
  context.positions = std::make_unique<Positions>(options, context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.options->analysis_mode(), context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options,
      context.options->analysis_mode(),
      *context.methods,
      context.stores);
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.types,
      *context.class_hierarchies,
      *context.feature_factory,
      *context.heuristics,
      *context.methods,
      *context.fields,
      *context.overrides,
      method_mappings,
      LifecycleMethods{},
      Shims{/* global_shims_size */ 0});

  // Run a model generator and compare output
  auto [models, field_models] =
      JsonModelGenerator::from_file(
          "TestModelGenerator", context, (directory / "model_generator.json"))
          .run(*context.methods, *context.fields);
  auto registry =
      Registry(context, models, field_models, /* literal_models */ {});

  std::string actual_output =
      test::normalize_json_lines(registry.dump_models());

  if (expected_output != actual_output) {
    filesystem::save_string_file(
        directory / "expected_output.json.actual", actual_output);
  }
  EXPECT_TRUE(actual_output == expected_output);
}

MT_INSTANTIATE_TEST_SUITE_P(
    JsonModelGeneratorIntegration,
    JsonModelGeneratorIntegrationTest,
    testing::ValuesIn(test::sub_directories(root_directory())));

} // namespace marianatrench
