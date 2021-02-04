/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <RedexContext.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/SanitizersOptions.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {
namespace test {

Test::Test() {
  g_redex = new RedexContext();
}

Test::~Test() {
  delete g_redex;
}

ContextGuard::ContextGuard() {
  g_redex = new RedexContext();
}

ContextGuard::~ContextGuard() {
  delete g_redex;
}

Context make_empty_context() {
  Context context;
  context.methods = std::make_unique<Methods>();
  context.positions = std::make_unique<Positions>();
  return context;
}

Context make_context(const DexStore& store) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_paths */ std::vector<std::string>{},
      /* rules_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_model_generation */ true);
  context.stores = {store};
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.positions =
      std::make_unique<Positions>(*context.options, context.stores);
  context.types = std::make_unique<Types>(context.stores);
  context.class_properties = std::make_unique<ClassProperties>(
      *context.options, context.stores, *context.features);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options, *context.methods, context.stores);
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.methods,
      *context.types,
      *context.class_hierarchies,
      *context.overrides,
      *context.features);
  return context;
}

Json::Value parse_json(std::string input) {
  return JsonValidation::parse_json(std::move(input));
}

Json::Value sorted_json(const Json::Value& value) {
  if (value.isArray() && value.size() > 0) {
    std::vector<Json::Value> elements;
    for (const auto& element : value) {
      elements.push_back(sorted_json(element));
    }
    std::sort(
        elements.begin(),
        elements.end(),
        [](const Json::Value& left, const Json::Value& right) {
          return left < right;
        });

    auto sorted = Json::Value(Json::arrayValue);
    for (auto& element : elements) {
      sorted.append(element);
    }
    return sorted;
  }

  if (value.isObject()) {
    auto sorted = Json::Value(Json::objectValue);
    for (auto member : value.getMemberNames()) {
      sorted[member] = sorted_json(value[member]);
    }
    return sorted;
  }

  return value;
}

boost::filesystem::path find_dex_path(
    const boost::filesystem::path& test_directory) {
  auto filename = test_directory.filename().native();
  const char* dex_path_from_environment = std::getenv(filename.c_str());
  if (dex_path_from_environment) {
    return dex_path_from_environment;
  } else {
    // Buck does not set environment variables when invoked with `buck run` but
    // this is useful for debugging. Working around by using a default path.
    // NOTE: we assume the test is run in dev mode.

    auto integration_test_directory =
        test_directory.parent_path().parent_path().string();
    auto dex_file_directory = integration_test_directory.substr(
        integration_test_directory.find("fbandroid"));
    for (const auto& directory : boost::filesystem::directory_iterator(
             boost::filesystem::current_path() / "buck-out/dev/gen")) {
      auto dex_path = directory.path() / dex_file_directory /
          fmt::format("test-dex-{}", filename) /
          fmt::format("test-class-{}.dex", filename);
      if (boost::filesystem::exists(dex_path)) {
        return dex_path;
      }
    }

    throw std::logic_error("Unable to find .dex");
  }
}

std::vector<std::string> sub_directories(
    const boost::filesystem::path& directory) {
  std::vector<std::string> directories;

  for (const auto& sub_directory :
       boost::filesystem::directory_iterator(directory)) {
    directories.push_back(boost::filesystem::canonical(sub_directory).string());
  }

  return directories;
}

std::string normalize_json_lines(const std::string& input) {
  std::vector<std::string> lines;
  boost::split(lines, input, boost::is_any_of("\n"));

  std::vector<std::string> normalized_elements;
  std::optional<std::string> buffer = std::nullopt;

  for (std::size_t index = 0; index < lines.size(); index++) {
    auto& line = lines[index];
    if (boost::starts_with(line, "//") || line.size() == 0) {
      normalized_elements.push_back(line);
      continue;
    }

    if (line == "{") {
      buffer = line;
      continue;
    } else if (line == "}" || !buffer) {
      // Consume a line or the buffer.
      if (!buffer) {
        buffer = "";
      }
      *buffer += line;

      auto value = test::parse_json(*buffer);
      auto normalized = JsonValidation::to_styled_string(sorted_json(value));
      boost::trim(normalized);
      normalized_elements.push_back(normalized);

      buffer = std::nullopt;
    } else {
      *buffer += "\n" + line;
      continue;
    }
  }

  std::sort(normalized_elements.begin(), normalized_elements.end());
  return boost::join(normalized_elements, "\n").substr(1) + "\n";
}

} // namespace test
} // namespace marianatrench
