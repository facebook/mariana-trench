/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <IRAssembler.h>
#include <RedexContext.h>
#include <Show.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

DexMethod* empty_method_with_signature(const std::string& signature) {
  auto body = fmt::format(
      R"(
        (method {}
         (
          (return-void)
         )
        )
      )",
      signature);
  return assembler::method_from_string(body);
}

const auto models = test::parse_json(R"#(
  [
    {
      "method": "LSource;.source:()LData;",
      "generations": [{
        "kind": "TestSource",
        "port": "Return"
      }]
    },
    {
      "method": "LSource;.alternative_source:()LData;",
      "generations": [{
        "kind": "AlternativeSource",
        "port": "Return"
      }]
    },
    {
      "method": "LData;.<init>:(LData;LData;)V",
      "propagation": [
        {
          "input": "Argument(2)",
          "output": "Argument(0)"
        }
      ]
    },
    {
      "method": "LData;.propagation:(LData;)LData;",
      "propagation": [
        {
          "input": "Argument(1)",
          "output": "Return"
        }
      ]
    },
    {
      "method": "LData;.propagation_this:()LData;",
      "propagation": [
        {
          "input": "Argument(0)",
          "output": "Return"
        }
      ]
    },
    {
      "method": "LSink;.sink:(LData;)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(1)" }]
    },
    {
      "method": "LSink;.alternative_sink:(LData;)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(1)" }]
    },
    {
      "method": "LSink;.private_sink:(LData;)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(1)" }]
    },
    {
      "method": "LSink;.static_sink:(LData;)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(0)" }]
    },
    {
      "method": "LSink;.interface_sink:(LData;)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(1)" }]
    },
    {
      "method": "LSink;.sink_without_flow:(LData;)V",
      "sinks": [{ "kind": "SinkWithoutFlow", "port": "Argument(1)" }]
    },
    {
      "method": "LSink;.sink_with_two_kinds:(LData;)V",
      "sinks": [
        { "kind": "TestSink", "port": "Argument(1)" },
        { "kind": "AlternativeSink", "port": "Argument(1)" }
      ]
    },
    {
      "method": "LSink;.sink_in_second_parameter:(II)V",
      "sinks": [{ "kind": "TestSink", "port": "Argument(2)" }]
    }
  ]
)#");

const auto rules = test::parse_json(R"(
  [
    {
      "name": "TestRule",
      "code": 1,
      "description": "Test source flow",
      "sources": ["TestSource"],
      "sinks": ["TestSink"]
    },
    {
      "name": "AlternativeRule",
      "code": 2,
      "description": "Test source flow",
      "sources": ["AlternativeSource"],
      "sinks": ["AlternativeSink"]
    }
  ]
)");

struct TestSource {
  const std::string class_name;
  const std::vector<std::string> methods;
  const std::optional<std::string> super = std::nullopt;
};

class Parser {
 public:
  static std::vector<TestSource> parse(const std::string& source) {
    return Parser().parse_source(source);
  }

 private:
  void parse_method(
      const std::string& source,
      const std::optional<std::string>& super) {
    // Extract class name.
    auto offset = source.find("\"") + 1;
    auto class_name = source.substr(offset, source.find(".") - offset);

    class_name_to_super_[class_name] = super;

    // Preprocess.
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

    if (std::find(class_order_.begin(), class_order_.end(), class_name) ==
        class_order_.end()) {
      class_order_.push_back(class_name);
    }
    class_name_to_methods_[class_name].push_back(preprocessed_source);
  }

  std::vector<TestSource> parse_source(const std::string& source) {
    // TODO(T54979484) proper parsing.
    std::vector<std::string> lines;
    boost::split(lines, source, boost::is_any_of("\n"));

    std::vector<std::string> buffer;
    std::optional<std::string> super = std::nullopt;

    for (const auto& line : lines) {
      if (boost::starts_with(line, "(method ") ||
          boost::starts_with(line, "(super")) {
        if (!buffer.empty()) {
          parse_method(boost::join(buffer, "\n"), super);
          buffer.clear();
          super = std::nullopt;
        }
      }

      if (boost::starts_with(line, "(super ")) {
        auto offset = line.find("\"") + 1;
        super = line.substr(offset, line.find("\"", offset) - offset);
        continue;
      }

      buffer.push_back(line);
    }

    if (!buffer.empty()) {
      parse_method(boost::join(buffer, "\n"), super);
    }

    std::vector<TestSource> sources;
    for (const auto& class_name : class_order_) {
      sources.push_back(TestSource{
          class_name,
          /* methods */ class_name_to_methods_.at(class_name),
          /* super */ class_name_to_super_.at(class_name),
      });
    }
    return sources;
  }

  std::vector<std::string> class_order_;
  std::unordered_map<std::string, std::vector<std::string>>
      class_name_to_methods_;
  std::unordered_map<std::string, std::optional<std::string>>
      class_name_to_super_;
};

Scope stubs() {
  Scope stubs;

  stubs.push_back(assembler::class_with_methods(
      /* class_name */ "LData;",
      /* methods */
      {
          empty_method_with_signature(
              "(public) \"LData;.<init>:(LData;LData;)V\""),
          empty_method_with_signature(
              "(public) \"LData;.propagation:(LData;)LData;\""),
          empty_method_with_signature(
              "(public) \"LData;.propagation_this:()LData;\""),
      }));

  stubs.push_back(assembler::class_with_methods(
      /* class_name */ "LSource;",
      /* methods */
      {
          empty_method_with_signature("(public) \"LSource;.source:()LData;\""),
          empty_method_with_signature(
              "(public) \"LSource;.alternative_source:()LData;\""),
      }));

  stubs.push_back(assembler::class_with_methods(
      /* class_name */ "LSink;",
      /* methods */
      {
          empty_method_with_signature("(public) \"LSink;.sink:(LData;)V\""),
          empty_method_with_signature(
              "(public) \"LSink;.alternative_sink:(LData;)V\""),
          empty_method_with_signature(
              "(public) \"LSink;.sink_without_flow:(LData;)V\""),
          empty_method_with_signature(
              "(public) \"LSink;.sink_in_second_parameter:(II)V\""),
          empty_method_with_signature(
              "(public) \"LSink;.sink_with_two_kinds:(LData;)V\""),
          empty_method_with_signature(
              "(private) \"LSink;.private_sink:(LData;)V\""),
          empty_method_with_signature(
              "(public static) \"LSink;.static_sink:(LData;)V\""),
          empty_method_with_signature(
              "(public) \"LSink;.interface_sink:(LData;)V\""),
      }));

  stubs.push_back(assembler::class_with_methods(
      /* class_name */ "LExternal;",
      /* methods */
      {
          empty_method_with_signature(
              "(public static) \"LExternal;.external:(LData;)V\""),
      }));
  return stubs;
}

struct IntegrationTest : public test::ContextGuard,
                         public testing::TestWithParam<std::string> {};

std::vector<std::string> sexp_paths(
    boost::filesystem::path root =
        boost::filesystem::path(__FILE__).parent_path()) {
  std::vector<std::string> paths;

  for (const auto& directory :
       boost::filesystem::recursive_directory_iterator(root)) {
    auto path = directory.path();
    if (path.extension() != ".sexp") {
      continue;
    }
    paths.push_back(path.native());
  }

  return paths;
}

} // namespace

TEST_P(IntegrationTest, ReturnsExpectedModel) {
  boost::filesystem::path path = GetParam();
  LOG(1, "Test case {}", path);

  Scope scope(stubs());
  std::vector<const DexMethod*> methods;

  std::string source;
  boost::filesystem::load_string_file(path, source);
  auto sources = Parser::parse(source);

  for (const auto& source : sources) {
    auto* super = source.super
        ? DexType::make_type(DexString::make_string(*source.super))
        : nullptr;
    const auto new_methods = redex::create_methods(
        scope,
        /* class_name */ source.class_name,
        /* methods */ source.methods,
        /* super */ super);
    methods.insert(methods.end(), new_methods.begin(), new_methods.end());
  }
  std::sort(
      methods.begin(),
      methods.end(),
      [](const DexMethod* left, const DexMethod* right) {
        return show(left) < show(right);
      });

  Context context;

  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ true,
      /* skip_model_generation */ true,
      /* optimized_model_generation */ false);
  const auto& options = *context.options;

  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};

  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.positions = std::make_unique<Positions>(options, context.stores);
  context.types = std::make_unique<Types>(context.stores);
  context.class_properties = std::make_unique<ClassProperties>(
      options, context.stores, *context.features);
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
  context.rules = std::make_unique<Rules>(context, rules);

  Registry registry(context, context.stores);
  registry.join_with(Registry(context, models));

  Model external_method(
      context.methods->get("LExternal;.external:(LData;)V"),
      context,
      /* modes */ Model::Mode::SkipAnalysis |
          Model::Mode::AddViaObscureFeature | Model::Mode::TaintInTaintOut);
  registry.set(external_method);

  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);
  context.scheduler =
      std::make_unique<Scheduler>(*context.methods, *context.dependencies);

  Interprocedural::run_analysis(context, registry);

  auto value = Json::Value(Json::objectValue);
  auto metadata = Json::Value(Json::objectValue);
  metadata
      ["@"
       "generated"] = Json::Value(true);
  value["metadata"] = metadata;

  auto models = Json::Value(Json::arrayValue);
  for (std::size_t index = 0; index < methods.size(); index++) {
    auto model = registry.get(context.methods->get(methods[index]));
    models.append(test::sorted_json(model.to_json(context)));
  }
  value["models"] = models;

  auto expected_path = path.replace_extension(".expected");
  std::string expected_output;
  if (boost::filesystem::exists(expected_path)) {
    boost::filesystem::load_string_file(expected_path, expected_output);
  }
  auto models_output = JsonValidation::to_styled_string(value);

  if (models_output != expected_output) {
    boost::filesystem::save_string_file(
        path.replace_extension(".expected.actual"), models_output);
  }

  EXPECT_EQ(JsonValidation::to_styled_string(value), expected_output);
}

INSTANTIATE_TEST_CASE_P(
    ModelIntegration,
    IntegrationTest,
    testing::ValuesIn(sexp_paths()));
