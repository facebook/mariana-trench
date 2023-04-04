/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/TransformsFactory.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

Context::Context()
    : kind_factory(std::make_unique<KindFactory>()),
      feature_factory(std::make_unique<FeatureFactory>()),
      statistics(std::make_unique<Statistics>()),
      transforms_factory(std::make_unique<TransformsFactory>()) {}

Context::Context(Context&&) noexcept = default;

Context::~Context() = default;

} // namespace marianatrench
