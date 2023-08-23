/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Constants.h>

namespace marianatrench {
namespace constants {

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
dfa_annotation get_dfa_annotation() {
  return {
      /* type */ "<undefined>",
      /* pattern_type */ "<undefined>",
  };
}

std::vector<std::string> get_private_uri_schemes() {
  return {};
}

std::string_view get_privacy_decision_type() {
  return "<undefined>";
}

std::unordered_map<std::string, ParameterPosition>
get_activity_routing_methods() {
  return {
      // For ShimsTest.cpp
      {"LClass;.startActivity:(Landroid/content/Intent;)V", 1},
      {"Landroid/app/Activity;.startActivity:(Landroid/content/Intent;)V", 1},
  };
}

std::unordered_map<std::string, ParameterPosition>
get_service_routing_methods() {
  return {
      {"Landroid/content/Context;.startService:(Landroid/content/Intent;)Landroid/content/ComponentName;",
       1},
  };
}

const std::unordered_set<std::string>&
get_broadcast_receiver_routing_method_names() {
  static const std::unordered_set<std::string> receiver_routing_method_names = {
      "sendBroadcast",
      "sendBroadcastAsUser",
      "sendBroadcastWithMultiplePermissions",
      "sendOrderedBroadcast",
      "sendOrderedBroadcastAsUser",
      "sendStickyBroadcast",
      "sendStickyBroadcastAsUser",
      "sendStickyOrderedBroadcast",
      "sendStickyOrderedBroadcastAsUser"};
  return receiver_routing_method_names;
}

const std::unordered_map<std::string, ParameterPosition>&
get_intent_receiving_method_names() {
  static const std::unordered_map<std::string, ParameterPosition>
      intent_receiving_method_names = {
          {"onStartCommand", 1}, {"onHandleIntent", 1}};
  return intent_receiving_method_names;
}

const std::unordered_map<std::string, ParameterPosition>&
get_intent_class_setters() {
  static const std::unordered_map<std::string, ParameterPosition> intent_class_setters =
      {{"Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V",
        2},
       {"Landroid/content/Intent;.<init>:(Ljava/lang/String;Landroid/net/Uri;Landroid/content/Context;Ljava/lang/Class;)V",
        4},
       {"Landroid/content/Intent;.setClass:(Landroid/content/Context;Ljava/lang/Class;)Landroid/content/Intent;",
        2}};
  return intent_class_setters;
}

#endif

} // namespace constants
} // namespace marianatrench
