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
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Filesystem.h>
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
          (new-instance "Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (return-object v0)
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
      for (const auto& source : sources) {
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
    if (abstract) {
      class_name_to_abstract_methods_[class_name].push_back(
          preprocessed_source);
      class_name_to_methods_[class_name] = std::vector<std::string>();
    } else {
      class_name_to_methods_[class_name].push_back(preprocessed_source);
      class_name_to_abstract_methods_[class_name] = std::vector<std::string>();
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
      if (boost::starts_with(line, "(method ") ||
          boost::starts_with(line, "(super ") ||
          boost::starts_with(line, "(abstract method ")) {
        if (!buffer.empty()) {
          parse_method(boost::join(buffer, "\n"), super, abstract);
          buffer.clear();
          super = std::nullopt;
          abstract = false;
        }
      }

      if (boost::starts_with(line, "(super ")) {
        auto offset = line.find("\"") + 1;
        super = line.substr(offset, line.find("\"", offset) - offset);
        continue;
      }

      if (boost::starts_with(line, "(abstract method ")) {
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
      parsed_sources.push_back(TestSource{
          class_name,
          /* methods */ class_name_to_methods_.at(class_name),
          /* abstract methods */ class_name_to_abstract_methods_.at(class_name),
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

Scope stubs() {
  Scope stubs;

  auto* ldata_class = assembler::class_with_methods(
      /* class_name */ "LData;",
      /* methods */
      {
          empty_method_with_signature(
              "(public) \"LData;.<init>:(LData;LData;)V\""),
          empty_method_with_signature(
              "(public) \"LData;.propagation:(LData;)LData;\""),
          empty_method_with_signature(
              "(public) \"LData;.propagation_this:()LData;\""),
      });
  stubs.push_back(ldata_class);
  // Add fields that are used in test cases
  ldata_class->add_field(DexField::make_field(
                             ldata_class->get_type(),
                             DexString::make_string("field"),
                             ldata_class->get_type())
                             ->make_concrete(DexAccessFlags::ACC_PUBLIC));
  ldata_class->add_field(DexField::make_field(
                             ldata_class->get_type(),
                             DexString::make_string("other_field"),
                             ldata_class->get_type())
                             ->make_concrete(DexAccessFlags::ACC_PUBLIC));

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

// Since the class Flow is created and populated with its methods during
// parsing of the test cases, we can't create the fields for this class
// beforehand within the stubs declaration above, as we can't add methods
// to a fully instantiated DexClass.
void add_flow_class_fields(DexStore& store) {
  auto dex_classes = store.get_dexen();
  for (const auto& classes : dex_classes) {
    for (auto* klass : classes) {
      if (klass->get_name()->str() == "LFlow;") {
        for (auto name : {"successor", "left", "right"}) {
          klass->add_field(DexField::make_field(
                               klass->get_type(),
                               DexString::make_string(name),
                               klass->get_type())
                               ->make_concrete(DexAccessFlags::ACC_PUBLIC));
        }
        const auto* data_type = DexType::get_type("LData;");
        mt_assert(data_type != nullptr);
        klass->add_field(
            DexField::make_field(
                klass->get_type(), DexString::make_string("field"), data_type)
                ->make_concrete(DexAccessFlags::ACC_PUBLIC));

        break;
      }
    }
  }
}

} // namespace

TEST_P(IntegrationTest, ReturnsExpectedModel) {
  std::filesystem::path name = GetParam();
  LOG(1, "Test case `{}`", name);
  std::filesystem::path path = root_directory() / name;

  Scope scope(stubs());
  std::vector<const DexMethod*> methods;

  std::string unparsed_source;
  filesystem::load_string_file(path, unparsed_source);
  auto sources = Parser::parse(unparsed_source);
  auto sorted_sources = Parser::sort_by_hierarchy(sources);

  // Create redex classes.
  for (const auto& source : sorted_sources) {
    auto* super = source.super ? DexType::get_type(*source.super) : nullptr;
    std::vector<redex::DexMethodSpecification> method_specifications;
    for (const auto& method : source.methods) {
      method_specifications.push_back(redex::DexMethodSpecification{method});
    }
    for (const auto& method : source.abstract_methods) {
      method_specifications.push_back(redex::DexMethodSpecification{
          /* body */ method, /* abstract */ true});
    }
    const auto new_methods = redex::create_methods(
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

  Context context;

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
      /* skip_analysis */ false,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* emit_all_via_cast_features */ false,
      /* remove_unreachable_code */ false);
  const auto& options = *context.options;

  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  add_flow_class_fields(store);

  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  MethodMappings method_mappings{*context.methods};
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(context);
  context.fields = std::make_unique<Fields>(context.stores);
  context.positions = std::make_unique<Positions>(options, context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(options, context.stores);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.field_cache =
      std::make_unique<FieldCache>(*context.class_hierarchies, context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options, *context.methods, context.stores);
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.types,
      *context.class_hierarchies,
      LifecycleMethods{},
      Shims{/* global_shims_size */ 0, intent_routing_analyzer},
      *context.feature_factory,
      *context.methods,
      *context.fields,
      *context.overrides,
      method_mappings);
  context.rules = std::make_unique<Rules>(context, rules);
  context.used_kinds = std::make_unique<UsedKinds>(
      UsedKinds::from_rules(*context.rules, *context.transforms_factory));
  context.class_intervals =
      std::make_unique<ClassIntervals>(*context.options, context.stores);

  Registry registry(context);
  registry.join_with(Registry(
      context,
      /* models_value */ models,
      /* field_models_value */ test::parse_json(R"([])"),
      /* literal_models_value */ test::parse_json(R"([])")));

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
  context.class_properties = std::make_unique<ClassProperties>(
      options, context.stores, *context.feature_factory, *context.dependencies);
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
  if (std::filesystem::exists(expected_path)) {
    filesystem::load_string_file(expected_path, expected_output);
  }
  auto models_output = JsonValidation::to_styled_string(value);
  models_output =
      boost::regex_replace(models_output, boost::regex("\\s+\n"), "\n");
  models_output += "\n";

  if (models_output != expected_output) {
    filesystem::save_string_file(
        path.replace_extension(".expected.actual"), models_output);
  }

  EXPECT_EQ(expected_output, models_output);
}

MT_INSTANTIATE_TEST_SUITE_P(
    ModelIntegration,
    IntegrationTest,
    testing::ValuesIn(sexp_paths()));
