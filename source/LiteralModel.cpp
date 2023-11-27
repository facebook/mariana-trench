/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/LiteralModel.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

namespace {

class LiteralModelConsistencyError {
 public:
  static void raise(const std::string& what) {
    ERROR(1, "Literal Model Consistency Error: {}", what);
    EventLogger::log_event("regex_model_consistency_error", what);
  }
};

} // namespace

static void check_taint_config_consistency(
    const std::string& pattern,
    const TaintConfig& frame) {
  if (frame.kind() == nullptr) {
    LiteralModelConsistencyError::raise(
        fmt::format("Model for regex `{}` must have a source kind.", pattern));
  }
}

static Taint create_taint(
    const std::string& pattern,
    const std::vector<TaintConfig>& sources) {
  Taint all_sources;
  for (const auto& source : sources) {
    mt_assert(source.is_leaf());
    check_taint_config_consistency(pattern, source);
    all_sources.join_with(Taint{source});
  }
  return all_sources;
}

LiteralModel::LiteralModel() {}

LiteralModel::LiteralModel(
    std::string pattern,
    const std::vector<TaintConfig>& sources)
    : regex_(pattern), sources_(create_taint(regex_->pattern(), sources)) {}

LiteralModel::LiteralModel(const LiteralModel& other)
    : regex_(
          other.regex_ ? std::optional(other.regex_->pattern()) : std::nullopt),
      sources_(other.sources_) {}

std::optional<std::string> LiteralModel::pattern() const {
  return regex_ ? regex_->pattern() : std::optional<std::string>();
}

const Taint& LiteralModel::sources() const {
  return sources_;
}

bool LiteralModel::matches(const std::string_view literal) const {
  return regex_ && re2::RE2::FullMatch(literal, *regex_);
}

void LiteralModel::join_with(const LiteralModel& other) {
  if (this == &other) {
    return;
  }

  if (pattern() != other.pattern()) {
    regex_.reset();
  }
  sources_.join_with(other.sources_);
}

bool LiteralModel::empty() const {
  return sources_.is_bottom();
}

LiteralModel LiteralModel::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::check_unexpected_members(
      value, {"pattern", "sources", "description"});

  std::vector<TaintConfig> sources;
  for (auto source :
       JsonValidation::null_or_array(value, /* field */ "sources")) {
    TaintConfig taintConfig{TaintConfig::from_json(source, context)};
    if (!taintConfig.is_leaf())
      throw JsonValidationError{source, /* field */ "sources", "empty callee"};

    sources.push_back(std::move(taintConfig));
  }
  return LiteralModel(JsonValidation::string(value, "pattern"), sources);
}

Json::Value LiteralModel::to_json(
    const ExportOriginsMode export_origins_mode) const {
  auto value = Json::Value(Json::objectValue);

  const std::optional<std::string> pattern_text{pattern()};
  if (pattern_text) {
    value["pattern"] = *pattern_text;
  }

  if (!sources_.is_bottom()) {
    auto sources_value = Json::Value(Json::arrayValue);
    sources_.visit_frames(
        [&sources_value, export_origins_mode](const Frame& source) {
          mt_assert(!source.is_bottom());
          sources_value.append(source.to_json(export_origins_mode));
        });
    value["sources"] = sources_value;
  }

  return value;
}

Json::Value LiteralModel::to_json(Context& context) const {
  auto value = to_json(context.options->export_origins_mode());
  value["position"] = context.positions->unknown()->to_json();
  return value;
}

} // namespace marianatrench
