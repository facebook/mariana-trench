/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Constants.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

/**
 * Analyzes Android inter-component communication via Intents.
 *
 * This analyzer identifies four key aspects of Intent-based communication:
 * 1. Send-target: The target component class to send to (e.g., via
 * Intent::setClass)
 *
 * 2. Send-point: The initiation of the inter-component communication via call
 * to send-apis (e.g., startActivity)
 *
 * 3. Receive-api: The API that retrieves the data sent (e.g., getIntent) i.e.
 * the intent used to start the component.
 *
 * 4. Receive-point: A component (i.e. send-target) can receive an intent in the
 * following ways:
 *    - As arguments of an event handler method (e.g.
 * BroadcastReceiver::onReceive(..., Intent i), or Service::onBind(Intent i,
 * ...)).
 *    - Calling receive-apis like getIntent() which returns the intent that
 * started the component. In this case, the caller of the receive-api is the
 * receive-point. Currently we rely on the user-specified propagation from
 * caller's call-effect-intent port to receive-api's return port to connect the
 * flows.
 */
class IntentRoutingAnalyzer final {
 public:
  using SendTargets = std::vector<const DexType*>;

  struct ReceivePoint {
    const Method* method;
    std::optional<Root> root;
    std::optional<Component> component;
  };

  using ReceivePoints = std::vector<ReceivePoint>;

  // Maps methods that initiate ICC to their send-target component classes
  using MethodToSendTargetsMap = ConcurrentMap<const Method*, SendTargets>;
  // Maps target classes to their receive-points (methods that receive intents)
  using TargetClassesToReceivePointsMap =
      ConcurrentMap<const DexType*, ReceivePoints>;

  explicit IntentRoutingAnalyzer() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingAnalyzer)

 public:
  /**
   * Runs the Intent routing analysis on all methods.
   */
  static std::unique_ptr<IntentRoutingAnalyzer>
  run(const Methods& methods, const Types& types, const Options& options);

  /**
   * Identifies potential targets for intent routing based on a caller and
   * callee method.
   *
   * This method "stitches together" send-points and receive-points by:
   * 1. Checking if the callee is a send-point (intent launcher API)
   * 2. Checking if the caller is a method that sets target classes for intents
   * 3. Finding all receive-points in those target classes that can handle the
   * intent
   * 4. Creating parameter mappings to connect the send-point to the
   * receive-points
   */
  InstantiatedShim::FlatSet<ShimTarget> get_intent_routing_targets(
      const Method* original_callee,
      const Method* caller) const;

  /**
   * Returns the map from a method to all their send-target component classes.
   * These are methods that initiate inter-component communication and the
   * classes they target.
   */
  const MethodToSendTargetsMap& method_to_send_targets() const {
    return method_to_send_targets_;
  }

  /**
   * Returns the map of target classes to their receive-points.
   * These are component classes that receive intents and the methods
   * within them that handle those intents.
   */
  const TargetClassesToReceivePointsMap& target_classes_to_receive_points()
      const {
    return target_classes_to_receive_points_;
  }

 private:
  MethodToSendTargetsMap method_to_send_targets_;
  TargetClassesToReceivePointsMap target_classes_to_receive_points_;
};

} // namespace marianatrench
