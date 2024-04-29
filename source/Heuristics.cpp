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
inline static const std::string k_join_override_threshold_attribute =
    "k_join_override_threshold";
inline static const std::string k_android_join_override_threshold_attribute =
    "k_android_join_override_threshold";
inline static const std::string k_warn_override_threshold_attribute =
    "k_warn_override_threshold";
inline static const std::string k_generation_max_port_size_attribute =
    "k_generation_max_port_size";
static inline const std::string k_generation_max_output_path_leaves_attribute =
    "k_generation_max_output_path_leaves";
static inline const std::string k_parameter_source_max_port_size_attribute =
    "k_parameter_source_max_port_size";
static inline const std::string k_parameter_source_max_output_path_leaves_attribute =
    "k_parameter_source_max_output_path_leaves";
static inline const std::string k_sink_max_port_size_attribute =
    "k_sink_max_port_size";
static inline const std::string k_sink_max_input_path_leaves_attribute =
    "k_sink_max_input_path_leaves";
static inline const std::string k_call_effect_source_max_port_size_attribute =
    "k_call_effect_source_max_port_size";
static inline const std::string
    k_call_effect_source_max_output_path_leaves_attribute =
        "k_call_effect_source_max_output_path_leaves";
static inline const std::string k_call_effect_sink_max_port_size_attribute =
    "k_call_effect_sink_max_port_size";
static inline const std::string k_call_effect_sink_max_input_path_leaves_attribute =
    "k_call_effect_sink_max_input_path_leaves";
static inline const std::string k_max_number_iterations_attribute =
    "k_max_number_iterations";
static inline const std::string k_max_depth_class_properties_attribute =
    "k_max_depth_class_properties";
static inline const std::string k_max_call_chain_source_sink_distance_attribute =
    "k_max_call_chain_source_sink_distance";
static inline const std::string k_propagation_max_input_path_size_attribute =
    "k_propagation_max_input_path_size";
static inline const std::string k_propagation_max_input_path_leaves_attribute =
    "k_propagation_max_input_path_leaves";

Heuristics::Heuristics(std::size_t k_join_override_threshold,
                       std::size_t k_android_join_override_threshold,
                       std::optional<std::size_t> k_warn_override_threshold,
                       std::size_t k_generation_max_port_size,
                       std::size_t k_generation_max_output_path_leaves,
                       std::size_t k_parameter_source_max_port_size,
                       std::size_t k_parameter_source_max_output_path_leaves,
                       std::size_t k_sink_max_port_size,
                       std::size_t k_sink_max_input_path_leaves,
                       std::size_t k_call_effect_source_max_port_size,
                       std::size_t k_call_effect_source_max_output_path_leaves,
                       std::size_t k_call_effect_sink_max_port_size,
                       std::size_t k_call_effect_sink_max_input_path_leaves,
                       std::size_t k_max_number_iterations,
                       std::size_t k_max_depth_class_properties,
                       std::size_t k_max_call_chain_source_sink_distance,
                       std::size_t k_propagation_max_input_path_size,
                       std::size_t k_propagation_max_input_path_leaves)
    : k_join_override_threshold_(k_join_override_threshold),
      k_android_join_override_threshold_(k_android_join_override_threshold),
      k_warn_override_threshold_(k_warn_override_threshold),
      k_generation_max_port_size_(k_generation_max_port_size),
      k_generation_max_output_path_leaves_(k_generation_max_output_path_leaves),
      k_parameter_source_max_port_size_(k_parameter_source_max_port_size),
      k_parameter_source_max_output_path_leaves_(
          k_parameter_source_max_output_path_leaves),
      k_sink_max_port_size_(k_sink_max_port_size),
      k_sink_max_input_path_leaves_(k_sink_max_input_path_leaves),
      k_call_effect_source_max_port_size_(k_call_effect_source_max_port_size),
      k_call_effect_source_max_output_path_leaves_(
          k_call_effect_source_max_output_path_leaves),
      k_call_effect_sink_max_port_size_(k_call_effect_sink_max_port_size),
      k_call_effect_sink_max_input_path_leaves_(
          k_call_effect_sink_max_input_path_leaves),
      k_max_number_iterations_(k_max_number_iterations),
      k_max_depth_class_properties_(k_max_depth_class_properties),
      k_max_call_chain_source_sink_distance_(
          k_max_call_chain_source_sink_distance),
      k_propagation_max_input_path_size_(k_propagation_max_input_path_size),
      k_propagation_max_input_path_leaves_(
          k_propagation_max_input_path_leaves) {}

Heuristics Heuristics::from_json(const Json::Value &value) {
  // Create an `Heuristics` object with the default values.
  Heuristics heuristics = Heuristics();
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {k_join_override_threshold_attribute,
       k_android_join_override_threshold_attribute,
       k_warn_override_threshold_attribute,
       k_generation_max_port_size_attribute,
       k_generation_max_output_path_leaves_attribute,
       k_parameter_source_max_port_size_attribute,
       k_parameter_source_max_output_path_leaves_attribute,
       k_sink_max_port_size_attribute, k_sink_max_input_path_leaves_attribute,
       k_call_effect_source_max_port_size_attribute,
       k_call_effect_source_max_output_path_leaves_attribute,
       k_call_effect_sink_max_port_size_attribute,
       k_call_effect_sink_max_input_path_leaves_attribute,
       k_max_number_iterations_attribute,
       k_max_depth_class_properties_attribute,
       k_max_call_chain_source_sink_distance_attribute,
       k_propagation_max_input_path_size_attribute,
       k_propagation_max_input_path_leaves_attribute});

  // Set the heuristics parameters that are specified in the JSON document.

  if (JsonValidation::has_field(value, k_join_override_threshold_attribute)) {
    heuristics.set_k_join_override_threshold(JsonValidation::unsigned_integer(
        value, k_join_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_android_join_override_threshold_attribute)) {
    heuristics.set_k_android_join_override_threshold(
        JsonValidation::unsigned_integer(
            value, k_android_join_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value, k_warn_override_threshold_attribute)) {
    heuristics.set_k_warn_override_threshold(JsonValidation::unsigned_integer(
        value, k_warn_override_threshold_attribute));
  }

  if (JsonValidation::has_field(value, k_generation_max_port_size_attribute)) {
    heuristics.set_k_generation_max_port_size(JsonValidation::unsigned_integer(
        value, k_generation_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, k_generation_max_output_path_leaves_attribute)) {
    heuristics.set_k_generation_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_generation_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_parameter_source_max_port_size_attribute)) {
    heuristics.set_k_parameter_source_max_port_size(
        JsonValidation::unsigned_integer(
            value, k_parameter_source_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, k_parameter_source_max_output_path_leaves_attribute)) {
    heuristics.set_k_parameter_source_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_parameter_source_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value, k_sink_max_port_size_attribute)) {
    heuristics.set_k_sink_max_port_size(JsonValidation::unsigned_integer(
        value, k_sink_max_port_size_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_sink_max_input_path_leaves_attribute)) {
    heuristics.set_k_sink_max_input_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_sink_max_input_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_call_effect_source_max_port_size_attribute)) {
    heuristics.set_k_call_effect_source_max_port_size(
        JsonValidation::unsigned_integer(
            value, k_call_effect_source_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, k_call_effect_source_max_output_path_leaves_attribute)) {
    heuristics.set_k_call_effect_source_max_output_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_call_effect_source_max_output_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_call_effect_sink_max_port_size_attribute)) {
    heuristics.set_k_call_effect_sink_max_port_size(
        JsonValidation::unsigned_integer(
            value, k_call_effect_sink_max_port_size_attribute));
  }

  if (JsonValidation::has_field(
          value, k_call_effect_sink_max_input_path_leaves_attribute)) {
    heuristics.set_k_call_effect_sink_max_input_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_call_effect_sink_max_input_path_leaves_attribute));
  }

  if (JsonValidation::has_field(value, k_max_number_iterations_attribute)) {
    heuristics.set_k_max_number_iterations(JsonValidation::unsigned_integer(
        value, k_max_number_iterations_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_max_depth_class_properties_attribute)) {
    heuristics.set_k_max_depth_class_properties(
        JsonValidation::unsigned_integer(
            value, k_max_depth_class_properties_attribute));
  }

  if (JsonValidation::has_field(
          value, k_max_call_chain_source_sink_distance_attribute)) {
    heuristics.set_k_max_call_chain_source_sink_distance(
        JsonValidation::unsigned_integer(
            value, k_max_call_chain_source_sink_distance_attribute));
  }

  if (JsonValidation::has_field(value,
                                k_propagation_max_input_path_size_attribute)) {
    heuristics.set_k_propagation_max_input_path_size(
        JsonValidation::unsigned_integer(
            value, k_propagation_max_input_path_size_attribute));
  }

  if (JsonValidation::has_field(
          value, k_propagation_max_input_path_leaves_attribute)) {
    heuristics.set_k_propagation_max_input_path_leaves(
        JsonValidation::unsigned_integer(
            value, k_propagation_max_input_path_leaves_attribute));
  }

  return heuristics;
}

std::size_t Heuristics::k_join_override_threshold() const {
  return this->k_join_override_threshold_;
}

void Heuristics::set_k_join_override_threshold(
    std::size_t k_join_override_threshold) {
  this->k_join_override_threshold_ = k_join_override_threshold;
}

std::size_t Heuristics::k_android_join_override_threshold() const {
  return this->k_android_join_override_threshold_;
}

void Heuristics::set_k_android_join_override_threshold(
    std::size_t k_android_join_override_threshold) {
  this->k_android_join_override_threshold_ = k_android_join_override_threshold;
}

const std::optional<std::size_t> Heuristics::k_warn_override_threshold() const {
  return this->k_warn_override_threshold_;
}

void Heuristics::set_k_warn_override_threshold(
    std::size_t k_warn_override_threshold) {
  this->k_warn_override_threshold_ = k_warn_override_threshold;
}

std::size_t Heuristics::k_generation_max_port_size() const {
  return this->k_generation_max_port_size_;
}

void Heuristics::set_k_generation_max_port_size(
    std::size_t k_generation_max_port_size) {
  this->k_generation_max_port_size_ = k_generation_max_port_size;
}

std::size_t Heuristics::k_generation_max_output_path_leaves() const {
  return this->k_generation_max_output_path_leaves_;
}

void Heuristics::set_k_generation_max_output_path_leaves(
    std::size_t k_generation_max_output_path_leaves) {
  this->k_generation_max_output_path_leaves_ =
      k_generation_max_output_path_leaves;
}

std::size_t Heuristics::k_parameter_source_max_port_size() const {
  return this->k_parameter_source_max_port_size_;
}

void Heuristics::set_k_parameter_source_max_port_size(
    std::size_t k_parameter_source_max_port_size) {
  this->k_parameter_source_max_port_size_ = k_parameter_source_max_port_size;
}

std::size_t Heuristics::k_parameter_source_max_output_path_leaves() const {
  return this->k_parameter_source_max_output_path_leaves_;
}

void Heuristics::set_k_parameter_source_max_output_path_leaves(
    std::size_t k_parameter_source_max_output_path_leaves) {
  this->k_parameter_source_max_output_path_leaves_ =
      k_parameter_source_max_output_path_leaves;
}

std::size_t Heuristics::k_sink_max_port_size() const {
  return this->k_sink_max_port_size_;
}

void Heuristics::set_k_sink_max_port_size(std::size_t k_sink_max_port_size) {
  this->k_sink_max_port_size_ = k_sink_max_port_size;
}

std::size_t Heuristics::k_sink_max_input_path_leaves() const {
  return this->k_sink_max_input_path_leaves_;
}

void Heuristics::set_k_sink_max_input_path_leaves(
    std::size_t k_sink_max_input_path_leaves) {
  this->k_sink_max_input_path_leaves_ = k_sink_max_input_path_leaves;
}

std::size_t Heuristics::k_call_effect_source_max_port_size() const {
  return this->k_call_effect_source_max_port_size_;
}

void Heuristics::set_k_call_effect_source_max_port_size(
    std::size_t k_call_effect_source_max_port_size) {
  this->k_call_effect_source_max_port_size_ =
      k_call_effect_source_max_port_size;
}

std::size_t Heuristics::k_call_effect_source_max_output_path_leaves() const {
  return this->k_call_effect_source_max_output_path_leaves_;
}

void Heuristics::set_k_call_effect_source_max_output_path_leaves(
    std::size_t k_call_effect_source_max_output_path_leaves) {
  this->k_call_effect_source_max_output_path_leaves_ =
      k_call_effect_source_max_output_path_leaves;
}

std::size_t Heuristics::k_call_effect_sink_max_port_size() const {
  return this->k_call_effect_sink_max_port_size_;
}

void Heuristics::set_k_call_effect_sink_max_port_size(
    std::size_t k_call_effect_sink_max_port_size) {
  this->k_call_effect_sink_max_port_size_ = k_call_effect_sink_max_port_size;
}

std::size_t Heuristics::k_call_effect_sink_max_input_path_leaves() const {
  return this->k_call_effect_sink_max_input_path_leaves_;
}

void Heuristics::set_k_call_effect_sink_max_input_path_leaves(
    std::size_t k_call_effect_sink_max_input_path_leaves) {
  this->k_call_effect_sink_max_input_path_leaves_ =
      k_call_effect_sink_max_input_path_leaves;
}

std::size_t Heuristics::k_max_number_iterations() const {
  return this->k_max_number_iterations_;
}

void Heuristics::set_k_max_number_iterations(
    std::size_t k_max_number_iterations) {
  this->k_max_number_iterations_ = k_max_number_iterations;
}

std::size_t Heuristics::k_max_depth_class_properties() const {
  return this->k_max_depth_class_properties_;
}

void Heuristics::set_k_max_depth_class_properties(
    std::size_t k_max_depth_class_properties) {
  this->k_max_depth_class_properties_ = k_max_depth_class_properties;
}

std::size_t Heuristics::k_max_call_chain_source_sink_distance() const {
  return this->k_max_call_chain_source_sink_distance_;
}

void Heuristics::set_k_max_call_chain_source_sink_distance(
    std::size_t k_max_call_chain_source_sink_distance) {
  this->k_max_call_chain_source_sink_distance_ =
      k_max_call_chain_source_sink_distance;
}

std::size_t Heuristics::k_propagation_max_input_path_size() const {
  return this->k_propagation_max_input_path_size_;
}

void Heuristics::set_k_propagation_max_input_path_size(
    std::size_t k_propagation_max_input_path_size) {
  this->k_propagation_max_input_path_size_ = k_propagation_max_input_path_size;
}

std::size_t Heuristics::k_propagation_max_input_path_leaves() const {
  return this->k_propagation_max_input_path_leaves_;
}

void Heuristics::set_k_propagation_max_input_path_leaves(
    std::size_t k_propagation_max_input_path_leaves) {
  this->k_propagation_max_input_path_leaves_ =
      k_propagation_max_input_path_leaves;
}

} // namespace marianatrench
