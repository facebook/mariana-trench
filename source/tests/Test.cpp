/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>
#include <tuple>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <RedexContext.h>

#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/SanitizersOptions.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {
namespace test {

Test::Test() : global_redex_context_(/* allow_class_duplicates */ false){};

ContextGuard::ContextGuard()
    : global_redex_context_(/* allow_class_duplicates */ false){};

Context make_empty_context() {
  Context context;
  context.methods = std::make_unique<Methods>();
  context.positions = std::make_unique<Positions>();
  return context;
}

Context make_context(const DexStore& store) {
  Context context;
  auto shims_path =
      boost::filesystem::path(__FILE__).parent_path() / "shims.json";
  context.options = std::make_unique<Options>(
      /* models_paths */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_paths */ std::vector<std::string>{},
      /* lifecycles_paths */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{shims_path.native()},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generators_search_path */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.fields = std::make_unique<Fields>(context.stores);
  context.positions =
      std::make_unique<Positions>(*context.options, context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options, *context.methods, context.stores);
  MethodMappings method_mappings{*context.methods};
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(context);
  auto shims =
      ShimGeneration::run(context, intent_routing_analyzer, method_mappings);
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.methods,
      *context.fields,
      *context.types,
      *context.class_hierarchies,
      *context.overrides,
      *context.feature_factory,
      shims,
      method_mappings);
  auto registry = Registry(context);
  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);
  context.class_properties = std::make_unique<ClassProperties>(
      *context.options,
      context.stores,
      *context.feature_factory,
      *context.dependencies);
  context.rules = std::make_unique<Rules>(context);
  context.used_kinds = std::make_unique<UsedKinds>(*context.transforms_factory);
  context.scheduler =
      std::make_unique<Scheduler>(*context.methods, *context.dependencies);
  return context;
}

std::unique_ptr<Options> make_default_options() {
  return std::make_unique<Options>(
      /* models_paths */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_paths */ std::vector<std::string>{},
      /* lifecycles_paths */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generators_search_path */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
}

Frame make_taint_frame(const Kind* kind, const FrameProperties& properties) {
  // Local positions/features should not be specified when making a Frame
  // because it is not stored in the Frame.
  mt_assert(properties.local_positions == LocalPositionSet{});
  mt_assert(properties.locally_inferred_features == FeatureMayAlwaysSet{});
  return Frame(
      kind,
      properties.callee_port,
      properties.callee,
      properties.field_callee,
      properties.call_position,
      properties.class_interval_context,
      properties.distance,
      properties.origins,
      properties.field_origins,
      properties.inferred_features,
      properties.user_features,
      properties.via_type_of_ports,
      properties.via_value_of_ports,
      properties.canonical_names,
      properties.call_info,
      properties.output_paths,
      properties.extra_traces);
}

TaintConfig make_taint_config(
    const Kind* kind,
    const FrameProperties& properties) {
  return TaintConfig(
      kind,
      properties.callee_port,
      properties.callee,
      properties.call_info,
      properties.field_callee,
      properties.call_position,
      properties.class_interval_context,
      properties.distance,
      properties.origins,
      properties.field_origins,
      properties.inferred_features,
      properties.user_features,
      properties.via_type_of_ports,
      properties.via_value_of_ports,
      properties.canonical_names,
      properties.output_paths,
      properties.local_positions,
      properties.locally_inferred_features,
      properties.extra_traces);
}

TaintConfig make_leaf_taint_config(const Kind* kind) {
  return make_leaf_taint_config(
      kind,
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ FeatureSet::bottom(),
      /* origins */ {});
}

TaintConfig make_leaf_taint_config(
    const Kind* kind,
    FeatureMayAlwaysSet inferred_features,
    FeatureMayAlwaysSet locally_inferred_features,
    FeatureSet user_features,
    MethodSet origins) {
  return TaintConfig(
      kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_info */ CallInfo::declaration(),
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* type_contexts */ CallClassIntervalContext(),
      /* distance */ 0,
      origins,
      /* field origins */ {},
      inferred_features,
      user_features,
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      locally_inferred_features,
      /* extra_traces */ {});
}

TaintConfig make_crtex_leaf_taint_config(
    const Kind* kind,
    AccessPath callee_port,
    CanonicalNameSetAbstractDomain canonical_names) {
  mt_assert(callee_port.root().is_anchor() || callee_port.root().is_producer());
  return TaintConfig(
      kind,
      /* callee_port */ callee_port,
      /* callee */ nullptr,
      /* call_info */ CallInfo::origin(),
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* type_contexts */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      canonical_names,
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

TaintConfig make_propagation_taint_config(const PropagationKind* kind) {
  return make_propagation_taint_config(
      kind,
      /* output_paths */ PathTreeDomain{{Path{}, CollapseDepth::zero()}},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ FeatureSet::bottom());
}

TaintConfig make_propagation_taint_config(
    const PropagationKind* kind,
    PathTreeDomain output_paths,
    FeatureMayAlwaysSet inferred_features,
    FeatureMayAlwaysSet locally_inferred_features,
    FeatureSet user_features) {
  return TaintConfig(
      kind,
      /* callee_port */ AccessPath(kind->root()),
      /* callee */ nullptr,
      /* call_info */ CallInfo::propagation(),
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ inferred_features,
      /* user_features */ user_features,
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      output_paths,
      /* local_positions */ {},
      /* locally_inferred_features */ locally_inferred_features,
      /* extra_traces */ {});
}

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
boost::filesystem::path find_repository_root() {
  auto path = boost::filesystem::current_path();
  while (!path.empty() && !boost::filesystem::is_directory(path / "source")) {
    path = path.parent_path();
  }
  if (path.empty()) {
    throw std::logic_error(
        "Could not find the root directory of the repository");
  }
  return path;
}
#endif

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
    directories.push_back(sub_directory.path().filename().string());
  }

  return directories;
}

namespace {

Json::Value json_model_full_name(const Json::Value& object) {
  if (!object.isObject()) {
    return Json::Value(Json::nullValue);
  }

  if (object.isMember("method")) {
    return object["method"];
  }

  if (object.isMember("field")) {
    return object["field"];
  }

  return Json::Value(Json::nullValue);
}

Json::Value json_model_short_name(const Json::Value& object) {
  Json::Value full_name = json_model_full_name(object);

  // Parameter type overrides.
  if (full_name.isObject() && full_name.isMember("name")) {
    return full_name["name"];
  }

  return full_name;
}

// Custom operator< on json values which sorts models per method or field name.
// This makes it easier to compare changes in models.
bool stable_json_compare(const Json::Value& left, const Json::Value& right) {
  return std::make_tuple(
             json_model_short_name(left), json_model_full_name(left), left) <
      std::make_tuple(
             json_model_short_name(right), json_model_full_name(right), right);
}

} // namespace

std::string normalize_json_lines(const std::string& input) {
  std::vector<std::string> lines;
  boost::split(lines, input, boost::is_any_of("\n"));

  std::vector<std::string> normalized_lines;
  std::vector<Json::Value> jsons;
  std::optional<std::string> buffer = std::nullopt;

  for (std::size_t index = 0; index < lines.size(); index++) {
    const auto& line = lines[index];
    if (line.empty()) {
      continue;
    }
    if (boost::starts_with(line, "//")) {
      normalized_lines.push_back(line);
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

      jsons.push_back(sorted_json(test::parse_json(*buffer)));
      buffer = std::nullopt;
    } else {
      *buffer += "\n" + line;
      continue;
    }
  }
  mt_assert(buffer == std::nullopt);

  std::sort(jsons.begin(), jsons.end(), stable_json_compare);

  for (const auto& json : jsons) {
    auto normalized = JsonValidation::to_styled_string(json);
    boost::trim(normalized);
    normalized_lines.push_back(normalized);
  }

  return boost::join(normalized_lines, "\n") + "\n";
}

} // namespace test
} // namespace marianatrench
