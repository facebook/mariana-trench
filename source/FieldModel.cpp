/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/FieldModel.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

namespace {

class FieldModelConsistencyError {
 public:
  static void raise(const std::string& what) {
    ERROR(1, "Field Model Consistency Error: {}", what);
  }
};

} // namespace

FieldModel::FieldModel(
    const Field* field,
    const std::vector<Frame>& sources,
    const std::vector<Frame>& sinks)
    : field_(field) {
  for (const auto& frame : sources) {
    add_source(frame);
  }

  for (const auto& frame : sinks) {
    add_sink(frame);
  }
}

bool FieldModel::operator==(const FieldModel& other) const {
  return sources_ == other.sources_ && sinks_ == other.sinks_;
}

bool FieldModel::operator!=(const FieldModel& other) const {
  return !(*this == other);
}

FieldModel FieldModel::instantiate(const Field* field) const {
  FieldModel field_model(field);

  for (const auto& sources_set : sources_) {
    for (const auto& source : sources_set) {
      field_model.add_source(source);
    }
  }

  for (const auto& sinks_set : sinks_) {
    for (const auto& sink : sinks_set) {
      field_model.add_sink(sink);
    }
  }
  return field_model;
}

bool FieldModel::empty() const {
  return sources_.is_bottom() && sinks_.is_bottom();
}

void FieldModel::check_frame_consistency(
    const Frame& frame,
    std::string_view kind) const {
  if (frame.is_bottom()) {
    FieldModelConsistencyError::raise(fmt::format(
        "Model for field `{}` contains a bottom {}.", show(field_), kind));
  }
  if (frame.is_artificial_source()) {
    FieldModelConsistencyError::raise(fmt::format(
        "Model for field `{}` contains an artificial {}.", show(field_), kind));
  }
  if (field_ && frame.field_origins().empty()) {
    FieldModelConsistencyError::raise(fmt::format(
        "Model for field `{}` contains a {} without field origins.",
        show(field_),
        kind));
  }
  if (!frame.callee_port().root().is_leaf() ||
      frame.call_position() != nullptr || frame.distance() != 0 ||
      !frame.origins().is_bottom() || frame.via_type_of_ports().size() != 0 ||
      !frame.local_positions().empty() || frame.canonical_names().size() != 0) {
    FieldModelConsistencyError::raise(fmt::format(
        "Frame {} in {}s for field `{}` contains an unexpected non-empty or non-bottom value for a field.",
        show(frame),
        show(kind),
        show(field_)));
  }
}

namespace {

Frame add_field_callee(const Field* MT_NULLABLE field, const Frame& frame) {
  if (field == nullptr) {
    return frame;
  }
  return Frame(
      frame.kind(),
      frame.callee_port(),
      frame.callee(),
      field,
      frame.call_position(),
      frame.distance(),
      frame.origins(),
      frame.field_origins(),
      frame.inferred_features(),
      frame.locally_inferred_features(),
      frame.user_features(),
      frame.via_type_of_ports(),
      frame.local_positions(),
      frame.canonical_names());
}

} // namespace

void FieldModel::add_source(Frame source) {
  mt_assert(source.is_leaf());
  if (field_ && source.field_origins().empty()) {
    source.set_field_origins(FieldSet{field_});
  }
  check_frame_consistency(source, "source");
  sources_.add(add_field_callee(field_, source));
}

void FieldModel::add_sink(Frame sink) {
  mt_assert(sink.is_leaf());
  if (field_ && sink.field_origins().empty()) {
    sink.set_field_origins(FieldSet{field_});
  }
  check_frame_consistency(sink, "sink");
  sinks_.add(add_field_callee(field_, sink));
}

void FieldModel::join_with(const FieldModel& other) {
  if (this == &other) {
    return;
  }

  mt_if_expensive_assert(auto previous = *this);

  sources_.join_with(other.sources_);
  sinks_.join_with(other.sinks_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

FieldModel FieldModel::from_json(
    const Field* MT_NULLABLE field,
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  FieldModel model(field);

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "sources")) {
    model.add_source(Frame::from_json(source_value, context));
  }
  for (auto sink_value :
       JsonValidation::null_or_array(value, /* field */ "sinks")) {
    model.add_sink(Frame::from_json(sink_value, context));
  }
  return model;
}

Json::Value FieldModel::to_json() const {
  auto value = Json::Value(Json::objectValue);

  if (field_) {
    value["field"] = field_->to_json();
  }

  if (!sources_.is_bottom()) {
    auto sources_value = Json::Value(Json::arrayValue);
    for (const auto& sources : sources_) {
      for (const auto& source : sources) {
        mt_assert(!source.is_bottom());
        sources_value.append(source.to_json());
      }
    }
    value["sources"] = sources_value;
  }

  if (!sinks_.is_bottom()) {
    auto sinks_value = Json::Value(Json::arrayValue);
    for (const auto& sinks : sinks_) {
      for (const auto& sink : sinks) {
        mt_assert(!sink.is_bottom());
        sinks_value.append(sink.to_json());
      }
    }
    value["sinks"] = sinks_value;
  }

  return value;
}

Json::Value FieldModel::to_json(Context& context) const {
  auto value = to_json();
  value["position"] = context.positions->unknown()->to_json();
  return value;
}

std::ostream& operator<<(std::ostream& out, const FieldModel& model) {
  out << "\nFieldModel(field=`" << show(model.field_) << "`";
  if (!model.sources_.is_bottom()) {
    out << ",\n  sources={\n";
    for (const auto& sources : model.sources_) {
      for (const auto& source : sources) {
        out << "    " << source << ",\n";
      }
    }
    out << "  }";
  }
  if (!model.sinks_.is_bottom()) {
    out << ",\n  sinks={\n";
    for (const auto& sinks : model.sinks_) {
      for (const auto& sink : sinks) {
        out << "    " << sink << ",\n";
      }
    }
    out << "  }";
  }
  return out << ")";
}

} // namespace marianatrench
