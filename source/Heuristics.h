/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <optional>

#include <json/json.h>

namespace marianatrench {

class Heuristics final {
public:
  explicit Heuristics(
      std::size_t k_join_override_threshold = k_join_override_threshold_default,
      std::size_t k_android_join_override_threshold =
          k_android_join_override_threshold_default,
      std::optional<std::size_t> k_warn_override_threshold =
          k_warn_override_threshold_default,
      std::size_t k_generation_max_port_size =
          k_generation_max_port_size_default,
      std::size_t k_generation_max_output_path_leaves =
          k_generation_max_output_path_leaves_default,
      std::size_t k_parameter_source_max_port_size =
          k_parameter_source_max_port_size_default,
      std::size_t k_parameter_source_max_output_path_leaves =
          k_parameter_source_max_output_path_leaves_default,
      std::size_t k_sink_max_port_size = k_sink_max_port_size_default,
      std::size_t k_sink_max_input_path_leaves =
          k_sink_max_input_path_leaves_default,
      std::size_t k_call_effect_source_max_port_size =
          k_call_effect_source_max_port_size_default,
      std::size_t k_call_effect_source_max_output_path_leaves =
          k_call_effect_source_max_output_path_leaves_default,
      std::size_t k_call_effect_sink_max_port_size =
          k_call_effect_sink_max_port_size_default,
      std::size_t k_call_effect_sink_max_input_path_leaves =
          k_call_effect_sink_max_input_path_leaves_default,
      std::size_t k_max_number_iterations = k_max_number_iterations_default,
      std::size_t k_max_depth_class_properties =
          k_max_depth_class_properties_default,
      std::size_t k_max_call_chain_source_sink_distance =
          k_max_call_chain_source_sink_distance_default,
      std::size_t k_propagation_max_input_path_size =
          k_propagation_max_input_path_size_default,
      std::size_t k_propagation_max_input_path_leaves =
          k_propagation_max_input_path_leaves_default);

  bool operator==(const Heuristics &other) const;

  static Heuristics from_json(const Json::Value &value);

  /**
   * When a method has a set of overrides greater than this threshold, we do not
   * join all overrides at call sites.
   */
  std::size_t k_join_override_threshold() const;
  constexpr static std::size_t k_join_override_threshold_default = 40;
  void set_k_join_override_threshold(std::size_t k_join_override_threshold);

  /**
   * When an android/java/google method has a set of overrides and greater than
   * this threshold, we do not join all overrides at call sites.
   */
  std::size_t k_android_join_override_threshold() const;
  constexpr static std::size_t k_android_join_override_threshold_default = 10;
  void set_k_android_join_override_threshold(
      std::size_t k_android_join_override_threshold);

  /**
   * When a method which has a set of overrides greater than this threshold that
   * is not marked with `NoJoinVirtualOverrides` is called at least once, we
   * print a warning.
   */
  const std::optional<std::size_t> k_warn_override_threshold() const;
  constexpr static std::optional<std::size_t>
      k_warn_override_threshold_default = std::nullopt;
  void set_k_warn_override_threshold(std::size_t k_warn_override_threshold);

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
  std::size_t k_generation_max_port_size() const;
  constexpr static std::size_t k_generation_max_port_size_default = 4;
  void set_k_generation_max_port_size(std::size_t k_generation_max_port_size);

  /**
   * Maximum number of leaves in the tree of output paths of generations.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t k_generation_max_output_path_leaves() const;
  constexpr static std::size_t k_generation_max_output_path_leaves_default = 20;
  void set_k_generation_max_output_path_leaves(
      std::size_t k_generation_max_output_path_leaves);

  /**
   * Maximum size of the port of a parameter source.
   */
  std::size_t k_parameter_source_max_port_size() const;
  constexpr static std::size_t k_parameter_source_max_port_size_default = 4;
  void set_k_parameter_source_max_port_size(
      std::size_t k_parameter_source_max_port_size);

  /**
   * Maximum number of leaves in the tree of output paths of parameter sources.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t k_parameter_source_max_output_path_leaves() const;
  constexpr static std::size_t
      k_parameter_source_max_output_path_leaves_default = 20;
  void set_k_parameter_source_max_output_path_leaves(
      std::size_t k_parameter_source_max_output_path_leaves);

  /**
   * Maximum size of the port of a sink.
   */
  std::size_t k_sink_max_port_size() const;
  constexpr static std::size_t k_sink_max_port_size_default = 4;
  void set_k_sink_max_port_size(std::size_t k_sink_max_port_size);

  /**
   * Maximum number of leaves in the tree of input paths of sinks.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t k_sink_max_input_path_leaves() const;
  constexpr static std::size_t k_sink_max_input_path_leaves_default = 20;
  void
  set_k_sink_max_input_path_leaves(std::size_t k_sink_max_input_path_leaves);

  /**
   * Maximum size of the port of a call effect source.
   */
  std::size_t k_call_effect_source_max_port_size() const;
  constexpr static std::size_t k_call_effect_source_max_port_size_default = 4;
  void set_k_call_effect_source_max_port_size(
      std::size_t k_call_effect_source_max_port_size);

  /**
   * Maximum number of leaves in the tree of output paths of call effect
   * sources.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t k_call_effect_source_max_output_path_leaves() const;
  constexpr static std::size_t
      k_call_effect_source_max_output_path_leaves_default = 20;
  void set_k_call_effect_source_max_output_path_leaves(
      std::size_t k_call_effect_source_max_output_path_leaves);

  /**
   * Maximum size of the port of a call effect sink.
   */
  std::size_t k_call_effect_sink_max_port_size() const;
  constexpr static std::size_t k_call_effect_sink_max_port_size_default = 4;
  void set_k_call_effect_sink_max_port_size(
      std::size_t k_call_effect_sink_max_port_size);

  /**
   * Maximum number of leaves in the tree of input paths of call effect sinks.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  std::size_t k_call_effect_sink_max_input_path_leaves() const;
  constexpr static std::size_t
      k_call_effect_sink_max_input_path_leaves_default = 20;
  void set_k_call_effect_sink_max_input_path_leaves(
      std::size_t k_call_effect_sink_max_input_path_leaves);

  /**
   * Maximum number of global iterations before we abort the analysis.
   */
  std::size_t k_max_number_iterations() const;
  constexpr static std::size_t k_max_number_iterations_default = 150;
  void set_k_max_number_iterations(std::size_t k_max_number_iterations);

  /**
   * Maximum number of local positions per frame.
   *
   * This parameter cannot be set a runtime, as it used at compile time.
   */
  constexpr static std::size_t kMaxNumberLocalPositions = 20;

  /**
   * Maximum depth of dependency graph traversal to find class properties.
   */
  std::size_t k_max_depth_class_properties() const;
  constexpr static std::size_t k_max_depth_class_properties_default = 10;
  void
  set_k_max_depth_class_properties(std::size_t k_max_depth_class_properties);

  /**
   * Maximum number of hops that can be tracked for a call chain issue.
   */
  std::size_t k_max_call_chain_source_sink_distance() const;
  constexpr static std::size_t k_max_call_chain_source_sink_distance_default =
      10;
  void set_k_max_call_chain_source_sink_distance(
      std::size_t k_max_call_chain_source_sink_distance);

  /**
   * Maximum size of the input access path of a propagation.
   */
  std::size_t k_propagation_max_input_path_size() const;
  constexpr static std::size_t k_propagation_max_input_path_size_default = 4;
  void set_k_propagation_max_input_path_size(
      std::size_t k_propagation_max_input_path_size);

  /**
   * Maximum size of the output access path of a propagation.
   */
  constexpr static std::size_t kPropagationMaxOutputPathSize = 4;

  /**
   * Maximum number of leaves in the tree of input paths of propagations.
   */
  std::size_t k_propagation_max_input_path_leaves() const;
  constexpr static std::size_t k_propagation_max_input_path_leaves_default = 4;
  void set_k_propagation_max_input_path_leaves(
      std::size_t k_propagation_max_input_path_leaves);

  /**
   * Maximum number of leaves in the tree of output paths of propagations.
   */
  constexpr static std::size_t kPropagationMaxOutputPathLeaves = 4;

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
  constexpr static std::uint32_t kPropagationMaxCollapseDepth = 4;

private:
  std::size_t k_join_override_threshold_;
  std::size_t k_android_join_override_threshold_;
  std::optional<std::size_t> k_warn_override_threshold_;
  std::size_t k_generation_max_port_size_;
  std::size_t k_generation_max_output_path_leaves_;
  std::size_t k_parameter_source_max_port_size_;
  std::size_t k_parameter_source_max_output_path_leaves_;
  std::size_t k_sink_max_port_size_;
  std::size_t k_sink_max_input_path_leaves_;
  std::size_t k_call_effect_source_max_port_size_;
  std::size_t k_call_effect_source_max_output_path_leaves_;
  std::size_t k_call_effect_sink_max_port_size_;
  std::size_t k_call_effect_sink_max_input_path_leaves_;
  std::size_t k_max_number_iterations_;
  std::size_t k_max_depth_class_properties_;
  std::size_t k_max_call_chain_source_sink_distance_;
  std::size_t k_propagation_max_input_path_size_;
  std::size_t k_propagation_max_input_path_leaves_;
};

} // namespace marianatrench
