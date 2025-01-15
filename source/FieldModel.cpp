/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FieldModel.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

namespace {

class FieldModelConsistencyError {
 public:
  static void raise(const std::string& what) {
    ERROR(1, "Field Model Consistency Error: {}", what);
    EventLogger::log_event(
        "field_model_consistency_error", what, /* verbosity_level */ 1);
  }
};

} // namespace

FieldModel::FieldModel(
    const Field* field,
    const std::vector<TaintConfig>& sources,
    const std::vector<TaintConfig>& sinks,
    const ModelGeneratorNameSet& model_generators)
    : field_(field), model_generators_(model_generators) {
  for (const auto& config : sources) {
    add_source(config);
  }

  for (const auto& config : sinks) {
    add_sink(config);
  }
}

bool FieldModel::operator==(const FieldModel& other) const {
  return sources_ == other.sources_ && sinks_ == other.sinks_ &&
      model_generators_ == other.model_generators_;
}

bool FieldModel::operator!=(const FieldModel& other) const {
  return !(*this == other);
}

FieldModel FieldModel::instantiate(const Field* field) const {
  FieldModel field_model(field);

  field_model.add_source(sources_);
  field_model.add_sink(sinks_);
  field_model.model_generators_ = model_generators_;

  return field_model;
}

bool FieldModel::empty() const {
  return sources_.is_bottom() && sinks_.is_bottom() &&
      model_generators_.is_bottom();
}

void FieldModel::check_taint_config_consistency(
    const TaintConfig& frame,
    std::string_view kind) const {
  if (frame.kind() == nullptr) {
    FieldModelConsistencyError::raise(fmt::format(
        "Model for field `{}` must have a kind {}.", show(field_), kind));
  }
  if ((frame.callee_port() != nullptr &&
       !frame.callee_port()->root().is_leaf()) ||
      frame.call_position() != nullptr || frame.distance() != 0 ||
      !frame.origins().is_bottom() || frame.via_type_of_ports().size() != 0 ||
      frame.canonical_names().size() != 0) {
    FieldModelConsistencyError::raise(fmt::format(
        "Frame in {}s for field `{}` contains an unexpected non-empty or non-bottom value for a field.",
        show(kind),
        show(field_)));
  }
}

void FieldModel::check_taint_consistency(
    const Taint& taint,
    std::string_view kind) const {
  taint.visit_frames([kind, this](const CallInfo&, const Frame& frame) {
    // If a field_ exists, there should be exactly one origin at the
    // declaration frame..
    const auto* origin = frame.origins().elements().singleton();
    if (field_ && (origin == nullptr || !(*origin)->is<FieldOrigin>())) {
      FieldModelConsistencyError::raise(fmt::format(
          "Model for field `{}` contains a {} without field origins.",
          show(field_),
          kind));
    }
  });
}

void FieldModel::add_source(TaintConfig source) {
  mt_assert(source.is_leaf());
  check_taint_config_consistency(source, "source");
  add_source(Taint{std::move(source)});
}

void FieldModel::add_sink(TaintConfig sink) {
  mt_assert(sink.is_leaf());
  check_taint_config_consistency(sink, "sink");
  add_sink(Taint{std::move(sink)});
}

void FieldModel::join_with(const FieldModel& other) {
  if (this == &other) {
    return;
  }

  mt_if_expensive_assert(auto previous = *this);

  sources_.join_with(other.sources_);
  sinks_.join_with(other.sinks_);
  model_generators_.join_with(other.model_generators_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

FieldModel FieldModel::from_config_json(
    const Field* MT_NULLABLE field,
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value, {"field", "sources", "sinks"});

  FieldModel model(field);

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "sources")) {
    model.add_source(TaintConfig::from_json(source_value, context));
  }
  for (auto sink_value :
       JsonValidation::null_or_array(value, /* field */ "sinks")) {
    model.add_sink(TaintConfig::from_json(sink_value, context));
  }
  return model;
}

Json::Value FieldModel::to_json(ExportOriginsMode export_origins_mode) const {
  auto value = Json::Value(Json::objectValue);

  if (field_) {
    value["field"] = field_->to_json();
  }

  if (!sources_.is_bottom()) {
    auto sources_value = Json::Value(Json::arrayValue);
    sources_.visit_frames([&sources_value, export_origins_mode](
                              const CallInfo& call_info, const Frame& source) {
      mt_assert(!source.is_bottom());
      // Field models do not have local positions/features
      sources_value.append(source.to_json(call_info, export_origins_mode));
    });
    value["sources"] = sources_value;
  }

  if (!sinks_.is_bottom()) {
    auto sinks_value = Json::Value(Json::arrayValue);
    sinks_.visit_frames([&sinks_value, export_origins_mode](
                            const CallInfo& call_info, const Frame& sink) {
      mt_assert(!sink.is_bottom());
      // Field models do not have local positions/features
      sinks_value.append(sink.to_json(call_info, export_origins_mode));
    });
    value["sinks"] = sinks_value;
  }

  if (!model_generators_.is_bottom()) {
    auto model_generators_value = Json::Value(Json::arrayValue);
    for (const auto* model_generator : model_generators_) {
      model_generators_value.append(model_generator->to_json());
    }
    value["model_generators"] = model_generators_value;
  }

  return value;
}

Json::Value FieldModel::to_json(Context& context) const {
  auto value = to_json(context.options->export_origins_mode());
  value["position"] = context.positions->unknown()->to_json();
  return value;
}

std::ostream& operator<<(std::ostream& out, const FieldModel& model) {
  out << "\nFieldModel(field=`" << show(model.field_) << "`";
  if (!model.sources_.is_bottom()) {
    out << ",\n  sources={\n";
    model.sources_.visit_frames(
        [&out](const CallInfo& call_info, const Frame& source) {
          out << "    call_info=" << call_info << ", source=" << source
              << ",\n";
        });
    out << "  }";
  }
  if (!model.sinks_.is_bottom()) {
    out << ",\n  sinks={\n";
    model.sinks_.visit_frames(
        [&out](const CallInfo& call_info, const Frame& sink) {
          out << "    call_info=" << call_info << ", sink=" << sink << ",\n";
        });
    out << "  }";
  }
  if (!model.model_generators_.is_bottom()) {
    out << ",\n  model_generators={";
    for (const auto* model_generator : model.model_generators_) {
      out << *model_generator << ", ";
    }
    out << "}";
  }
  return out << ")";
}

void FieldModel::add_source(Taint source) {
  if (field_) {
    source.add_origins_if_declaration(field_);
  }
  check_taint_consistency(source, "source");
  sources_.join_with(source);
}

void FieldModel::add_sink(Taint sink) {
  if (field_) {
    sink.add_origins_if_declaration(field_);
  }
  check_taint_consistency(sink, "sink");
  sinks_.join_with(sink);
}

void FieldModel::add_model_generator(
    const ModelGeneratorName* model_generator) {
  model_generators_.add(model_generator);
}

void FieldModel::add_model_generator_if_empty(
    const ModelGeneratorName* model_generator) {
  if (!model_generators_.is_bottom()) {
    return;
  }

  model_generators_.add(model_generator);
}

} // namespace marianatrench
