/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

Frame::Frame(const TaintConfig& config)
    : Frame(
          config.kind(),
          AccessPathFactory::singleton().get(config.callee_port()),
          config.callee(),
          config.call_position(),
          config.class_interval_context(),
          config.distance(),
          config.origins(),
          config.field_origins(),
          config.inferred_features(),
          config.user_features(),
          config.via_type_of_ports(),
          config.via_value_of_ports(),
          config.canonical_names(),
          config.call_kind(),
          config.output_paths(),
          config.extra_traces()) {}

void Frame::set_origins(const MethodSet& origins) {
  origins_ = origins;
}

void Frame::set_field_origins(const FieldSet& field_origins) {
  field_origins_ = field_origins;
}

void Frame::add_inferred_features(const FeatureMayAlwaysSet& features) {
  inferred_features_.add(features);
}

void Frame::add_user_features(const FeatureSet& features) {
  user_features_.join_with(features);
}

FeatureMayAlwaysSet Frame::features() const {
  auto features = inferred_features_;

  if (features.is_bottom()) {
    return FeatureMayAlwaysSet::make_always(user_features_);
  }

  features.add_always(user_features_);
  mt_assert(!features.is_bottom());
  return features;
}

void Frame::add_extra_trace(const Frame& propagation_frame) {
  if (call_kind_.is_propagation_without_trace()) {
    // These should be added as the next hop of the trace.
    return;
  }
  extra_traces_.add(ExtraTrace(
      propagation_frame.kind_,
      propagation_frame.callee_,
      propagation_frame.call_position_,
      propagation_frame.callee_port_,
      propagation_frame.call_kind_));
}

bool Frame::leq(const Frame& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  } else {
    return kind_ == other.kind_ && callee_port_ == other.callee_port_ &&
        callee_ == other.callee_ && call_position_ == other.call_position_ &&
        call_kind_ == other.call_kind_ && distance_ >= other.distance_ &&
        class_interval_context_ == other.class_interval_context_ &&
        origins_.leq(other.origins_) &&
        field_origins_.leq(other.field_origins_) &&
        inferred_features_.leq(other.inferred_features_) &&
        user_features_.leq(other.user_features_) &&
        via_type_of_ports_.leq(other.via_type_of_ports_) &&
        via_value_of_ports_.leq(other.via_value_of_ports_) &&
        canonical_names_.leq(other.canonical_names_) &&
        output_paths_.leq(other.output_paths_) &&
        extra_traces_.leq(other.extra_traces_);
  }
}

bool Frame::equals(const Frame& other) const {
  if (is_bottom()) {
    return other.is_bottom();
  } else if (other.is_bottom()) {
    return false;
  } else {
    return kind_ == other.kind_ && callee_port_ == other.callee_port_ &&
        callee_ == other.callee_ && call_position_ == other.call_position_ &&
        call_kind_ == other.call_kind_ &&
        class_interval_context_ == other.class_interval_context_ &&
        distance_ == other.distance_ && origins_ == other.origins_ &&
        field_origins_ == other.field_origins_ &&
        inferred_features_ == other.inferred_features_ &&
        user_features_ == other.user_features_ &&
        via_type_of_ports_ == other.via_type_of_ports_ &&
        via_value_of_ports_ == other.via_value_of_ports_ &&
        canonical_names_ == other.canonical_names_ &&
        output_paths_ == other.output_paths_ &&
        extra_traces_ == other.extra_traces_;
  }
}

void Frame::join_with(const Frame& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    mt_assert(kind_ == other.kind_);
    mt_assert(callee_ == other.callee_);
    mt_assert(call_position_ == other.call_position_);
    mt_assert(callee_port_ == other.callee_port_);
    mt_assert(call_kind_ == other.call_kind_);
    mt_assert(class_interval_context_ == other.class_interval_context_);

    distance_ = std::min(distance_, other.distance_);
    origins_.join_with(other.origins_);
    field_origins_.join_with(other.field_origins_);
    inferred_features_.join_with(other.inferred_features_);
    user_features_.join_with(other.user_features_);
    via_type_of_ports_.join_with(other.via_type_of_ports_);
    via_value_of_ports_.join_with(other.via_value_of_ports_);
    canonical_names_.join_with(other.canonical_names_);

    output_paths_.join_with(other.output_paths_);
    // Approximate the output paths here to avoid storing very large trees
    // during the analysis of a method.
    output_paths_.collapse_deeper_than(
        Heuristics::kPropagationMaxOutputPathSize);
    output_paths_.limit_leaves(Heuristics::kPropagationMaxOutputPathLeaves);

    extra_traces_.join_with(other.extra_traces_);
  }

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void Frame::widen_with(const Frame& other) {
  join_with(other);
}

void Frame::meet_with(const Frame& /*other*/) {
  // Not implemented.
}

void Frame::narrow_with(const Frame& other) {
  meet_with(other);
}

const PropagationKind* Frame::propagation_kind() const {
  mt_assert(kind_ != nullptr);
  const auto* propagation_kind =
      kind_->discard_transforms()->as<PropagationKind>();
  mt_assert(propagation_kind != nullptr);
  return propagation_kind;
}

Frame Frame::with_kind(const Kind* kind) const {
  Frame new_frame(*this);
  new_frame.kind_ = kind;
  return new_frame;
}

Frame Frame::update_with_propagation_trace(
    const Frame& propagation_frame) const {
  return Frame(
      kind_,
      propagation_frame.callee_port_,
      propagation_frame.callee_,
      propagation_frame.call_position_,
      class_interval_context_,
      propagation_frame.distance_,
      propagation_frame.origins_,
      field_origins_,
      inferred_features_,
      /* user_features */ FeatureSet::bottom(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      propagation_frame.call_kind_,
      output_paths_,
      extra_traces_);
}

Frame Frame::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  const Kind* base_kind = kind_;
  const TransformList* global_transforms = nullptr;

  if (const auto* transform_kind = kind_->as<TransformKind>()) {
    // If the current kind is already a TransformKind, append existing
    // local_transforms.
    local_transforms = transforms_factory.concat(
        local_transforms, transform_kind->local_transforms());
    global_transforms = transform_kind->global_transforms();
    base_kind = transform_kind->base_kind();
  } else if (kind_->is<PropagationKind>()) {
    // If the current kind is PropagationKind, set the transform as a global
    // transform. This is done to track the next hops for propagation with
    // trace.
    global_transforms = local_transforms;
    local_transforms = nullptr;
  }

  mt_assert(base_kind != nullptr);
  const auto* new_kind = kind_factory.transform_kind(
      base_kind, local_transforms, global_transforms);

  if (!used_kinds.should_keep(new_kind)) {
    return Frame::bottom();
  }

  Frame new_frame{*this};
  new_frame.kind_ = new_kind;
  return new_frame;
}

std::vector<const Feature*> Frame::materialize_via_type_of_ports(
    const Method* callee,
    const FeatureFactory* feature_factory,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types)
    const {
  std::vector<const Feature*> features_added;
  if (via_type_of_ports().is_bottom()) {
    return features_added;
  }

  // via_type_of_ports apply to leaf/declaration frames and are not propagated.
  mt_assert(distance() == 0);

  // Materialize via_type_of_ports into features and add them to the inferred
  // features
  for (const auto& port : via_type_of_ports()) {
    if (!port.is_argument() ||
        port.parameter_position() >= source_register_types.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_type_of ports of method {}",
          port,
          callee->show());
      continue;
    }
    const auto* feature = feature_factory->get_via_type_of_feature(
        source_register_types[port.parameter_position()]);
    features_added.push_back(feature);
  }
  return features_added;
}

std::vector<const Feature*> Frame::materialize_via_value_of_ports(
    const Method* callee,
    const FeatureFactory* feature_factory,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  std::vector<const Feature*> features_added;
  if (via_value_of_ports().is_bottom()) {
    return features_added;
  }

  // via_value_of_ports apply to leaf/declaration frames and are not propagated.
  mt_assert(distance() == 0);

  // Materialize via_value_of_ports into features and add them to the inferred
  // features
  for (const auto& port : via_value_of_ports().elements()) {
    if (!port.is_argument() ||
        port.parameter_position() >= source_constant_arguments.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_value_of ports of method {}",
          port,
          callee->show());
      continue;
    }
    const auto* feature = feature_factory->get_via_value_of_feature(
        source_constant_arguments[port.parameter_position()]);
    features_added.push_back(feature);
  }
  return features_added;
}

void Frame::append_to_propagation_output_paths(Path::Element path_element) {
  PathTreeDomain new_output_paths;
  for (const auto& [path, collapse_depth] : output_paths_.elements()) {
    if (collapse_depth.is_zero()) {
      new_output_paths.write(path, collapse_depth, UpdateKind::Weak);
    } else {
      auto new_collapse_depth = CollapseDepth(collapse_depth.value() - 1);
      auto new_path = path;
      new_path.append(path_element);
      new_output_paths.write(new_path, new_collapse_depth, UpdateKind::Weak);
    }
  }
  output_paths_ = std::move(new_output_paths);
  output_paths_.collapse_deeper_than(Heuristics::kPropagationMaxOutputPathSize);
  output_paths_.limit_leaves(Heuristics::kPropagationMaxOutputPathLeaves);
}

void Frame::update_maximum_collapse_depth(
    CollapseDepth maximum_collapse_depth) {
  output_paths_.map([maximum_collapse_depth](CollapseDepth collapse_depth) {
    return CollapseDepth(
        std::min(collapse_depth.value(), maximum_collapse_depth.value()));
  });
}

Json::Value Frame::to_json(ExportOriginsMode export_origins_mode) const {
  auto value = Json::Value(Json::objectValue);

  mt_assert(kind_ != nullptr);
  auto kind_json = kind_->to_json();
  mt_assert(kind_json.isObject());
  for (const auto& member : kind_json.getMemberNames()) {
    value[member] = kind_json[member];
  }

  if (distance_ != 0) {
    value["distance"] = Json::Value(distance_);
  }

  if (!origins_.empty()) {
    if (call_kind_.is_origin() ||
        export_origins_mode == ExportOriginsMode::Always) {
      value["origins"] = origins_.to_json();
    }
  }

  if (!field_origins_.empty()) {
    value["field_origins"] = field_origins_.to_json();
  }

  // For output purposes, user features and inferred features are not
  // differentiated.
  auto all_features = features();
  JsonValidation::update_object(value, all_features.to_json());

  if (!via_type_of_ports_.is_bottom()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& root : via_type_of_ports_) {
      ports.append(root.to_json());
    }
    value["via_type_of"] = ports;
  }

  if (!via_value_of_ports_.is_bottom()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& root : via_value_of_ports_) {
      ports.append(root.to_json());
    }
    value["via_value_of"] = ports;
  }

  if (canonical_names_.is_value() && !canonical_names_.elements().empty()) {
    auto canonical_names = Json::Value(Json::arrayValue);
    for (const auto& canonical_name : canonical_names_.elements()) {
      canonical_names.append(canonical_name.to_json());
    }
    value["canonical_names"] = canonical_names;
  }

  // This was originally called `call_info` and was renamed `call_kind`.
  // We keep this for backward compatibility.
  value["call_info"] = Json::Value(std::string(call_kind_.to_trace_string()));

  if (!output_paths_.is_bottom()) {
    auto output_paths_value = Json::Value(Json::objectValue);
    for (const auto& [output_path, collapse_depth] : output_paths_.elements()) {
      // Convert to int64_t because `Json::Value` represents signed and
      // unsigned integers differently.
      output_paths_value[output_path.to_string()] =
          Json::Value(static_cast<std::int64_t>(collapse_depth.value()));
    }
    value["output_paths"] = output_paths_value;
  }

  if (!class_interval_context_.is_default()) {
    auto interval_json = class_interval_context_.to_json();
    for (const auto& member : interval_json.getMemberNames()) {
      value[member] = interval_json[member];
    }
  }

  if (extra_traces_.is_value() && !extra_traces_.elements().empty()) {
    auto extra_traces = Json::Value(Json::arrayValue);
    for (const auto& extra_trace : extra_traces_.elements()) {
      extra_traces.append(extra_trace.to_json());
    }
    value["extra_traces"] = extra_traces;
  }

  return value;
}

std::ostream& operator<<(std::ostream& out, const Frame& frame) {
  out << "Frame(kind=`" << show(frame.kind_)
      << "`, callee_port=" << show(frame.callee_port_);
  if (frame.callee_ != nullptr) {
    out << ", callee=`" << show(frame.callee_) << "`";
  }
  if (frame.call_position_ != nullptr) {
    out << ", call_position=" << show(frame.call_position_);
  }
  out << ", class_interval_context=" << show(frame.class_interval_context_);
  out << ", call_kind=" << frame.call_kind_.to_trace_string();
  if (frame.distance_ != 0) {
    out << ", distance=" << frame.distance_;
  }
  if (!frame.origins_.empty()) {
    out << ", origins=" << frame.origins_;
  }
  if (!frame.field_origins_.empty()) {
    out << ", field_origins=" << frame.field_origins_;
  }
  if (!frame.inferred_features_.empty()) {
    out << ", inferred_features=" << frame.inferred_features_;
  }
  if (!frame.user_features_.empty()) {
    out << ", user_features=" << frame.user_features_;
  }
  if (!frame.via_type_of_ports_.is_bottom()) {
    out << ", via_type_of_ports=" << frame.via_type_of_ports_.elements();
  }
  if (!frame.via_value_of_ports_.is_bottom()) {
    out << ", via_value_of_ports=" << frame.via_value_of_ports_.elements();
  }
  if (frame.canonical_names_.is_value() &&
      !frame.canonical_names_.elements().empty()) {
    out << ", canonical_names=" << frame.canonical_names_;
  }
  if (!frame.output_paths_.is_bottom()) {
    out << ", output_paths=" << frame.output_paths_;
  }
  if (frame.extra_traces_.is_value() &&
      !frame.extra_traces_.elements().empty()) {
    out << ", extra_traces=" << frame.extra_traces_;
  }
  return out << ")";
}

} // namespace marianatrench
