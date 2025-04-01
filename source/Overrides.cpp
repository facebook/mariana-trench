/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <vector>

#include <MethodOverrideGraph.h>
#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

Overrides::Overrides(
    const Options& options,
    AnalysisMode analysis_mode,
    Methods& method_factory,
    const DexStoresVector& stores) {
  switch (analysis_mode) {
    case AnalysisMode::Normal:
      init_from_stores(stores, method_factory);
      break;
    case AnalysisMode::CachedModels:
      init_from_stores(stores, method_factory);
      add_cached_overrides(options, method_factory);
      break;
    case AnalysisMode::Replay:
      // Do not recompute overrides in replay mode.
      add_cached_overrides(options, method_factory);
      break;
    default:
      mt_unreachable();
  }

  if (options.dump_overrides()) {
    auto overrides_path = options.overrides_output_path();
    LOG(1, "Writing override graph to `{}`", overrides_path.native());
    JsonWriter::write_json_file(overrides_path, to_json());
  }
}

const std::unordered_set<const Method*>& Overrides::get(
    const Method* method) const {
  auto* overrides = overrides_.get(method, /* default */ nullptr);
  if (overrides != nullptr) {
    return *overrides;
  } else {
    return empty_method_set_;
  }
}

void Overrides::set(
    const Method* method,
    std::unordered_set<const Method*> overrides) {
  if (overrides.empty()) {
    mt_assert(overrides_.get(method, /* default */ nullptr) == nullptr);
    return;
  }

  overrides_.emplace(
      method,
      std::make_unique<std::unordered_set<const Method*>>(
          std::move(overrides)));
}

const std::unordered_set<const Method*>& Overrides::empty_method_set() const {
  return empty_method_set_;
}

bool Overrides::has_obscure_override_for(const Method* method) const {
  const auto& overrides = get(method);
  for (const auto* override : overrides) {
    if (!override->get_code()) {
      return true;
    }
  }
  return false;
}

Json::Value Overrides::to_json() const {
  auto value = Json::Value(Json::objectValue);
  for (auto [method, overrides] : overrides_) {
    auto overrides_value = Json::Value(Json::arrayValue);
    for (const auto* override : *overrides) {
      overrides_value.append(Json::Value(show(override)));
    }
    value[method->show()] = overrides_value;
  }
  return value;
}

Overrides::MapType Overrides::from_json(
    const Json::Value& value,
    Methods& methods) {
  JsonValidation::validate_object(value);
  std::unordered_map<const Method*, std::unordered_set<const Method*>> result;

  // When reading from JSON, some methods might not exist in the current APK
  // or loaded JARs (i.e. not defined in them). They could however, still be
  // referenced at a call-site, and the reference may not be direct, such as
  // a call to a base class that contains an override. For simplicity and
  // completeness, any non-existent method will be created here.
  for (const auto& method_name : value.getMemberNames()) {
    const auto* dex_method = redex::get_or_make_method(method_name);
    std::unordered_set<const Method*> overrides;

    for (const auto& override_json : value[method_name]) {
      auto override_method_name = JsonValidation::string(override_json);
      const auto* override_dex_method =
          redex::get_or_make_method(override_method_name);
      overrides.insert(methods.create(override_dex_method));
    }

    const auto* method = methods.create(dex_method);
    result[method] = overrides;
  }

  return result;
}

namespace {

Overrides::MapType read_overrides(
    const std::filesystem::path& overrides_file,
    Methods& methods) {
  if (!std::filesystem::exists(overrides_file)) {
    throw std::runtime_error(
        "Overrides file must exist when sharded input models are provided.");
  }

  LOG(1, "Reading overrides from `{}`", overrides_file.native());
  auto overrides_json = JsonReader::parse_json_file(overrides_file);
  return Overrides::from_json(overrides_json, methods);
}

} // namespace

void Overrides::add_cached_overrides(const Options& options, Methods& methods) {
  auto overrides_input_path = options.overrides_input_path();
  mt_assert(overrides_input_path.has_value());
  auto cached_overrides = read_overrides(*overrides_input_path, methods);
  for (const auto& [method, overrides] : cached_overrides) {
    // Merge with existing overrides. Modifying the underlying value is safe
    // since this is not happening concurrently.
    auto* existing_overrides = overrides_.get_unsafe(method);
    if (existing_overrides == nullptr) {
      set(method, overrides);
    } else {
      existing_overrides->insert(overrides.begin(), overrides.end());
    }
  }
}

void Overrides::init_from_stores(
    const DexStoresVector& stores,
    Methods& method_factory) {
  std::vector<std::unique_ptr<const method_override_graph::Graph>>
      method_override_graphs;
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    method_override_graphs.push_back(method_override_graph::build_graph(scope));
  }

  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(
        scope,
        [&method_factory, &method_override_graphs, this](
            const DexMethod* dex_method) {
          std::unordered_set<const Method*> overrides;
          const auto* method = method_factory.get(dex_method);

          for (const auto& graph : method_override_graphs) {
            auto overrides_in_scope =
                method_override_graph::get_overriding_methods(
                    *graph, dex_method, /* include_interfaces */ true);
            for (const auto* override : overrides_in_scope) {
              overrides.insert(method_factory.get(override));
            }
          }
          set(method, std::move(overrides));
        });
  }
}

} // namespace marianatrench
