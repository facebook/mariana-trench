/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>

#include <IRAssembler.h>
#include <RedexContext.h>
#include <Show.h>
#include <boost/regex.hpp>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>
#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;
using namespace marianatrench::local_flow;

namespace {

struct TestSource {
  std::string class_name;
  std::vector<std::string> methods;
  std::vector<std::string> abstract_methods;
  std::optional<std::string> super = std::nullopt;
};

class Parser {
 public:
  static std::vector<TestSource> parse(const std::string& source) {
    return Parser().parse_source(source);
  }

  static std::vector<TestSource> sort_by_hierarchy(
      std::vector<TestSource>& sources) {
    // Ensure dependencies are created first, if they contain methods.
    auto sorted_sources = std::vector<TestSource>();
    while (sources.size() != sorted_sources.size()) {
      auto unsorted_sources = std::vector<TestSource>();
      for (const auto& source : UnorderedIterable(sources)) {
        if (!source.super ||
            std::find_if(
                sorted_sources.begin(),
                sorted_sources.end(),
                [source](const TestSource& sorted_source) {
                  return sorted_source.class_name == source.super;
                }) != sorted_sources.end()) {
          sorted_sources.push_back(source);
        } else {
          unsorted_sources.push_back(source);
        }
      }
      if (unsorted_sources.size() < sources.size()) {
        sources = unsorted_sources;
      } else {
        // Remaining classes do not have dependencies with methods.
        for (const auto& source : unsorted_sources) {
          if (source.super) {
            DexType::make_type(DexString::make_string(*source.super));
          }
          sorted_sources.push_back(source);
        }
        break;
      }
    }
    return sorted_sources;
  }

 private:
  void parse_method(
      const std::string& source,
      const std::optional<std::string>& super,
      const bool abstract) {
    // Extract class name.
    auto offset = source.find("\"") + 1;
    auto class_name = source.substr(offset, source.find(".") - offset);

    class_name_to_super_[class_name] = super;

    // Preprocess LINE(N) markers.
    auto preprocessed_source = source;
    size_t start = 0;
    while ((start = preprocessed_source.find("LINE(", start)) !=
           std::string::npos) {
      auto end = preprocessed_source.find(")", start);
      auto line = std::atoi(
          preprocessed_source.substr(start + 5, end - start - 5).c_str());
      auto replacement =
          fmt::format("(.pos \"LFlow;.flow:()V\" \"File.java\" \"{}\")", line);
      preprocessed_source.replace(start, end - start + 1, replacement);
      start += replacement.size();
    }
    if (abstract) {
      class_name_to_abstract_methods_[class_name].push_back(
          preprocessed_source);
      // Ensure the non-abstract map entry exists without overwriting.
      class_name_to_methods_[class_name];
    } else {
      class_name_to_methods_[class_name].push_back(preprocessed_source);
      // Ensure the abstract map entry exists without overwriting.
      class_name_to_abstract_methods_[class_name];
    }
    if (std::find(class_order_.begin(), class_order_.end(), class_name) ==
        class_order_.end()) {
      class_order_.push_back(class_name);
    }
  }

  std::vector<TestSource> parse_source(const std::string& source) {
    std::vector<TestSource> parsed_sources;
    std::vector<std::string> lines;
    boost::split(lines, source, boost::is_any_of("\n"));

    std::vector<std::string> buffer;
    std::optional<std::string> super = std::nullopt;
    bool abstract = false;

    for (const auto& line : lines) {
      if (line.starts_with("(method ") || line.starts_with("(super ") ||
          line.starts_with("(abstract method ")) {
        if (!buffer.empty()) {
          parse_method(boost::join(buffer, "\n"), super, abstract);
          buffer.clear();
          super = std::nullopt;
          abstract = false;
        }
      }

      if (line.starts_with("(super ")) {
        auto offset = line.find("\"") + 1;
        super = line.substr(offset, line.find("\"", offset) - offset);
        continue;
      }

      if (line.starts_with("(abstract method ")) {
        abstract = true;
        auto modified_line = line;
        buffer.push_back(
            modified_line.replace(0, sizeof("(abstract method"), "(method"));
        continue;
      }

      buffer.push_back(line);
    }

    if (!buffer.empty()) {
      parse_method(boost::join(buffer, "\n"), super, abstract);
    }

    for (const auto& class_name : class_order_) {
      parsed_sources.push_back(
          TestSource{
              class_name,
              /* methods */ class_name_to_methods_.at(class_name),
              /* abstract methods */
              class_name_to_abstract_methods_.at(class_name),
              /* super */ class_name_to_super_.at(class_name),
          });
    }
    return parsed_sources;
  }

  std::vector<std::string> class_order_;
  std::unordered_map<std::string, std::vector<std::string>>
      class_name_to_methods_;
  std::unordered_map<std::string, std::vector<std::string>>
      class_name_to_abstract_methods_;
  std::unordered_map<std::string, std::optional<std::string>>
      class_name_to_super_;
};

struct IntegrationTest : public test::ContextGuard,
                         public testing::TestWithParam<std::string> {};

std::filesystem::path root_directory() {
  return std::filesystem::path(__FILE__).parent_path();
}

std::vector<std::string> sexp_paths() {
  auto root = root_directory();
  std::vector<std::string> paths;

  for (const auto& directory :
       std::filesystem::recursive_directory_iterator(root)) {
    auto path = directory.path();
    if (path.extension() != ".sexp") {
      continue;
    }
    paths.push_back(std::filesystem::relative(path, root).native());
  }

  return paths;
}

} // namespace

TEST_P(IntegrationTest, ReturnsExpectedResults) {
  std::filesystem::path name = GetParam();
  LOG(1, "Test case `{}`", name);
  std::filesystem::path path = root_directory() / name;

  Scope scope;
  std::vector<const DexMethod*> methods;
  std::set<std::string> sexp_class_names;

  std::string unparsed_source;
  filesystem::load_string_file(path, unparsed_source);
  auto sources = Parser::parse(unparsed_source);
  auto sorted_sources = Parser::sort_by_hierarchy(sources);

  // Create redex classes.
  for (const auto& source : sorted_sources) {
    sexp_class_names.insert(source.class_name);
    auto* super = source.super ? DexType::get_type(*source.super) : nullptr;
    std::vector<marianatrench::redex::DexMethodSpecification>
        method_specifications;
    for (const auto& method : source.methods) {
      method_specifications.push_back(
          marianatrench::redex::DexMethodSpecification{method});
    }
    for (const auto& method : source.abstract_methods) {
      method_specifications.push_back(
          marianatrench::redex::DexMethodSpecification{/* body */ method,
                                                       /* abstract */ true});
    }
    const auto new_methods = marianatrench::redex::create_methods(
        scope,
        /* class_name */ source.class_name,
        /* methods */ method_specifications,
        /* super */ super);
    methods.insert(methods.end(), new_methods.begin(), new_methods.end());
  }
  std::sort(
      methods.begin(),
      methods.end(),
      [](const DexMethod* left, const DexMethod* right) {
        return show(left) < show(right);
      });

  // Build context.
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

  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};

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

  // Run method analysis.
  const auto* positions = context.positions.get();
  auto results = Json::Value(Json::arrayValue);
  for (const auto* dex_method : methods) {
    auto* method = context.methods->get(dex_method);
    auto method_result = LocalFlowMethodAnalyzer::analyze(
        method, /* max_structural_depth */ 10, positions);
    if (method_result.has_value()) {
      results.append(test::sorted_json(method_result->to_json()));
    }
  }

  // Run class analysis.
  auto class_results = LocalFlowClassAnalyzer::analyze_classes(context);
  std::sort(
      class_results.begin(),
      class_results.end(),
      [](const LocalFlowClassResult& left, const LocalFlowClassResult& right) {
        return left.class_name < right.class_name;
      });
  for (const auto& class_result : class_results) {
    if (sexp_class_names.count(class_result.class_name) > 0) {
      results.append(test::sorted_json(class_result.to_json()));
    }
  }

  // Build output.
  auto value = Json::Value(Json::objectValue);
  auto metadata = Json::Value(Json::objectValue);
  metadata
      ["@"
       "generated"] = Json::Value(true);
  value["metadata"] = metadata;
  value["results"] = results;

  // Compare with expected output.
  auto expected_path = path.replace_extension(".expected");
  std::string expected_output;
  if (std::filesystem::exists(expected_path)) {
    filesystem::load_string_file(expected_path, expected_output);
  }
  auto actual_output = JsonWriter::to_styled_string(value);
  actual_output =
      boost::regex_replace(actual_output, boost::regex("\\s+\n"), "\n");
  actual_output += "\n";

  if (actual_output != expected_output) {
    filesystem::save_string_file(
        path.replace_extension(".expected.actual"), actual_output);
  }

  EXPECT_EQ(expected_output, actual_output);
}

MT_INSTANTIATE_TEST_SUITE_P(
    LocalFlowIntegration,
    IntegrationTest,
    testing::ValuesIn(sexp_paths()));
