/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Log.h>
#include <mariana-trench/WideningPointsToResolver.h>

#include <sparta/WeakTopologicalOrdering.h>

namespace marianatrench {

namespace {

class DummyRootMemoryLocation : public RootMemoryLocation {
  std::string str() const override {
    return "DummyRootMemoryLocation";
  }
};

/**
 * This implements a widening operation on the PointsToEnvironment to remove any
 * cycles in the aliasing graph and applies the appropriate AliasingProperties
 * so that taint analysis can correctly read the taint trees from the widened
 * memory locations.
 *
 * --------------------
 * # Widening Strategy:
 * --------------------
 * 1. Apply the weak topological ordering (WTO) algorithm (i.e.
 * Bourdoncle's algorithm) on the given PointsToEnvironment.
 *   - A WTO (or Bourdoncle ordering) is a well-parenthesized ordering of the
 * vertices of a directed graph in which:
 *   - No two left parentheses are adjacent; thus, the head of every component
 * is in no subcomponent.
 *   - If u -> v is a feedback edge, then v is the head of some component
 * containing u.
 *
 * The original algorithm is described in the following paper:
 *   F. Bourdoncle. Efficient chaotic iteration strategies with widenings.
 *   In Formal Methods in Programming and Their Applications, pp 128-141.
 *
 * 2. PointsToEnvironment represents a rooted directed graph (with possible
 * cycles).
 *   - The set W of components’ heads of a WTO of the PointsToEnvironment
 * is a valid set of widening points. i.e. If we have a WTO of the
 * PointsToEnvironment, then we can use the set of heads of the ordering as
 * widening points.
 *
 * 3. The widened points-to environment is a PointsToEnvironment derived from
 * the original environment_ with the following additional properties:
 *    - is directed and acyclic.
 *    - where the components (if any) and the paths that lead to the formation
 * of the component are collapsed into a single node represented by the head.
 *    - the AliasingProperties are updated to reflect this widened state.
 *
 */
class WidenedPointsToBuilder final {
 public:
  explicit WidenedPointsToBuilder(
      const PointsToEnvironment& environment,
      RootMemoryLocationPointsToTreeMap& widened_resolved_environment,
      WidenedPointsToComponents& widened_components)
      : environment_(environment),
        widened_resolved_environment_(widened_resolved_environment),
        widened_components_(widened_components) {}

  void build() {
    // Create a dummy root memory location so that we can traverse all the
    // potentially disconnected components at once.
    DummyRootMemoryLocation dummy_entry_node{};

    auto successor_fn = [&environment = environment_,
                         dummy_entry_node_ptr =
                             &dummy_entry_node](RootMemoryLocation* vertex) {
      auto successors = boost::container::flat_set<RootMemoryLocation*>{};

      if (vertex == dummy_entry_node_ptr) {
        // Collect all the keys of the environment_ as successors for the dummy
        // node.
        for (const auto& [root_vertex, _points_to_tree] : environment) {
          successors.insert(root_vertex);
        }

        return successors;
      }

      environment.get(vertex).visit(
          [&successors](
              const Path& /* path */, const PointsToSet& points_to_set) {
            for (const auto& [successor, _properties] : points_to_set) {
              successors.insert(successor);
            }
          });

      return successors;
    };

    // 1. Create a WTO with root_vertex as the entry point into the graph.
    sparta::WeakTopologicalOrdering<RootMemoryLocation*> wto(
        &dummy_entry_node, successor_fn);

    // 2. Collect all the widened components from the WTO, subsuming the
    // nested components into the largest component.
    auto ordering = build_widened_components(wto);

    // 3. Build the widened points-to tree from the original environment_,
    // applying the widening strategy.
    auto widened_points_to_environment =
        build_widened_points_to_environment(wto);

    // 4. Build the mapping from each root memory location to the fully
    // resolved PointsToTree.
    build_widened_resolved_aliases(ordering, widened_points_to_environment);
  }

 private:
  /**
   * Build the widened components from the WTO.
   * This collects all the widened components from the WTO, subsuming the nested
   * components into the largest component.
   */
  std::vector<RootMemoryLocation*> build_widened_components(
      sparta::WeakTopologicalOrdering<RootMemoryLocation*>& wto) {
    LOG(5, "Building widened components...");

    std::vector<RootMemoryLocation*> ordering;
    for (const auto& node : wto) {
      if (node.head_node()->is<DummyRootMemoryLocation>()) {
        // Skip the dummy node.
        mt_assert(node.is_vertex());
        continue;
      }
      build_widened_components_impl(node, /* head */ nullptr, ordering);
    }

    LOG(5, "Built Widened components: {}", widened_components_);

    return ordering;
  }

  /**
   * Builds the widened components for each WTOComponent.
   * This preserves only the largest component by subsuming all the nested
   * components into it.
   */
  void build_widened_components_impl(
      const sparta::WtoComponent<RootMemoryLocation*>& node,
      RootMemoryLocation* head,
      std::vector<RootMemoryLocation*>& ordering) {
    auto* current_node = node.head_node();

    // TODO: Implement reverse iterator in sparta's WeakTopological ordering so
    // that we don't have to store this just for that.
    ordering.emplace_back(current_node);

    if (node.is_vertex() && head != nullptr) {
      widened_components_.add_component(head, current_node);
    } else if (node.is_scc()) {
      RootMemoryLocation* current_head;
      if (head == nullptr) {
        // Verify that we haven't processed this node already.
        mt_assert(widened_components_.get_head(node.head_node()) == nullptr);

        // Found the head of a new component.
        current_head = node.head_node();
        widened_components_.create_component(current_head);
      } else {
        // Found a nested component. Merge into the larger component.
        current_head = head;
        widened_components_.add_component(current_head, node.head_node());
      }

      for (const auto& inner_node : node) {
        build_widened_components_impl(inner_node, current_head, ordering);
      }
    }
  }

  /**
   * Builds the widened points-to environment from the WTO.
   */
  PointsToEnvironment build_widened_points_to_environment(
      sparta::WeakTopologicalOrdering<RootMemoryLocation*>& wto) {
    LOG(5, "Building Widened Points-to Environment...");

    PointsToEnvironment widened_environment{};

    for (const auto& node : wto) {
      if (node.head_node()->is<DummyRootMemoryLocation>()) {
        // Skip the dummy node.
        mt_assert(node.is_vertex());
        continue;
      }
      build_widened_points_to_environment_impl(
          widened_environment, node, /* head */ nullptr);
    }

    LOG(5, "Built Widened Points-to Environment: {}", widened_environment);

    return widened_environment;
  }

  /**
   * Builds the widened points-to environment for each WTOComponent.
   */
  PointsToTree build_widened_points_to_environment_impl(
      PointsToEnvironment& widened_environment,
      const sparta::WtoComponent<RootMemoryLocation*>& node,
      RootMemoryLocation* head) {
    auto* current_head = head ? head : node.head_node();
    PointsToTree widened_points_to_tree;

    if (node.is_vertex()) {
      widened_points_to_tree = build_widened_points_to_tree(
          current_head, environment_.get(node.head_node()));
    } else {
      mt_assert(node.is_scc());
      mt_assert(current_head == widened_components_.get_head(current_head));

      // This is a new component that has not been added to the widened
      // points-to environment. We join the points-to-trees of all the
      // members of this component to a single node represented by the head.
      widened_points_to_tree = build_widened_points_to_tree(
          current_head, environment_.get(node.head_node()));

      for (const auto& inner_node : node) {
        widened_points_to_tree.join_with(
            build_widened_points_to_environment_impl(
                widened_environment, inner_node, current_head));
      }
    }

    // If we are visiting the actual head of a component, update the
    // widened-points-to-environment
    if (current_head == node.head_node()) {
      // Update the widened points-to environment.
      widened_environment.update(
          current_head, [widened_points_to_tree](const PointsToTree& existing) {
            auto copy = existing;
            copy.join_with(widened_points_to_tree);
            return copy;
          });
    }

    return widened_points_to_tree;
  }

  /**
   * Processes the given points_to_tree for the head memory location to create a
   * widened points-to tree. This:
   * - filters out the memory locations from the points-to set that are present
   * in the component containing the head (if any).
   * - replaces the memory locations in the points-to set with the corresponding
   * head (if any).
   * - updates the aliasing properties of the the points-set containing a
   * widened memory location with collapse depth 0.
   */
  PointsToTree build_widened_points_to_tree(
      RootMemoryLocation* head,
      const PointsToTree& points_to_tree) {
    PointsToTree result{};

    const auto* current_component = widened_components_.get_component(head);

    points_to_tree.visit([this, &result, &current_component](
                             const Path& path,
                             const PointsToSet& points_to_set) {
      PointsToSet new_points_to_set{};
      for (const auto& [points_to, properties] : points_to_set) {
        if (current_component && current_component->contains(points_to)) {
          // points_to is a part of the current component and hence is
          // merged/collapsed into (i.e. represented by) the head of the
          // component. Note that the points-to environment does not store a
          // points-to set (and hence aliasing properties) for the root memory
          // location of the tree and hence the aliasing properties is ignored
          // here as well.
          continue;
        }

        if (auto* component_head =
                this->widened_components_.get_head(points_to)) {
          auto new_properties = properties;
          new_properties.set_always_collapse();
          new_points_to_set.update_aliasing_properties(
              component_head, new_properties);
        } else {
          new_points_to_set.update_aliasing_properties(points_to, properties);
        }
      }

      result.write(path, std::move(new_points_to_set), UpdateKind::Weak);
    });

    return result;
  }

  /**
   * Traverses the widened points-to environment in weak topological ordering
   * and updates the RootMemoryLocationToPointsToTree for each root with the
   * fully resolved PointsToTree as value which includes the self-resolution at
   * the root.
   */
  void build_widened_resolved_aliases(
      const std::vector<RootMemoryLocation*>& ordering,
      const PointsToEnvironment& widened_environment) {
    // We need to iterate in reverse topological order so that we can use the
    // widened_resolved_environment_ itself for memoization.
    for (auto node = ordering.rbegin(); node != ordering.rend(); node++) {
      bool is_component = true;
      auto* head = widened_components_.get_head(*node);
      if (head == nullptr) {
        // Not a part of any component.
        head = *node;
        is_component = false;
      }

      if (widened_resolved_environment_.find(head) !=
          widened_resolved_environment_.end()) {
        // Already resolved.
        continue;
      }

      auto points_to_tree = widened_environment.get(head);
      PointsToTree resolved_points_to_tree{};
      points_to_tree.visit(
          [this, &resolved_points_to_tree](
              const Path& inner_path, const PointsToSet& points_to_set) {
            // The root element of the PointsToTree of a root memory location is
            // always empty.
            mt_assert(!inner_path.empty() || points_to_set.is_bottom());

            for (const auto& [points_to, properties] : points_to_set) {
              // We are resolving the widened points-to environment in weak
              // topological ordering hence we always expect to find the
              // points-to memory location in the widened resolved environment.
              auto resolved_points_to =
                  this->widened_resolved_environment_.at(points_to);

              // Update the root of the widened resolved point-to tree
              // with the current aliasing properties.
              resolved_points_to_tree.write(
                  inner_path,
                  resolved_points_to.with_aliasing_properties(properties),
                  UpdateKind::Weak);
            }
          });

      // Add self resolution.
      // When reading from a widened memory location (i.e. component), we
      // always need to set the collapse depth to 0. This is updated in the
      // AliasingProperties correctly in the widened_environment when another
      // memory location aliases/points-to a widened memory location. However,
      // we do not store the AliasingProperties for a memory location when it
      // is the "root"/key of the PointsToEnvironment and since
      // PointsToEnvironment::resolve_aliases() currently does not have access
      // to this information, we update it after the fact here.
      auto self_resolution = PointsToSet{
          head,
          is_component ? AliasingProperties::always_collapse()
                       : AliasingProperties::empty()};

      resolved_points_to_tree.write(
          Path{}, std::move(self_resolution), UpdateKind::Weak);

      widened_resolved_environment_.emplace(
          head, std::move(resolved_points_to_tree));
    }
  }

 private:
  // Original environment to widen
  const PointsToEnvironment& environment_;

  // Output params
  RootMemoryLocationPointsToTreeMap& widened_resolved_environment_;
  WidenedPointsToComponents& widened_components_;
};

} // namespace

void WidenedPointsToComponents::create_component(RootMemoryLocation* head) {
  LOG(5, "WidenedPointsToComponents::create_component({})", show(head));
  components_.emplace(head, Component{head});
}

void WidenedPointsToComponents::add_component(
    RootMemoryLocation* head,
    RootMemoryLocation* member) {
  LOG(5,
      "WidenedPointsToComponents::add_component({}, {})",
      show(head),
      show(member));

  auto& strongly_connected_component = components_.at(head);
  strongly_connected_component.insert(member);
}

RootMemoryLocation* MT_NULLABLE
WidenedPointsToComponents::get_head(RootMemoryLocation* memory_location) const {
  if (components_.contains(memory_location)) {
    return memory_location;
  }

  for (const auto& [head, component] : components_) {
    if (component.contains(memory_location)) {
      return head;
    }
  }

  return nullptr;
}

const WidenedPointsToComponents::Component* MT_NULLABLE
WidenedPointsToComponents::get_component(
    RootMemoryLocation* memory_location) const {
  auto result = components_.find(memory_location);
  if (result != components_.end()) {
    // vertex is the head of the component.
    return &result->second;
  }

  for (const auto& [_head, component] : components_) {
    if (component.count(memory_location) > 0) {
      return &component;
    }
  }

  return nullptr;
}

WideningPointsToResolver::WideningPointsToResolver(
    const PointsToEnvironment& points_to_environment) {
  WidenedPointsToBuilder builder(
      points_to_environment, resolved_aliases_, widened_components_);
  builder.build();

  LOG(5, "Built WideningPointsToResolver: {}", *this);
}

PointsToTree WideningPointsToResolver::resolved_aliases(
    RootMemoryLocation* root_memory_location) const {
  if (auto* head = widened_components_.get_head(root_memory_location)) {
    // Check for widened component's head
    root_memory_location = head;
  }

  auto result = resolved_aliases_.find(root_memory_location);
  if (result == resolved_aliases_.end()) {
    return PointsToTree{PointsToSet{root_memory_location}};
  }

  return result->second;
}

std::ostream& operator<<(
    std::ostream& out,
    const WidenedPointsToComponents& components) {
  out << "WidenedPointsToComponents(";
  for (const auto& [head, component] : components.components_) {
    out << "\n  " << show(head) << " -> {";

    for (const auto& c : component) {
      out << show(c) << ", ";
    }

    out << "}";
  }

  return out << ")";
}

std::ostream& operator<<(
    std::ostream& out,
    const WideningPointsToResolver& widening_resolver) {
  out << "WideningPointsToResolver(";
  for (const auto& [root_memory_location, points_to_tree] :
       widening_resolver.resolved_aliases_) {
    out << "\n  ";

    if (auto* head = widening_resolver.widened_components_.get_head(
            root_memory_location)) {
      out << "Widened(head=" << show(head) << ", members={";
      auto* component =
          widening_resolver.widened_components_.get_component(head);
      mt_assert(component != nullptr);
      for (const auto* member : *component) {
        if (head == member) {
          continue;
        }
        out << show(member) << ",";
      }
      out << "})";
    } else {
      out << show(root_memory_location);
    }

    out << " -> " << points_to_tree;
  }

  return out << ")";
}

} // namespace marianatrench
