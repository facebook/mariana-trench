/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include <json/json.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class Heuristics final {
 public:
  explicit Heuristics();

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Heuristics)

  static void init_from_file(const std::filesystem::path& heuristics_path);

  static const Heuristics& singleton();

  /**
   * When a method has a set of overrides greater than this threshold, we do not
   * join all overrides at call sites.
   */
  std::uint32_t join_override_threshold() const {
    return join_override_threshold_;
  }

  /**
   * When an android/java/google method has a set of overrides and greater than
   * this threshold, we do not join all overrides at call sites.
   */
  std::uint32_t android_join_override_threshold() const {
    return android_join_override_threshold_;
  }

  /**
   * When a method which has a set of overrides greater than this threshold that
   * is not marked with `NoJoinVirtualOverrides` is called at least once, we
   * print a warning.
   */
  const std::optional<std::uint32_t> warn_override_threshold() const {
    return warn_override_threshold_;
  }

  /**
   * Maximum height of a taint (source or sink) tree after widening.
   *
   * When reaching the maximum, we collapse the leaves to reduce the height.
   */
  std::uint32_t source_sink_tree_widening_height() const {
    return source_sink_tree_widening_height_;
  }

  /**
   * Maximum size of the port of a generation.
   *
   * This is the maximum depth of the generation taint tree. We truncate the
   * ports upto this threshold when updating the tree. This is equivalent to
   * collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t generation_max_port_size() const {
    return generation_max_port_size_;
  }

  /**
   * Maximum number of leaves in the tree of output paths of generations.
   *
   * This is the maximum width of the generation taint tree. When the number of
   * leaves exceeds this threshold, we compute the depth at which the tree
   * exceeds the threshold and collapse all the subtrees into the nodes at this
   * level.
   */
  std::uint32_t generation_max_output_path_leaves() const {
    return generation_max_output_path_leaves_;
  }

  /**
   * Maximum size of the port of a parameter source.
   *
   * This is the maximum depth of the parameter source taint tree. We truncate
   * the ports upto this threshold when updating the tree. This is equivalent
   * to collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t parameter_source_max_port_size() const {
    return parameter_source_max_port_size_;
  }

  /**
   * Maximum number of leaves in the tree of output paths of parameter sources.
   *
   * This is the maximum width of the parameter source taint tree. When the
   * number of leaves exceeds this threshold, we compute the depth at which the
   * tree exceeds the threshold and collapse all the subtrees into the nodes at
   * this level.
   */
  std::uint32_t parameter_source_max_output_path_leaves() const {
    return parameter_source_max_output_path_leaves_;
  }

  /**
   * Maximum size of the port of a sink.
   *
   * This is the maximum depth of the sink taint tree. We truncate the
   * ports upto this threshold when updating the tree. This is equivalent to
   * collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t sink_max_port_size() const {
    return sink_max_port_size_;
  }

  /**
   * Maximum number of leaves in the tree of input paths of sinks.
   *
   * This is the maximum width of the sink taint tree. When the number of
   * leaves exceeds this threshold, we compute the depth at which the tree
   * exceeds the threshold and collapse all the subtrees into the nodes at this
   * level.
   */
  std::uint32_t sink_max_input_path_leaves() const {
    return sink_max_input_path_leaves_;
  }

  /**
   * Maximum size of the port of a call effect source.
   *
   * This is the maximum depth of the call effect source taint tree. We truncate
   * the ports upto this threshold when updating the tree. This is equivalent
   * to collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t call_effect_source_max_port_size() const {
    return call_effect_source_max_port_size_;
  }

  /**
   * Maximum number of leaves in the tree of output paths of call effect
   * sources.
   *
   * This is the maximum width of the call effect source taint tree. When the
   * number of leaves exceeds this threshold, we compute the depth at which the
   * tree exceeds the threshold and collapse all the subtrees into the nodes at
   * this level.
   */
  std::uint32_t call_effect_source_max_output_path_leaves() const {
    return call_effect_source_max_output_path_leaves_;
  }

  /**
   * Maximum size of the port of a call effect sink.
   *
   * This is the maximum depth of the call effect sink taint tree. We truncate
   * the ports upto this threshold when updating the tree. This is equivalent
   * to collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t call_effect_sink_max_port_size() const {
    return call_effect_sink_max_port_size_;
  }

  /**
   * Maximum number of leaves in the tree of input paths of call effect sinks.
   *
   * This is the maximum width of the call effect sink taint tree. When the
   * number of leaves exceeds this threshold, we compute the depth at which the
   * tree exceeds the threshold and collapse all the subtrees into the nodes at
   * this level.
   */
  std::uint32_t call_effect_sink_max_input_path_leaves() const {
    return call_effect_sink_max_input_path_leaves_;
  }

  /**
   * Maximum number of global iterations before we abort the analysis.
   */
  std::uint32_t max_number_iterations() const {
    return max_number_iterations_;
  }

  /**
   * Maximum number of local positions per frame.
   *
   * This parameter cannot be set a runtime, as it used at compile time.
   */
  constexpr static std::uint32_t kMaxNumberLocalPositions = 20;

  /**
   * Maximum depth of dependency graph traversal to find class properties.
   */
  std::uint32_t max_depth_class_properties() const {
    return max_depth_class_properties_;
  }

  /**
   * Maximum number of hops that can be tracked for a call chain issue.
   */
  std::uint32_t max_call_chain_source_sink_distance() const {
    return max_call_chain_source_sink_distance_;
  }

  /**
   * Maximum size of the input access path of a propagation.
   *
   * This is the maximum depth of the propagation taint tree. We truncate the
   * ports upto this threshold when updating the tree. This is equivalent to
   * collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t propagation_max_input_path_size() const {
    return propagation_max_input_path_size_;
  }

  /**
   * Maximum number of leaves in input access path of a propagation.
   *
   * This is the maximum width of the propagation taint tree. When the number of
   * leaves exceeds this threshold, we compute the depth at which the tree
   * exceeds the threshold and collapse all the subtrees into the nodes at this
   * level.
   */
  std::uint32_t propagation_max_input_path_leaves() const {
    return propagation_max_input_path_leaves_;
  }

  /**
   * Maximum size of the output access path of propagations.
   *
   * This is the maximum depth of the propagation output paths tree. We truncate
   * the ports upto this threshold when updating the tree. This is equivalent
   * to collapsing all the subtrees exceeding this threshold to the node at this
   * maximum depth.
   */
  std::uint32_t propagation_max_output_path_size() const {
    return propagation_max_output_path_size_;
  }

  /**
   * Maximum number of leaves in the propagations output paths tree.
   *
   * This is the maximum width of the propagation output paths tree. When the
   * number of leaves exceeds this threshold, we compute the depth at which the
   * tree exceeds the threshold and collapse all the subtrees into the nodes at
   * this level.
   */
  std::uint32_t propagation_max_output_path_leaves() const {
    return propagation_max_output_path_leaves_;
  }

  /**
   * Maximum height of the output path tree of propagations after widening.
   *
   * When reaching the maximum, we collapse the leaves to reduce the height.
   */
  std::uint32_t propagation_output_path_tree_widening_height() const {
    return propagation_output_path_tree_widening_height_;
  }

  /**
   * Maximum height of the input taint tree when applying propagations.
   *
   * This is also the maximum collapse depth for inferred propagations.
   */
  std::uint32_t propagation_max_collapse_depth() const {
    return propagation_max_collapse_depth_;
  }

 private:
  void enforce_heuristics_consistency();

 private:
  std::uint32_t join_override_threshold_;
  std::uint32_t android_join_override_threshold_;
  std::optional<std::uint32_t> warn_override_threshold_;
  std::uint32_t source_sink_tree_widening_height_;
  std::uint32_t generation_max_port_size_;
  std::uint32_t generation_max_output_path_leaves_;
  std::uint32_t parameter_source_max_port_size_;
  std::uint32_t parameter_source_max_output_path_leaves_;
  std::uint32_t sink_max_port_size_;
  std::uint32_t sink_max_input_path_leaves_;
  std::uint32_t call_effect_source_max_port_size_;
  std::uint32_t call_effect_source_max_output_path_leaves_;
  std::uint32_t call_effect_sink_max_port_size_;
  std::uint32_t call_effect_sink_max_input_path_leaves_;
  std::uint32_t max_number_iterations_;
  std::uint32_t max_depth_class_properties_;
  std::uint32_t max_call_chain_source_sink_distance_;
  std::uint32_t propagation_max_input_path_size_;
  std::uint32_t propagation_max_input_path_leaves_;
  std::uint32_t propagation_max_output_path_size_;
  std::uint32_t propagation_max_output_path_leaves_;
  std::uint32_t propagation_output_path_tree_widening_height_;
  std::uint32_t propagation_max_collapse_depth_;
};

} // namespace marianatrench
