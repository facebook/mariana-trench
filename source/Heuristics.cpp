/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

namespace {

Heuristics& get_mutable_singleton() {
  // Thread-safe global variable, initialized on first call.
  static Heuristics heuristics;
  return heuristics;
}

} // namespace

// Default values for heuristics parameters.
constexpr std::size_t join_override_threshold_default = 40;
constexpr std::size_t android_join_override_threshold_default = 10;
constexpr std::optional<std::size_t> warn_override_threshold_default =
    std::nullopt;
constexpr std::size_t source_sink_tree_widening_height_default = 4;
constexpr std::size_t generation_max_port_size_default = 4;
constexpr std::size_t generation_max_output_path_leaves_default = 20;
constexpr std::size_t parameter_source_max_port_size_default = 4;
constexpr std::size_t parameter_source_max_output_path_leaves_default = 20;
constexpr std::size_t sink_max_port_size_default = 4;
constexpr std::size_t sink_max_input_path_leaves_default = 20;
constexpr std::size_t call_effect_source_max_port_size_default = 4;
constexpr std::size_t call_effect_source_max_output_path_leaves_default = 20;
constexpr std::size_t call_effect_sink_max_port_size_default = 4;
constexpr std::size_t call_effect_sink_max_input_path_leaves_default = 20;
constexpr std::size_t max_number_iterations_default = 150;
constexpr std::size_t max_depth_class_properties_default = 10;
constexpr std::size_t max_call_chain_source_sink_distance_default = 10;
constexpr std::size_t propagation_max_input_path_size_default = 4;
constexpr std::size_t propagation_max_input_path_leaves_default = 4;
constexpr std::size_t propagation_max_output_path_size_default = 4;
constexpr std::size_t propagation_max_output_path_leaves_default = 4;
constexpr std::uint32_t propagation_max_collapse_depth_default = 4;

Heuristics::Heuristics()
    : join_override_threshold_(join_override_threshold_default),
      android_join_override_threshold_(android_join_override_threshold_default),
      warn_override_threshold_(warn_override_threshold_default),
      source_sink_tree_widening_height_(
          source_sink_tree_widening_height_default),
      generation_max_port_size_(generation_max_port_size_default),
      generation_max_output_path_leaves_(
          generation_max_output_path_leaves_default),
      parameter_source_max_port_size_(parameter_source_max_port_size_default),
      parameter_source_max_output_path_leaves_(
          parameter_source_max_output_path_leaves_default),
      sink_max_port_size_(sink_max_port_size_default),
      sink_max_input_path_leaves_(sink_max_input_path_leaves_default),
      call_effect_source_max_port_size_(
          call_effect_source_max_port_size_default),
      call_effect_source_max_output_path_leaves_(
          call_effect_source_max_output_path_leaves_default),
      call_effect_sink_max_port_size_(call_effect_sink_max_port_size_default),
      call_effect_sink_max_input_path_leaves_(
          call_effect_sink_max_input_path_leaves_default),
      max_number_iterations_(max_number_iterations_default),
      max_depth_class_properties_(max_depth_class_properties_default),
      max_call_chain_source_sink_distance_(
          max_call_chain_source_sink_distance_default),
      propagation_max_input_path_size_(propagation_max_input_path_size_default),
      propagation_max_input_path_leaves_(
          propagation_max_input_path_leaves_default),
      propagation_max_output_path_size_(
          propagation_max_output_path_size_default),
      propagation_max_output_path_leaves_(
          propagation_max_output_path_leaves_default),
      propagation_max_collapse_depth_(propagation_max_collapse_depth_default) {}

void Heuristics::init_from_file(const std::filesystem::path& heuristics_path) {
  // Create an `Heuristics` object with the default values.
  Json::Value value = JsonReader::parse_json_file(heuristics_path);
  auto& heuristics = get_mutable_singleton();
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {"join_override_threshold",
       "android_join_override_threshold",
       "warn_override_threshold",
       "source_sink_tree_widening_height",
       "generation_max_port_size",
       "generation_max_output_path_leaves",
       "parameter_source_max_port_size",
       "parameter_source_max_output_path_leaves",
       "sink_max_port_size",
       "sink_max_input_path_leaves",
       "call_effect_source_max_port_size",
       "call_effect_source_max_output_path_leaves",
       "call_effect_sink_max_port_size",
       "call_effect_sink_max_input_path_leaves",
       "max_number_iterations",
       "max_depth_class_properties",
       "max_call_chain_source_sink_distance",
       "propagation_max_input_path_size",
       "propagation_max_input_path_leaves",
       "propagation_max_output_path_size",
       "propagation_max_output_path_leaves",
       "propagation_max_collapse_depth"});

  // Set the heuristics parameters that are specified in the JSON document.

  if (JsonValidation::has_field(value, "join_override_threshold")) {
    heuristics.join_override_threshold_ =
        JsonValidation::unsigned_integer(value, "join_override_threshold");
  }

  if (JsonValidation::has_field(value, "android_join_override_threshold")) {
    heuristics.android_join_override_threshold_ =
        JsonValidation::unsigned_integer(
            value, "android_join_override_threshold");
  }

  if (JsonValidation::has_field(value, "warn_override_threshold")) {
    heuristics.warn_override_threshold_ =
        JsonValidation::unsigned_integer(value, "warn_override_threshold");
  }

  if (JsonValidation::has_field(value, "source_sink_tree_widening_height")) {
    heuristics.source_sink_tree_widening_height_ =
        JsonValidation::unsigned_integer(
            value, "source_sink_tree_widening_height");
  }

  if (JsonValidation::has_field(value, "generation_max_port_size")) {
    heuristics.generation_max_port_size_ =
        JsonValidation::unsigned_integer(value, "generation_max_port_size");
  }

  if (JsonValidation::has_field(value, "generation_max_output_path_leaves")) {
    heuristics.generation_max_output_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "generation_max_output_path_leaves");
  }

  if (JsonValidation::has_field(value, "parameter_source_max_port_size")) {
    heuristics.parameter_source_max_port_size_ =
        JsonValidation::unsigned_integer(
            value, "parameter_source_max_port_size");
  }

  if (JsonValidation::has_field(
          value, "parameter_source_max_output_path_leaves")) {
    heuristics.parameter_source_max_output_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "parameter_source_max_output_path_leaves");
  }

  if (JsonValidation::has_field(value, "sink_max_port_size")) {
    heuristics.sink_max_port_size_ =
        JsonValidation::unsigned_integer(value, "sink_max_port_size");
  }

  if (JsonValidation::has_field(value, "sink_max_input_path_leaves")) {
    heuristics.sink_max_input_path_leaves_ =
        JsonValidation::unsigned_integer(value, "sink_max_input_path_leaves");
  }

  if (JsonValidation::has_field(value, "call_effect_source_max_port_size")) {
    heuristics.call_effect_source_max_port_size_ =
        JsonValidation::unsigned_integer(
            value, "call_effect_source_max_port_size");
  }

  if (JsonValidation::has_field(
          value, "call_effect_source_max_output_path_leaves")) {
    heuristics.call_effect_source_max_output_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "call_effect_source_max_output_path_leaves");
  }

  if (JsonValidation::has_field(value, "call_effect_sink_max_port_size")) {
    heuristics.call_effect_sink_max_port_size_ =
        JsonValidation::unsigned_integer(
            value, "call_effect_sink_max_port_size");
  }

  if (JsonValidation::has_field(
          value, "call_effect_sink_max_input_path_leaves")) {
    heuristics.call_effect_sink_max_input_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "call_effect_sink_max_input_path_leaves");
  }

  if (JsonValidation::has_field(value, "max_number_iterations")) {
    heuristics.max_number_iterations_ =
        JsonValidation::unsigned_integer(value, "max_number_iterations");
  }

  if (JsonValidation::has_field(value, "max_depth_class_properties")) {
    heuristics.max_depth_class_properties_ =
        JsonValidation::unsigned_integer(value, "max_depth_class_properties");
  }

  if (JsonValidation::has_field(value, "max_call_chain_source_sink_distance")) {
    heuristics.max_call_chain_source_sink_distance_ =
        JsonValidation::unsigned_integer(
            value, "max_call_chain_source_sink_distance");
  }

  if (JsonValidation::has_field(value, "propagation_max_input_path_size")) {
    heuristics.propagation_max_input_path_size_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_input_path_size");
  }

  if (JsonValidation::has_field(value, "propagation_max_input_path_leaves")) {
    heuristics.propagation_max_input_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_input_path_leaves");
  }

  if (JsonValidation::has_field(value, "propagation_max_output_path_size")) {
    heuristics.propagation_max_output_path_size_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_output_path_size");
  }

  if (JsonValidation::has_field(value, "propagation_max_output_path_leaves")) {
    heuristics.propagation_max_output_path_leaves_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_output_path_leaves");
  }

  if (JsonValidation::has_field(value, "propagation_max_collapse_depth")) {
    heuristics.propagation_max_collapse_depth_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_collapse_depth");
  }
}

const Heuristics& Heuristics::singleton() {
  return get_mutable_singleton();
}

} // namespace marianatrench
