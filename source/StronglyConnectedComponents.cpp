/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_map>
#include <unordered_set>

#include <mariana-trench/Methods.h>
#include <mariana-trench/StronglyConnectedComponents.h>

namespace marianatrench {

namespace {

/*
 * Tarjan's algorithm to compute strongly connected components.
 * https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
 */
class StronglyConnectedComponentsBuilder final {
 public:
  explicit StronglyConnectedComponentsBuilder(
      const Methods& methods,
      const Dependencies& dependencies,
      std::vector<std::vector<const Method*>>& components)
      : methods_(methods),
        dependencies_(dependencies),
        components_(components) {}

  void build() {
    for (const auto* method : methods_) {
      if (index_.count(method) == 0) {
        process(method);
      }
    }

    std::reverse(components_.begin(), components_.end());
  }

 private:
  void process(const Method* method) {
    index_[method] = current_index_;
    lowlink_[method] = current_index_;
    current_index_++;
    stack_.push_back(method);
    on_stack_.insert(method);

    for (const auto* caller : dependencies_.dependencies(method)) {
      if (index_.count(caller) == 0) {
        process(caller);
        lowlink_[method] = std::min(lowlink_[method], lowlink_[caller]);
      } else if (on_stack_.count(caller) > 0) {
        lowlink_[method] = std::min(lowlink_[method], index_[caller]);
      }
    }

    if (lowlink_[method] == index_[method]) {
      // Found a strongly connected component.
      std::vector<const Method*> component;

      const Method* other_method;
      do {
        other_method = stack_.back();
        stack_.pop_back();
        on_stack_.erase(other_method);
        component.push_back(other_method);
      } while (method != other_method);

      components_.push_back(std::move(component));
    }
  }

 private:
  const Methods& methods_;
  const Dependencies& dependencies_;
  std::vector<std::vector<const Method*>>& components_;

  // State of the algorithm.
  std::size_t current_index_ = 0;
  std::vector<const Method*> stack_;
  std::unordered_map<const Method*, std::size_t> index_;
  std::unordered_map<const Method*, std::size_t> lowlink_;
  std::unordered_set<const Method*> on_stack_;
};

} // namespace

StronglyConnectedComponents::StronglyConnectedComponents(
    const Methods& methods,
    const Dependencies& dependencies) {
  StronglyConnectedComponentsBuilder(methods, dependencies, components_)
      .build();
}

} // namespace marianatrench
