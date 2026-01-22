/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <mariana-trench/Access.h>

namespace marianatrench {

enum class Component {
  Activity,
  Service,
  BroadcastReceiver,
};

namespace constants {

std::string_view get_dfa_annotation_type();

std::string_view get_public_access_scope();

// TODO(T149770577): Should be configurable with model generator syntax.
// For now, use a mapping of method signature to intent argument position.
// Taint is routed via the intent, so its position per activity routing method
// needs to be known.
const std::unordered_map<std::string_view, ParameterPosition>&
get_activity_routing_methods();

const std::unordered_map<std::string_view, ParameterPosition>&
get_service_routing_methods();

const std::unordered_set<std::string_view>&
get_broadcast_receiver_routing_method_names();

const std::
    unordered_map<std::string_view, std::pair<ParameterPosition, Component>>&
    get_intent_receiving_method_names();

const std::string_view get_intent_receiving_api_method_name();

const std::unordered_map<std::string_view, ParameterPosition>&
get_intent_class_setters();

} // namespace constants
} // namespace marianatrench
