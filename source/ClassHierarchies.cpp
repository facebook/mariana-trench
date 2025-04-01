/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

namespace {

class Graph {
 public:
  void add_edge(const DexType* child, const DexType* parent) {
    extends_.update(
        parent,
        [=](const DexType* /* parent */,
            std::unordered_set<const DexType*>& children,
            bool /* exists */) { children.insert(child); });
  }

  std::unordered_set<const DexType*> extends(const DexType* parent) const {
    std::unordered_set<const DexType*> result;
    std::vector<const DexType*> worklist;
    worklist.push_back(parent);

    while (!worklist.empty()) {
      const DexType* klass = worklist.back();
      worklist.pop_back();

      auto children = extends_.find(klass);
      if (children != extends_.end()) {
        for (const auto* child : children->second) {
          if (result.insert(child).second) {
            worklist.push_back(child);
          }
        }
      }
    }

    return result;
  }

 private:
  // Map a class to all classes that *directly* extend it.
  ConcurrentMap<const DexType*, std::unordered_set<const DexType*>> extends_;
};

} // namespace

ClassHierarchies::ClassHierarchies(
    const Options& options,
    AnalysisMode analysis_mode,
    const DexStoresVector& stores) {
  switch (analysis_mode) {
    case AnalysisMode::Normal:
      init_from_stores(stores);
      break;
    case AnalysisMode::CachedModels:
      init_from_stores(stores);
      add_cached_hierarchies(options);
      break;
    case AnalysisMode::Replay:
      // Do not recompute class hierarchies in replay mode.
      add_cached_hierarchies(options);
      break;
    default:
      mt_unreachable();
  }

  if (options.dump_class_hierarchies()) {
    auto class_hierarchies_path = options.class_hierarchies_output_path();
    LOG(1,
        "Writing class hierarchies to `{}`",
        class_hierarchies_path.native());
    JsonWriter::write_json_file(class_hierarchies_path, to_json());
  }
}

const std::unordered_set<const DexType*>& ClassHierarchies::extends(
    const DexType* klass) const {
  mt_assert(klass != type::java_lang_Object());

  const auto* extends = extends_.get(klass, /* default */ nullptr);
  if (extends != nullptr) {
    return *extends;
  } else {
    return empty_type_set_;
  }
}

Json::Value ClassHierarchies::to_json() const {
  auto extends_value = Json::Value(Json::objectValue);
  for (auto [klass, extends] : extends_) {
    auto ClassHierarchies_value = Json::Value(Json::arrayValue);
    for (const auto* extend : *extends) {
      ClassHierarchies_value.append(Json::Value(show(extend)));
    }
    extends_value[show(klass)] = ClassHierarchies_value;
  }
  auto value = Json::Value(Json::objectValue);
  value["extends"] = extends_value;
  return value;
}

ClassHierarchies::MapType ClassHierarchies::from_json(
    const Json::Value& value) {
  auto extends_value = JsonValidation::object(value, "extends");
  std::unordered_map<const DexType*, std::unordered_set<const DexType*>> result;

  // When reading from JSON, some types might not exist in the current APK
  // or loaded JARs (i.e. not defined in them). The full type information, such
  // as class hierarchy information, is not known, which is why they are being
  // loaded here. The DexType will be created if it does not exist. In general,
  // the analysis should rely on `ClassHierarchies` rather than `DexType` to
  // determine class hierarchy.
  for (const auto& type_name : extends_value.getMemberNames()) {
    const auto* dex_type = redex::get_or_make_type(type_name);
    std::unordered_set<const DexType*> extends;

    for (const auto& extends_json : extends_value[type_name]) {
      auto extends_type_name = JsonValidation::string(extends_json);
      const auto* extends_type = redex::get_or_make_type(extends_type_name);
      extends.insert(extends_type);
    }

    result[dex_type] = std::move(extends);
  }

  return result;
}

namespace {

ClassHierarchies::MapType read_class_hierarchies(
    const std::filesystem::path& class_hierarchies_file) {
  if (!std::filesystem::exists(class_hierarchies_file)) {
    throw std::runtime_error(
        "Class hierarchies file must exist when sharded input models are provided.");
  }

  LOG(1,
      "Reading class hierarchies from `{}`",
      class_hierarchies_file.native());

  auto class_hierarchies_json =
      JsonReader::parse_json_file(class_hierarchies_file);
  return ClassHierarchies::from_json(class_hierarchies_json);
}

} // namespace

void ClassHierarchies::add_cached_hierarchies(const Options& options) {
  auto class_hierarchies_input_path = options.class_hierarchies_input_path();
  mt_assert(class_hierarchies_input_path.has_value());
  auto cached_hierarchies =
      read_class_hierarchies(*class_hierarchies_input_path);
  for (const auto& [method, hierarchies] : cached_hierarchies) {
    // Merge with existing overrides. Modifying the underlying value is safe
    // since this is not happening concurrently.
    auto* existing = extends_.get_unsafe(method);
    if (existing == nullptr) {
      extends_.emplace(
          method,
          std::make_unique<std::unordered_set<const DexType*>>(hierarchies));
    } else {
      existing->insert(hierarchies.begin(), hierarchies.end());
    }
  }
}

void ClassHierarchies::init_from_stores(const DexStoresVector& stores) {
  Graph graph;

  // Compute the class hierarchy graph.
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&graph](const DexClass* klass) {
      const DexType* super = klass->get_super_class();
      if (super != type::java_lang_Object()) {
        graph.add_edge(/* child */ klass->get_type(), /* parent */ super);
      }
      for (DexType* interface : *klass->get_interfaces()) {
        graph.add_edge(/* child */ klass->get_type(), /* parent */ interface);
      }
    });
  }

  // Record the results.
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&graph, this](const DexClass* klass) {
      const auto* class_type = klass->get_type();
      auto extends = graph.extends(class_type);

      if (!extends.empty()) {
        extends_.emplace(
            klass->get_type(),
            std::make_unique<std::unordered_set<const DexType*>>(
                std::move(extends)));
      }
    });
  }
}

} // namespace marianatrench
