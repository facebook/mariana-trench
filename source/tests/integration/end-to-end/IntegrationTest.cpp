/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
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
  Json::FastWriter writer;
  std::string actual_string;
  actual_string.append("// @");
  actual_string.append("generated\n");
  actual_string.append(Json::FastWriter().write(actual));
  compare_expected(directory, filename, expected, actual_string);
}

} // namespace

namespace marianatrench {

TEST_P(IntegrationTest, CompareFlows) {
  boost::filesystem::path directory = GetParam();
  LOG(1, "Test case {}", directory);

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

  // Read the configuration for this test case.
  context.options = std::make_unique<Options>(
      /* models_path */
      std::vector<std::string>{(directory / "/models.json").native()},
      /* rules_path */
      std::vector<std::string>{(directory / "/rules.json").native()},
      /* sequential */ true,
      /* skip_source_indexing */ false,
      /* skip_model_generation */ true,
      /* source_root_directory */ directory.string());

  // Load test Java classes
  boost::filesystem::path dex_path = test::find_dex_path(directory);
  LOG(3, "Dex path is {}", dex_path);

  DexMetadata dexmetadata;
  dexmetadata.set_id("classes");
  DexStore root_store(dexmetadata);
  root_store.add_classes(load_classes_from_dex(dex_path.c_str()));
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

INSTANTIATE_TEST_CASE_P(
    Integration,
    IntegrationTest,
    testing::ValuesIn(test::sub_directories(
        boost::filesystem::path(__FILE__).parent_path() / "code")));

} // namespace marianatrench
