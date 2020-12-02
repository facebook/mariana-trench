// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <boost/algorithm/string/predicate.hpp>

#include <mariana-trench/Generator.h>

namespace {

std::unordered_map<std::string, marianatrench::ParameterPosition>
    k_alarm_signatures = {
        {"Landroid/app/AlarmManager;.setRepeating:(IJJLandroid/app/PendingIntent;)V",
         4},
};

} // namespace

namespace marianatrench {

class RepeatingAlarmSinkGenerator : public Generator {
 public:
  explicit RepeatingAlarmSinkGenerator(Context& context)
      : Generator("repeating_alarm_sinks", context) {}

  std::vector<Model> run(const DexStoresVector& stores) {
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
};

} // namespace marianatrench
