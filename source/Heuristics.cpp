/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

namespace {

// Default values for heuristics parameters.
constexpr std::uint32_t join_override_threshold_default = 40;
constexpr std::uint32_t android_join_override_threshold_default = 10;
constexpr std::optional<std::uint32_t> warn_override_threshold_default =
    std::nullopt;
constexpr std::uint32_t source_sink_tree_widening_height_default = 4;
constexpr std::uint32_t generation_max_port_size_default = 4;
constexpr std::uint32_t generation_max_output_path_leaves_default = 20;
constexpr std::uint32_t parameter_source_max_port_size_default = 4;
constexpr std::uint32_t parameter_source_max_output_path_leaves_default = 20;
constexpr std::uint32_t sink_max_port_size_default = 4;
constexpr std::uint32_t sink_max_input_path_leaves_default = 20;
constexpr std::uint32_t call_effect_source_max_port_size_default = 4;
constexpr std::uint32_t call_effect_source_max_output_path_leaves_default = 20;
constexpr std::uint32_t call_effect_sink_max_port_size_default = 4;
constexpr std::uint32_t call_effect_sink_max_input_path_leaves_default = 20;
constexpr std::uint32_t max_number_iterations_default = 150;
constexpr std::uint32_t max_depth_class_properties_default = 10;
constexpr std::uint32_t max_call_chain_source_sink_distance_default = 10;
constexpr std::uint32_t propagation_max_input_path_size_default = 4;
constexpr std::uint32_t propagation_max_input_path_leaves_default = 4;
constexpr std::uint32_t propagation_max_output_path_size_default = 4;
constexpr std::uint32_t propagation_max_output_path_leaves_default = 4;
constexpr std::uint32_t propagation_output_path_tree_widening_height_default =
    4;
constexpr std::uint32_t propagation_max_collapse_depth_default = 4;

Heuristics& get_mutable_singleton() {
  // Thread-safe global variable, initialized on first call.
  static Heuristics heuristics;
  return heuristics;
}

} // namespace

void Heuristics::enforce_heuristics_consistency() {
  if (propagation_max_collapse_depth_ > propagation_max_input_path_size_ ||
      propagation_max_collapse_depth_ > propagation_max_output_path_size_) {
    WARNING(
        1,
        "propagation_max_collapse_depth ({}) is greater than propagation_max_input_path_size ({}) and/or propagation_max_output_path_size ({}). "
        "Updating propagation_max_collapse_depth to the minimum of the two.",
        propagation_max_collapse_depth_,
        propagation_max_input_path_size_,
        propagation_max_output_path_size_);

    // For correctness, max collapse depth cannot be greater. If so, when
    // applying propagations for path sizes > propagation max sizes but < max
    // collapse depth, we will fail to collapse the taint tree and lead to
    // false-negative results.
    propagation_max_collapse_depth_ = std::min(
        propagation_max_input_path_size_, propagation_max_output_path_size_);
  }

  // source_sink_tree_widening_height is used in TaintTreeConfiguration and
  // hence applies for all TaintAccessPathTree. This limits the height of the
  // taint tree on widen operation. We allow separate *_max_port_size heuristics
  // to limit the height of the taint tree on write operations. These can be set
  // at different levels for different taint trees of a Model
  // (generations/parameter_sources/sinks/call_effect[source|sink]/propagations).
  //
  // Here, we log a warning when the common source_sink_tree_widening_height is
  // greater than the *_max_port_size heuristics as this means that the widening
  // operation will not affect the height of the tree. This is not incorrect but
  // might not be what the user is expecting.
  if (source_sink_tree_widening_height_ > generation_max_port_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > generation_max_port_size ({}). "
        "Both affect the maximum depth of the generation taint tree. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        generation_max_port_size_);
  }

  if (source_sink_tree_widening_height_ > sink_max_port_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > sink_max_port_size ({}). "
        "Both affect the maximum depth of the sink taint tree. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        sink_max_port_size_);
  }

  if (source_sink_tree_widening_height_ > parameter_source_max_port_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > parameter_source_max_port_size ({}). "
        "Both affect the maximum depth of the parameter source taint tree. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        parameter_source_max_port_size_);
  }

  if (source_sink_tree_widening_height_ > call_effect_source_max_port_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > call_effect_source_max_port_size ({}). "
        "Both affect the maximum depth of the call effect source taint tree. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        call_effect_source_max_port_size_);
  }

  if (source_sink_tree_widening_height_ > call_effect_sink_max_port_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > call_effect_sink_max_port_size ({}). "
        "Both affect the maximum depth of the call effect sink taint tree. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        call_effect_sink_max_port_size_);
  }

  if (source_sink_tree_widening_height_ > propagation_max_input_path_size_) {
    WARNING(
        1,
        "source_sink_tree_widening_height ({}) > propagation_max_input_path_size ({}). "
        "Both affect the maximum depth of the propagation taint tree input path. "
        "The final model may not be as expected.",
        source_sink_tree_widening_height_,
        propagation_max_input_path_size_);
  }

  // Similar to the source_sink_tree_widening_height,
  // propagation_output_path_tree_widening_height is used in
  // PathTreeConfiguration. Here we log a warning when the
  // propagation_output_path_tree_widening_height is greater than the
  // propagation_max_output_path_size heuristic as this means that the widening
  // operation on the PathTreeDomain will not affect the height of the tree.
  // This is not incorrect but might not be what the user is expecting.
  if (propagation_output_path_tree_widening_height_ >
      propagation_max_output_path_size_) {
    WARNING(
        1,
        "propagation_output_path_tree_widening_height ({}) > propagation_max_output_path_size ({}). "
        "Both affect the maximum depth of the propagation output path tree. "
        "The final model may not be as expected.",
        propagation_output_path_tree_widening_height_,
        propagation_max_output_path_size_);
  }
}

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
      propagation_output_path_tree_widening_height_(
          propagation_output_path_tree_widening_height_default),
      propagation_max_collapse_depth_(propagation_max_collapse_depth_default) {
  enforce_heuristics_consistency();
}

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
       "propagation_output_path_tree_widening_height",
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

  if (JsonValidation::has_field(
          value, "propagation_output_path_tree_widening_height")) {
    heuristics.propagation_output_path_tree_widening_height_ =
        JsonValidation::unsigned_integer(
            value, "propagation_output_path_tree_widening_height");
  }

  if (JsonValidation::has_field(value, "propagation_max_collapse_depth")) {
    heuristics.propagation_max_collapse_depth_ =
        JsonValidation::unsigned_integer(
            value, "propagation_max_collapse_depth");

    if (heuristics.propagation_max_collapse_depth_ >
        heuristics.propagation_max_output_path_size_) {
      throw JsonValidationError(
          value,
          std::nullopt,
          fmt::format(
              "propagation_max_collapse_depth ({}) > propagation_max_output_path_size ({}). "
              "Both affect the output path of propagations and propagation_max_output_path_size takes precedence. "
              "The final model may not be as expected.",
              heuristics.propagation_max_collapse_depth_,
              heuristics.propagation_max_output_path_size_));
    }

    if (heuristics.propagation_max_collapse_depth_ >
        heuristics.propagation_max_input_path_size_) {
      throw JsonValidationError(
          value,
          std::nullopt,
          fmt::format(
              "propagation_max_collapse_depth ({}) > propagation_max_input_path_size ({}). "
              "Both affect the output path of propagations and propagation_max_input_path_size takes precedence. "
              "The final model may not be as expected.",
              heuristics.propagation_max_collapse_depth_,
              heuristics.propagation_max_output_path_size_));
    }
  }

  heuristics.enforce_heuristics_consistency();
}

const Heuristics& Heuristics::singleton() {
  return get_mutable_singleton();
}

} // namespace marianatrench
