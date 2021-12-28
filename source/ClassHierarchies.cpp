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
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

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
    const DexStoresVector& stores) {
  Graph graph;

  // Compute the class hierarchy graph.
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
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
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      auto extends = graph.extends(klass->get_type());

      if (!extends.empty()) {
        extends_.emplace(
            klass->get_type(),
            std::make_unique<std::unordered_set<const DexType*>>(
                std::move(extends)));
      }
    });
  }

  if (options.dump_class_hierarchies()) {
    LOG(1, "Writing class hierarchies to `class_hierarchies.json`");
    JsonValidation::write_json_file("class_hierarchies.json", to_json());
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

} // namespace marianatrench
