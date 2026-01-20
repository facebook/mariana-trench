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
std::string_view get_dfa_annotation_type() {
  return "<undefined>";
}

std::string_view get_public_access_scope() {
  return "<undefined>";
}

const std::unordered_map<std::string_view, ParameterPosition>&
get_activity_routing_methods() {
  static std::unordered_map<std::string_view, ParameterPosition>
      activity_routing_methods = {
          // For ShimsTest.cpp
          {"LClass;.startActivity:(Landroid/content/Intent;)V", 1},
          {"Landroid/app/Activity;.startActivity:(Landroid/content/Intent;)V",
           1},
      };

  return activity_routing_methods;
}

const std::unordered_map<std::string_view, ParameterPosition>&
get_service_routing_methods() {
  static std::unordered_map<std::string_view, ParameterPosition>
      service_routing_methods = {
          {"Landroid/content/Context;.startService:(Landroid/content/Intent;)Landroid/content/ComponentName;",
           1},
          {"Landroidx/core/app/JobIntentService;.enqueueWork:(Landroid/content/Context;Ljava/lang/Class;ILandroid/content/Intent;)V",
           3},
      };

  return service_routing_methods;
}

const std::unordered_set<std::string_view>&
get_broadcast_receiver_routing_method_names() {
  static const std::unordered_set<std::string_view>
      receiver_routing_method_names = {
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

const std::
    unordered_map<std::string_view, std::pair<ParameterPosition, Component>>&
    get_intent_receiving_method_names() {
  static const std::
      unordered_map<std::string_view, std::pair<ParameterPosition, Component>>
          intent_receiving_method_names = {
              {"onStartCommand", {1, Component::Service}},
              {"onHandleIntent", {1, Component::Service}},
              {"onHandleWork", {1, Component::Service}},
              {"onNewIntent", {1, Component::Activity}},
              {"onReceive", {2, Component::BroadcastReceiver}}};

  return intent_receiving_method_names;
}

const std::unordered_map<std::string_view, ParameterPosition>&
get_intent_class_setters() {
  static const std::unordered_map<std::string_view, ParameterPosition> intent_class_setters = {
      // Apis with java.lang.Class based reflection
      {"Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V",
       2},
      {"Landroid/content/Intent;.<init>:(Ljava/lang/String;Landroid/net/Uri;Landroid/content/Context;Ljava/lang/Class;)V",
       4},
      {"Landroid/content/Intent;.setClass:(Landroid/content/Context;Ljava/lang/Class;)Landroid/content/Intent;",
       2},
      {"Landroid/content/ComponentName;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V",
       2},
      {"Landroidx/core/app/JobIntentService;.enqueueWork:(Landroid/content/Context;Ljava/lang/Class;ILandroid/content/Intent;)V",
       1},
      // Apis with java.lang.String based reflection
      {"Landroid/content/ComponentName;.<init>:(Landroid/content/Context;Ljava/lang/String;)V",
       2},
      {"Landroid/content/ComponentName;.<init>:(Ljava/lang/String;Ljava/lang/String;)V",
       2},
      {"Landroid/content/Intent;.setClassName:(Landroid/content/Context;Ljava/lang/String;)Landroid/content/Intent;",
       2},
      {"Landroid/content/Intent;.setClassName:(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
       2}};

  return intent_class_setters;
}

#endif

} // namespace constants
} // namespace marianatrench
