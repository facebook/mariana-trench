/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/model-generator/RepeatingAlarmSinkGenerator.h>

namespace {

std::unordered_map<std::string, marianatrench::ParameterPosition>
    k_alarm_signatures = {
        {"Landroid/app/AlarmManager;.setRepeating:(IJJLandroid/app/PendingIntent;)V",
         4},
};

} // namespace

namespace marianatrench {

std::vector<Model> RepeatingAlarmSinkGenerator::run(
    const DexStoresVector& /*stores*/) {
  std::vector<Model> models;

  for (const auto& alarm_signature : k_alarm_signatures) {
    // Add sink model.
    const auto* method = methods_.get(alarm_signature.first);
    if (!method) {
      continue;
    }

    auto model = Model(method, context_);
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, alarm_signature.second)),
        generator::sink(
            context_,
            method,
            /* kind */ "RepeatingAlarmSet"));
    models.push_back(model);
  }

  return models;
}

} // namespace marianatrench
