/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/string_file.hpp>
#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <json/json.h>

#include <DexLoader.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <JarLoader.h>
#include <RedexContext.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

struct IntegrationTest : public test::ContextGuard,
                         public testing::TestWithParam<std::string> {};

boost::filesystem::path root_directory() {
  return boost::filesystem::path(__FILE__).parent_path() / "code";
}

std::string load_expected_json(
    const boost::filesystem::path& directory,
    const std::string& filename) {
  try {
    std::string loaded_output;
    boost::filesystem::load_string_file(directory / filename, loaded_output);
    return test::normalize_json_lines(loaded_output);
  } catch (std::exception& error) {
    ERROR(1, "Unable to load `{}`: {}", filename, error.what());
    return "";
  }
}

void compare_expected(
    const boost::filesystem::path& directory,
    const std::string& filename,
    const std::string& expected,
    std::string actual) {
  actual = test::normalize_json_lines(actual);

  if (expected != actual) {
    boost::filesystem::save_string_file(
        directory / fmt::format("{}.actual", filename), actual);
  }
  EXPECT_EQ(actual, expected);
}

void compare_expected(
    const boost::filesystem::path& directory,
    const std::string& filename,
    const std::string& expected,
    const Json::Value& actual) {
  auto writer = JsonValidation::compact_writer();
  std::ostringstream actual_string;
  actual_string << "// @";
  actual_string << "generated\n";
  writer->write(actual, &actual_string);
  actual_string << "\n";
  compare_expected(directory, filename, expected, actual_string.str());
}

} // namespace

namespace marianatrench {

TEST_P(IntegrationTest, CompareFlows) {
  boost::filesystem::path name = GetParam();
  LOG(1, "Test case `{}`", name);
  boost::filesystem::path directory = root_directory() / name;

  std::string expected_output =
      load_expected_json(directory, "expected_output.json");
  std::string expected_class_hierarchies =
      load_expected_json(directory, "expected_class_hierarchies.json");
  std::string expected_overrides =
      load_expected_json(directory, "expected_overrides.json");
  std::string expected_call_graph =
      load_expected_json(directory, "expected_call_graph.json");
  std::string expected_dependencies =
      load_expected_json(directory, "expected_dependencies.json");

  Context context;

  std::vector<std::string> lifecycles_paths;
  auto lifecycles_path = directory / "lifecycles.json";
  if (boost::filesystem::exists(lifecycles_path)) {
    lifecycles_paths.emplace_back(lifecycles_path.native());
  }

  std::vector<std::string> shims_paths;
  auto shims_path = directory / "shims.json";
  if (boost::filesystem::exists(shims_path)) {
    shims_paths.emplace_back(shims_path.native());
  }

  std::string graphql_metadata_paths;
  auto graphql_metadata_filepath = directory / "graphql_metadata.json";
  if (boost::filesystem::exists(graphql_metadata_filepath)) {
    graphql_metadata_paths = graphql_metadata_filepath.string();
  }

  auto generator_configuration_file = directory / "/generator_config.json";
  std::vector<ModelGeneratorConfiguration> model_generators_configurations;
  if (boost::filesystem::exists(generator_configuration_file)) {
    LOG(3, "Found generator configuration.");

    Json::Value json =
        JsonValidation::parse_json_file(generator_configuration_file);
    for (const auto& value : JsonValidation::null_or_array(json)) {
      model_generators_configurations.emplace_back(
          ModelGeneratorConfiguration::from_json(value));
    }
  }

  auto model_generators_file = directory / "/model_generators.json";
  std::vector<std::string> model_generator_search_paths;
  if (boost::filesystem::exists(model_generators_file)) {
    LOG(3, "Found model generator. Will run model generation.");
    model_generator_search_paths.emplace_back(directory.native());
    model_generators_configurations.emplace_back(
        model_generators_file.stem().native());
  }
  auto field_models_file = directory / "/field_models.json";

  // Read the configuration for this test case.
  context.options = std::make_unique<Options>(
      /* models_paths */
      std::vector<std::string>{(directory / "/models.json").native()},
      /* field_models_path */
      boost::filesystem::exists(field_models_file)
          ? std::vector<std::string>{field_models_file.string()}
          : std::vector<std::string>{},
      /* rules_paths */
      std::vector<std::string>{(directory / "/rules.json").native()},
      /* lifecycles_paths */
      lifecycles_paths,
      /* shims_path */ shims_paths,
      /* graphql_metadata_path */ graphql_metadata_paths,
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ false,
      /* skip_analysis */ false,
      model_generators_configurations,
      model_generator_search_paths,
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ true,
      /* source_root_directory */ directory.string(),
      /* enable_cross_component_analysis */ true);

  // Load test Java classes
  boost::filesystem::path dex_path = test::find_dex_path(directory);
  LOG(3, "Dex path is `{}`", dex_path);

  DexMetadata dexmetadata;
  dexmetadata.set_id("classes");
  DexStore root_store(dexmetadata);
  root_store.add_classes(load_classes_from_dex(
      DexLocation::make_location("dex", dex_path.c_str())));
  context.stores.push_back(root_store);

  // Run the analysis.
  auto tool = MarianaTrench();
  auto registry = tool.analyze(context);

  // Compare the results.
  compare_expected(
      directory,
      "expected_output.json",
      expected_output,
      registry.dump_models());
  compare_expected(
      directory,
      "expected_class_hierarchies.json",
      expected_class_hierarchies,
      context.class_hierarchies->to_json());
  compare_expected(
      directory,
      "expected_overrides.json",
      expected_overrides,
      context.overrides->to_json());
  compare_expected(
      directory,
      "expected_call_graph.json",
      expected_call_graph,
      context.call_graph->to_json());
  compare_expected(
      directory,
      "expected_dependencies.json",
      expected_dependencies,
      context.dependencies->to_json());
}

MT_INSTANTIATE_TEST_SUITE_P(
    Integration,
    IntegrationTest,
    testing::ValuesIn(test::sub_directories(root_directory())));

} // namespace marianatrench
