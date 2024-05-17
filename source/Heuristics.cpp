/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

// Names of the heuristics parameters that can be set
inline static const std::string join_override_threshold_attribute =
    "join_override_threshold";
inline static const std::string android_join_override_threshold_attribute =
    "android_join_override_threshold";
inline static const std::string warn_override_threshold_attribute =
    "warn_override_threshold";
inline static const std::string generation_max_port_size_attribute =
    "generation_max_port_size";
static inline const std::string generation_max_output_path_leaves_attribute =
    "generation_max_output_path_leaves";
static inline const std::string parameter_source_max_port_size_attribute =
    "parameter_source_max_port_size";
static inline const std::string
    parameter_source_max_output_path_leaves_attribute =
        "parameter_source_max_output_path_leaves";
static inline const std::string sink_max_port_size_attribute =
    "sink_max_port_size";
static inline const std::string sink_max_input_path_leaves_attribute =
    "sink_max_input_path_leaves";
static inline const std::string call_effect_source_max_port_size_attribute =
    "call_effect_source_max_port_size";
static inline const std::string
    call_effect_source_max_output_path_leaves_attribute =
        "call_effect_source_max_output_path_leaves";
static inline const std::string call_effect_sink_max_port_size_attribute =
    "call_effect_sink_max_port_size";
static inline const std::string
    call_effect_sink_max_input_path_leaves_attribute =
        "call_effect_sink_max_input_path_leaves";
static inline const std::string max_number_iterations_attribute =
    "max_number_iterations";
static inline const std::string max_depth_class_properties_attribute =
    "max_depth_class_properties";
static inline const std::string max_call_chain_source_sink_distance_attribute =
    "max_call_chain_source_sink_distance";
static inline const std::string propagation_max_input_path_size_attribute =
    "propagation_max_input_path_size";
static inline const std::string propagation_max_input_path_leaves_attribute =
    "propagation_max_input_path_leaves";

Heuristics::Heuristics(std::size_t join_override_threshold,
                       std::size_t android_join_override_threshold,
                       std::optional<std::size_t> warn_override_threshold,
                       std::size_t generation_max_port_size,
                       std::size_t generation_max_output_path_leaves,
                       std::size_t parameter_source_max_port_size,
                       std::size_t parameter_source_max_output_path_leaves,
                       std::size_t sink_max_port_size,
                       std::size_t sink_max_input_path_leaves,
                       std::size_t call_effect_source_max_port_size,
                       std::size_t call_effect_source_max_output_path_leaves,
                       std::size_t call_effect_sink_max_port_size,
                       std::size_t call_effect_sink_max_input_path_leaves,
                       std::size_t max_number_iterations,
                       std::size_t max_depth_class_properties,
                       std::size_t max_call_chain_source_sink_distance,
                       std::size_t propagation_max_input_path_size,
                       std::size_t propagation_max_input_path_leaves)
    : join_override_threshold_(join_override_threshold),
      android_join_override_threshold_(android_join_override_threshold),
      warn_override_threshold_(warn_override_threshold),
      generation_max_port_size_(generation_max_port_size),
      generation_max_output_path_leaves_(generation_max_output_path_leaves),
      parameter_source_max_port_size_(parameter_source_max_port_size),
      parameter_source_max_output_path_leaves_(
          parameter_source_max_output_path_leaves),
      sink_max_port_size_(sink_max_port_size),
      sink_max_input_path_leaves_(sink_max_input_path_leaves),
      call_effect_source_max_port_size_(call_effect_source_max_port_size),
      call_effect_source_max_output_path_leaves_(
          call_effect_source_max_output_path_leaves),
      call_effect_sink_max_port_size_(call_effect_sink_max_port_size),
      call_effect_sink_max_input_path_leaves_(
          call_effect_sink_max_input_path_leaves),
      max_number_iterations_(max_number_iterations),
      max_depth_class_properties_(max_depth_class_properties),
      max_call_chain_source_sink_distance_(max_call_chain_source_sink_distance),
      propagation_max_input_path_size_(propagation_max_input_path_size),
      propagation_max_input_path_leaves_(propagation_max_input_path_leaves) {}

Heuristics Heuristics::from_json(const Json::Value &value) {
  // Create an `Heuristics` object with the default values.
  Heuristics heuristics = Heuristics();
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {join_override_threshold_attribute,
       android_join_override_threshold_attribute,
       warn_override_threshold_attribute, generation_max_port_size_attribute,
       generation_max_output_path_leaves_attribute,
       parameter_source_max_port_size_attribute,
       parameter_source_max_output_path_leaves_attribute,
       sink_max_port_size_attribute, sink_max_input_path_leaves_attribute,
       call_effect_source_max_port_size_attribute,
       call_effect_source_max_output_path_leaves_attribute,
       call_effect_sink_max_port_size_attribute,
       call_effect_sink_max_input_path_leaves_attribute,
       max_number_iterations_attribute, max_depth_class_properties_attribute,
       max_call_chain_source_sink_distance_attribute,
       propagation_max_input_path_size_attribute,
       propagation_max_input_path_leaves_attribute});

  // Set the heuristics parameters that are specified in the JSON document.

  if (JsonValidation::has_field(value, join_override_threshold_attribute)) {
    heuristics.set_join_override_threshold(JsonValidation::unsigned_integer(
        value, join_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value,
                                android_join_override_threshold_attribute)) {
    heuristics.set_android_join_override_threshold(
        JsonValidation::unsigned_integer(
            value, android_join_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value, warn_override_threshold_attribute)) {
    heuristics.set_warn_override_threshold(JsonValidation::unsigned_integer(
        value, warn_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value, generation_max_port_size_attribute)) {
    heuristics.set_generation_max_port_size(JsonValidation::unsigned_integer(
        value, generation_max_port_size_attribute));
  }

  if (JsonValidation::has_field(value,
                                generation_max_output_path_leaves_attribute)) {
    heuristics.set_generation_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, generation_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                parameter_source_max_port_size_attribute)) {
    heuristics.set_parameter_source_max_port_size(
        JsonValidation::unsigned_integer(
            value, parameter_source_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, parameter_source_max_output_path_leaves_attribute)) {
    heuristics.set_parameter_source_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, parameter_source_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value, sink_max_port_size_attribute)) {
    heuristics.set_sink_max_port_size(
        JsonValidation::unsigned_integer(value, sink_max_port_size_attribute));
  }

  if (JsonValidation::has_field(value, sink_max_input_path_leaves_attribute)) {
    heuristics.set_sink_max_input_path_leaves(JsonValidation::unsigned_integer(
        value, sink_max_input_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                call_effect_source_max_port_size_attribute)) {
    heuristics.set_call_effect_source_max_port_size(
        JsonValidation::unsigned_integer(
            value, call_effect_source_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, call_effect_source_max_output_path_leaves_attribute)) {
    heuristics.set_call_effect_source_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, call_effect_source_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                call_effect_sink_max_port_size_attribute)) {
    heuristics.set_call_effect_sink_max_port_size(
        JsonValidation::unsigned_integer(
            value, call_effect_sink_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, call_effect_sink_max_input_path_leaves_attribute)) {
    heuristics.set_call_effect_sink_max_input_path_leaves(
        JsonValidation::unsigned_integer(
            value, call_effect_sink_max_input_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value, max_number_iterations_attribute)) {
    heuristics.set_max_number_iterations(JsonValidation::unsigned_integer(
        value, max_number_iterations_attribute));
  }

  if (JsonValidation::has_field(value, max_depth_class_properties_attribute)) {
    heuristics.set_max_depth_class_properties(JsonValidation::unsigned_integer(
        value, max_depth_class_properties_attribute));
  }

  if (JsonValidation::has_field(
          value, max_call_chain_source_sink_distance_attribute)) {
    heuristics.set_max_call_chain_source_sink_distance(
        JsonValidation::unsigned_integer(
            value, max_call_chain_source_sink_distance_attribute));
  }

  if (JsonValidation::has_field(value,
                                propagation_max_input_path_size_attribute)) {
    heuristics.set_propagation_max_input_path_size(
        JsonValidation::unsigned_integer(
            value, propagation_max_input_path_size_attribute));
  }

  if (JsonValidation::has_field(value,
                                propagation_max_input_path_leaves_attribute)) {
    heuristics.set_propagation_max_input_path_leaves(
        JsonValidation::unsigned_integer(
            value, propagation_max_input_path_leaves_attribute));
  }

  return heuristics;
}

std::size_t Heuristics::join_override_threshold() const {
  return this->join_override_threshold_;
}

void Heuristics::set_join_override_threshold(
    std::size_t join_override_threshold) {
  this->join_override_threshold_ = join_override_threshold;
}

std::size_t Heuristics::android_join_override_threshold() const {
  return this->android_join_override_threshold_;
}

void Heuristics::set_android_join_override_threshold(
    std::size_t android_join_override_threshold) {
  this->android_join_override_threshold_ = android_join_override_threshold;
}

const std::optional<std::size_t> Heuristics::warn_override_threshold() const {
  return this->warn_override_threshold_;
}

void Heuristics::set_warn_override_threshold(
    std::size_t warn_override_threshold) {
  this->warn_override_threshold_ = warn_override_threshold;
}

std::size_t Heuristics::generation_max_port_size() const {
  return this->generation_max_port_size_;
}

void Heuristics::set_generation_max_port_size(
    std::size_t generation_max_port_size) {
  this->generation_max_port_size_ = generation_max_port_size;
}

std::size_t Heuristics::generation_max_output_path_leaves() const {
  return this->generation_max_output_path_leaves_;
}

void Heuristics::set_generation_max_output_path_leaves(
    std::size_t generation_max_output_path_leaves) {
  this->generation_max_output_path_leaves_ = generation_max_output_path_leaves;
}

std::size_t Heuristics::parameter_source_max_port_size() const {
  return this->parameter_source_max_port_size_;
}

void Heuristics::set_parameter_source_max_port_size(
    std::size_t parameter_source_max_port_size) {
  this->parameter_source_max_port_size_ = parameter_source_max_port_size;
}

std::size_t Heuristics::parameter_source_max_output_path_leaves() const {
  return this->parameter_source_max_output_path_leaves_;
}

void Heuristics::set_parameter_source_max_output_path_leaves(
    std::size_t parameter_source_max_output_path_leaves) {
  this->parameter_source_max_output_path_leaves_ =
      parameter_source_max_output_path_leaves;
}

std::size_t Heuristics::sink_max_port_size() const {
  return this->sink_max_port_size_;
}

void Heuristics::set_sink_max_port_size(std::size_t sink_max_port_size) {
  this->sink_max_port_size_ = sink_max_port_size;
}

std::size_t Heuristics::sink_max_input_path_leaves() const {
  return this->sink_max_input_path_leaves_;
}

void Heuristics::set_sink_max_input_path_leaves(
    std::size_t sink_max_input_path_leaves) {
  this->sink_max_input_path_leaves_ = sink_max_input_path_leaves;
}

std::size_t Heuristics::call_effect_source_max_port_size() const {
  return this->call_effect_source_max_port_size_;
}

void Heuristics::set_call_effect_source_max_port_size(
    std::size_t call_effect_source_max_port_size) {
  this->call_effect_source_max_port_size_ = call_effect_source_max_port_size;
}

std::size_t Heuristics::call_effect_source_max_output_path_leaves() const {
  return this->call_effect_source_max_output_path_leaves_;
}

void Heuristics::set_call_effect_source_max_output_path_leaves(
    std::size_t call_effect_source_max_output_path_leaves) {
  this->call_effect_source_max_output_path_leaves_ =
      call_effect_source_max_output_path_leaves;
}

std::size_t Heuristics::call_effect_sink_max_port_size() const {
  return this->call_effect_sink_max_port_size_;
}

void Heuristics::set_call_effect_sink_max_port_size(
    std::size_t call_effect_sink_max_port_size) {
  this->call_effect_sink_max_port_size_ = call_effect_sink_max_port_size;
}

std::size_t Heuristics::call_effect_sink_max_input_path_leaves() const {
  return this->call_effect_sink_max_input_path_leaves_;
}

void Heuristics::set_call_effect_sink_max_input_path_leaves(
    std::size_t call_effect_sink_max_input_path_leaves) {
  this->call_effect_sink_max_input_path_leaves_ =
      call_effect_sink_max_input_path_leaves;
}

std::size_t Heuristics::max_number_iterations() const {
  return this->max_number_iterations_;
}

void Heuristics::set_max_number_iterations(std::size_t max_number_iterations) {
  this->max_number_iterations_ = max_number_iterations;
}

std::size_t Heuristics::max_depth_class_properties() const {
  return this->max_depth_class_properties_;
}

void Heuristics::set_max_depth_class_properties(
    std::size_t max_depth_class_properties) {
  this->max_depth_class_properties_ = max_depth_class_properties;
}

std::size_t Heuristics::max_call_chain_source_sink_distance() const {
  return this->max_call_chain_source_sink_distance_;
}

void Heuristics::set_max_call_chain_source_sink_distance(
    std::size_t max_call_chain_source_sink_distance) {
  this->max_call_chain_source_sink_distance_ =
      max_call_chain_source_sink_distance;
}

std::size_t Heuristics::propagation_max_input_path_size() const {
  return this->propagation_max_input_path_size_;
}

void Heuristics::set_propagation_max_input_path_size(
    std::size_t propagation_max_input_path_size) {
  this->propagation_max_input_path_size_ = propagation_max_input_path_size;
}

std::size_t Heuristics::propagation_max_input_path_leaves() const {
  return this->propagation_max_input_path_leaves_;
}

void Heuristics::set_propagation_max_input_path_leaves(
    std::size_t propagation_max_input_path_leaves) {
  this->propagation_max_input_path_leaves_ = propagation_max_input_path_leaves;
}

} // namespace marianatrench
