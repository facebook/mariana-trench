/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/FieldModel.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Model.h>

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
    const std::vector<Frame>& generations,
    const std::vector<Frame>& sinks)
    : field_(field) {
  for (const auto& frame : generations) {
    add_generation(frame);
  }

  for (const auto& frame : sinks) {
    add_sink(frame);
  }
}

bool FieldModel::operator==(const FieldModel& other) const {
  return generations_ == other.generations_ && sinks_ == other.sinks_;
}

bool FieldModel::operator!=(const FieldModel& other) const {
  return !(*this == other);
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
      !frame.origins().is_bottom() || !frame.via_type_of_ports().is_bottom() ||
      !frame.local_positions().is_bottom() ||
      !frame.canonical_names().is_bottom()) {
    FieldModelConsistencyError::raise(fmt::format(
        "Frame {} in {}s for field `{}` contains an unexpected non-empty or non-bottom value for a field.",
        show(frame),
        show(kind),
        show(field_)));
  }
}

void FieldModel::add_generation(Frame source) {
  mt_assert(source.is_leaf());
  if (field_ && source.field_origins().empty()) {
    source.set_field_origins(FieldSet{field_});
  }
  check_frame_consistency(source, "source");
  generations_.add(source);
}

void FieldModel::add_sink(Frame sink) {
  mt_assert(sink.is_leaf());
  if (field_ && sink.field_origins().empty()) {
    sink.set_field_origins(FieldSet{field_});
  }
  check_frame_consistency(sink, "sink");
  sinks_.add(sink);
}

void FieldModel::join_with(const FieldModel& other) {
  if (this == &other) {
    return;
  }

  mt_if_expensive_assert(auto previous = *this);

  generations_.join_with(other.generations_);
  sinks_.join_with(other.sinks_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

FieldModel FieldModel::from_json(
    const Field* MT_NULLABLE field,
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  FieldModel model(field);

  for (auto generation_value :
       JsonValidation::null_or_array(value, /* field */ "generations")) {
    model.add_generation(Frame::from_json(generation_value, context));
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

  if (!generations_.is_bottom()) {
    auto generations_value = Json::Value(Json::arrayValue);
    for (const auto& generations : generations_) {
      for (const auto& generation : generations) {
        mt_assert(!generation.is_bottom());
        generations_value.append(generation.to_json());
      }
    }
    value["generations"] = generations_value;
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

std::ostream& operator<<(std::ostream& out, const FieldModel& model) {
  out << "\nFieldModel(field=`" << show(model.field_) << "`";
  if (!model.generations_.is_bottom()) {
    out << ",\n  generations={\n";
    for (const auto& generations : model.generations_) {
      for (const auto& generation : generations) {
        out << "    " << generation << ",\n";
      }
    }
    out << "  }";
  }
  if (!model.sinks_.is_bottom()) {
    out << ",\n  sinks={\n";
    for (const auto& generations : model.sinks_) {
      for (const auto& generation : generations) {
        out << "    " << generation << ",\n";
      }
    }
    out << "  }";
  }
  return out << ")";
}

} // namespace marianatrench
