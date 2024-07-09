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
  std::size_t join_override_threshold() const;

  /**
   * When an android/java/google method has a set of overrides and greater than
   * this threshold, we do not join all overrides at call sites.
   */
  std::size_t android_join_override_threshold() const;

  /**
   * When a method which has a set of overrides greater than this threshold that
   * is not marked with `NoJoinVirtualOverrides` is called at least once, we
   * print a warning.
   */
  const std::optional<std::size_t> warn_override_threshold() const;

  /**
   * Maximum height of a taint (source or sink) tree after widening.
   *
   * When reaching the maximum, we collapse the leaves to reduce the height.
   * This parameter cannot be set a runtime, as it used at compile time.
   */
  constexpr static std::size_t kSourceSinkTreeWideningHeight = 4;

  /**
   * Maximum size of the port of a generation.
   */
  std::size_t generation_max_port_size() const;

  /**
   * Maximum number of leaves in the tree of output paths of generations.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t generation_max_output_path_leaves() const;

  /**
   * Maximum size of the port of a parameter source.
   */
  std::size_t parameter_source_max_port_size() const;

  /**
   * Maximum number of leaves in the tree of output paths of parameter sources.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t parameter_source_max_output_path_leaves() const;

  /**
   * Maximum size of the port of a sink.
   */
  std::size_t sink_max_port_size() const;

  /**
   * Maximum number of leaves in the tree of input paths of sinks.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t sink_max_input_path_leaves() const;

  /**
   * Maximum size of the port of a call effect source.
   */
  std::size_t call_effect_source_max_port_size() const;

  /**
   * Maximum number of leaves in the tree of output paths of call effect
   * sources.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t call_effect_source_max_output_path_leaves() const;

  /**
   * Maximum size of the port of a call effect sink.
   */
  std::size_t call_effect_sink_max_port_size() const;

  /**
   * Maximum number of leaves in the tree of input paths of call effect sinks.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t call_effect_sink_max_input_path_leaves() const;

  /**
   * Maximum number of global iterations before we abort the analysis.
   */
  std::size_t max_number_iterations() const;

  /**
   * Maximum number of local positions per frame.
   *
   * This parameter cannot be set a runtime, as it used at compile time.
   */
  constexpr static std::size_t kMaxNumberLocalPositions = 20;

  /**
   * Maximum depth of dependency graph traversal to find class properties.
   */
  std::size_t max_depth_class_properties() const;

  /**
   * Maximum number of hops that can be tracked for a call chain issue.
   */
  std::size_t max_call_chain_source_sink_distance() const;

  /**
   * Maximum size of the input access path of a propagation.
   */
  std::size_t propagation_max_input_path_size() const;

  /**
   * Maximum size of the output access path of a propagation.
   */
  std::size_t propagation_max_output_path_size() const;

  /**
   * Maximum number of leaves in the tree of input paths of propagations.
   */
  std::size_t propagation_max_input_path_leaves() const;

  /**
   * Maximum number of leaves in the tree of output paths of propagations.
   */
  std::size_t propagation_max_output_path_leaves() const;

  /**
   * Maximum height of the output path tree of propagations after widening.
   *
   * When reaching the maximum, we collapse the leaves to reduce the height.
   * This parameter cannot be set a runtime, as it used at compile time.
   */
  constexpr static std::size_t kPropagationOutputPathTreeWideningHeight = 4;

  /**
   * Maximum height of the input taint tree when applying propagations.
   *
   * This is also the maximum collapse depth for inferred propagations.
   */
  std::uint32_t propagation_max_collapse_depth() const;

 private:
  std::size_t join_override_threshold_;
  std::size_t android_join_override_threshold_;
  std::optional<std::size_t> warn_override_threshold_;
  std::size_t generation_max_port_size_;
  std::size_t generation_max_output_path_leaves_;
  std::size_t parameter_source_max_port_size_;
  std::size_t parameter_source_max_output_path_leaves_;
  std::size_t sink_max_port_size_;
  std::size_t sink_max_input_path_leaves_;
  std::size_t call_effect_source_max_port_size_;
  std::size_t call_effect_source_max_output_path_leaves_;
  std::size_t call_effect_sink_max_port_size_;
  std::size_t call_effect_sink_max_input_path_leaves_;
  std::size_t max_number_iterations_;
  std::size_t max_depth_class_properties_;
  std::size_t max_call_chain_source_sink_distance_;
  std::size_t propagation_max_input_path_size_;
  std::size_t propagation_max_input_path_leaves_;
  std::size_t propagation_max_output_path_size_;
  std::size_t propagation_max_output_path_leaves_;
  std::uint32_t propagation_max_collapse_depth_;
};

} // namespace marianatrench
