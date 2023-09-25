/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CallClassIntervalContext.h>
#include <mariana-trench/CollapseDepth.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/OriginFactory.h>
#include <mariana-trench/TaggedRootSet.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

Frame::Frame(const TaintConfig& config)
    : Frame(
          config.kind(),
          config.class_interval_context(),
          config.distance(),
          config.origins(),
          config.inferred_features(),
          config.user_features(),
          config.annotation_features(),
          config.via_type_of_ports(),
          config.via_value_of_ports(),
          config.canonical_names(),
          config.output_paths(),
          config.extra_traces()) {}

void Frame::add_origin(const Method* method, const AccessPath* port) {
  origins_.add(OriginFactory::singleton().method_origin(method, port));
}

void Frame::add_origin(const Field* field) {
  origins_.add(OriginFactory::singleton().field_origin(field));
}

void Frame::add_origin(std::string_view literal) {
  origins_.add(OriginFactory::singleton().string_origin(literal));
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

void Frame::add_extra_trace(ExtraTrace&& extra_trace) {
  extra_traces_.add(std::move(extra_trace));
}

bool Frame::leq(const Frame& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  } else {
    return kind_ == other.kind_ && distance_ >= other.distance_ &&
        class_interval_context_ == other.class_interval_context_ &&
        origins_.leq(other.origins_) &&
        inferred_features_.leq(other.inferred_features_) &&
        user_features_.leq(other.user_features_) &&
        annotation_features_.leq(other.annotation_features_) &&
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
    return kind_ == other.kind_ &&
        class_interval_context_ == other.class_interval_context_ &&
        distance_ == other.distance_ && origins_ == other.origins_ &&
        inferred_features_ == other.inferred_features_ &&
        user_features_ == other.user_features_ &&
        annotation_features_ == other.annotation_features_ &&
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
    mt_assert(class_interval_context_ == other.class_interval_context_);

    distance_ = std::min(distance_, other.distance_);
    origins_.join_with(other.origins_);
    inferred_features_.join_with(other.inferred_features_);
    user_features_.join_with(other.user_features_);
    annotation_features_.join_with(other.annotation_features_);
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
      class_interval_context_,
      propagation_frame.distance_,
      propagation_frame.origins_,
      inferred_features_,
      /* user_features */ FeatureSet::bottom(),
      /* annotation_features */ AnnotationFeatureSet::bottom(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
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
  if (via_type_of_ports_.is_bottom() || via_type_of_ports_.empty()) {
    return features_added;
  }

  // via_type_of_ports apply to leaf/declaration frames and are not propagated.
  mt_assert(distance() == 0);

  // Materialize via_type_of_ports into features and add them to the inferred
  // features
  for (const auto& tagged_root : via_type_of_ports().elements()) {
    if (!tagged_root.root().is_argument() ||
        tagged_root.root().parameter_position() >=
            source_register_types.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_type_of ports of method {}",
          tagged_root,
          callee->show());
      continue;
    }
    const auto* feature = feature_factory->get_via_type_of_feature(
        source_register_types[tagged_root.root().parameter_position()],
        tagged_root.tag());
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
  if (via_value_of_ports_.is_bottom() || via_value_of_ports_.empty()) {
    return features_added;
  }

  // via_value_of_ports apply to leaf/declaration frames and are not propagated.
  mt_assert(distance() == 0);

  // Materialize via_value_of_ports into features and add them to the inferred
  // features
  for (const auto& tagged_root : via_value_of_ports().elements()) {
    if (!tagged_root.root().is_argument() ||
        tagged_root.root().parameter_position() >=
            source_constant_arguments.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_value_of ports of method {}",
          tagged_root,
          callee->show());
      continue;
    }
    const auto* feature = feature_factory->get_via_value_of_feature(
        source_constant_arguments[tagged_root.root().parameter_position()],
        tagged_root.tag());
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
  output_paths_.transform(
      [maximum_collapse_depth](CollapseDepth collapse_depth) {
        return CollapseDepth(
            std::min(collapse_depth.value(), maximum_collapse_depth.value()));
      });
}

Frame Frame::with_origins(OriginSet origins) const {
  auto copy = *this;
  copy.origins_ = std::move(origins);
  return copy;
}

void Frame::clear_annotation_features() {
  annotation_features_.difference_with(annotation_features_);
}

Frame Frame::from_json(
    const Json::Value& value,
    const CallInfo& call_info,
    Context& context) {
  JsonValidation::validate_object(value);

  const auto* kind = Kind::from_json(value, context);

  int distance = 0;
  if (value.isMember("distance")) {
    distance = JsonValidation::integer(value, "distance");
  }

  auto origins = OriginSet::bottom();
  if (value.isMember("origins")) {
    origins = OriginSet::from_json(
        JsonValidation::nonempty_array(value, "origins"), context);
  }

  // `to_json()` does not differentiate between user and inferred features.
  // The call kind from `call_info` can be useful for that.
  // Declaration - JSON came directly from a user-declared model.
  //   These are all user features.
  // CallSite - Should not contain any user features. Inferred features only.
  // Origins - This is tricky. Locally inferred features can result from
  //   propagations along the flow. User features can result from materialized
  //   via-value/type-of features or from a propagated Declaration frame. Since
  //   they cannot be differentiated from the JSON, they are assumed to all be
  //   inferred features. Practically speaking, when constructing from a
  //   non-user-config JSON, these can arguably be considered non-user-declared.
  auto inferred_features = FeatureMayAlwaysSet::bottom();
  auto user_features = FeatureSet();
  auto json_features = FeatureMayAlwaysSet::from_json(
      value,
      context,
      /* check_unexpected_members */ false);
  if (call_info.call_kind().is_declaration()) {
    if (!json_features.is_bottom()) {
      if (!json_features.may().empty()) {
        throw JsonValidationError(
            value,
            /* field */ "may_features",
            /* expected */
            "empty may_features when CallKind is Declaration");
      }
      user_features = json_features.always();
    }
  } else {
    inferred_features = std::move(json_features);
  }

  auto via_type_of_ports = TaggedRootSet();
  if (value.isMember("via_type_of")) {
    auto via_type_of_json =
        JsonValidation::nonempty_array(value, "via_type_of");
    for (const auto& element : via_type_of_json) {
      via_type_of_ports.add(TaggedRoot::from_json(element));
    }
  }

  auto via_value_of_ports = TaggedRootSet();
  if (value.isMember("via_value_of")) {
    auto via_value_of_json =
        JsonValidation::nonempty_array(value, "via_value_of");
    for (const auto& element : via_value_of_json) {
      via_value_of_ports.add(TaggedRoot::from_json(element));
    }
  }

  auto canonical_names = CanonicalNameSetAbstractDomain();
  if (value.isMember("canonical_names")) {
    auto canonical_names_json =
        JsonValidation::nonempty_array(value, "canonical_names");
    for (const auto& element : canonical_names_json) {
      canonical_names.add(CanonicalName::from_json(element));
    }
  }

  auto output_paths = PathTreeDomain::bottom();
  if (value.isMember("output_paths")) {
    auto output_paths_json = JsonValidation::object(value, "output_paths");
    for (const auto& output_path : output_paths_json.getMemberNames()) {
      auto depth = output_paths_json[output_path].asInt();
      auto path = Path::from_json(output_path);
      output_paths.write(path, CollapseDepth(depth), UpdateKind::Weak);
    }
  }

  auto class_interval_context = CallClassIntervalContext::from_json(value);

  auto extra_traces = ExtraTraceSet();
  if (value.isMember("extra_traces")) {
    auto extra_traces_json =
        JsonValidation::nonempty_array(value, "extra_traces");
    for (const auto& extra_trace_json : extra_traces_json) {
      extra_traces.add(ExtraTrace::from_json(extra_trace_json, context));
    }
  }

  return Frame(
      kind,
      class_interval_context,
      distance,
      origins,
      inferred_features,
      user_features,
      via_type_of_ports,
      via_value_of_ports,
      canonical_names,
      output_paths,
      extra_traces);
}

Json::Value Frame::to_json(
    const CallInfo& call_info,
    ExportOriginsMode export_origins_mode) const {
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
    if (call_info.call_kind().is_origin() ||
        export_origins_mode == ExportOriginsMode::Always) {
      value["origins"] = origins_.to_json();
    }
  }

  // For output purposes, user features and inferred features are not
  // differentiated.
  auto all_features = features();
  JsonValidation::update_object(value, all_features.to_json());

  if (via_type_of_ports_.is_value() && !via_type_of_ports_.elements().empty()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& tagged_root : via_type_of_ports_.elements()) {
      ports.append(tagged_root.to_json());
    }
    value["via_type_of"] = ports;
  }

  if (via_value_of_ports_.is_value() &&
      !via_value_of_ports_.elements().empty()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& tagged_root : via_value_of_ports_.elements()) {
      ports.append(tagged_root.to_json());
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

  auto interval_json = class_interval_context_.to_json();
  for (const auto& member : interval_json.getMemberNames()) {
    value[member] = interval_json[member];
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
  out << "Frame(kind=`" << show(frame.kind_);
  out << "`, class_interval_context=" << show(frame.class_interval_context_);
  if (frame.distance_ != 0) {
    out << ", distance=" << frame.distance_;
  }
  if (!frame.origins_.empty()) {
    out << ", origins=" << frame.origins_;
  }
  if (!frame.inferred_features_.empty()) {
    out << ", inferred_features=" << frame.inferred_features_;
  }
  if (!frame.user_features_.empty()) {
    out << ", user_features=" << frame.user_features_;
  }
  if (frame.via_type_of_ports_.is_value() &&
      !frame.via_type_of_ports_.elements().empty()) {
    out << ", via_type_of_ports=" << frame.via_type_of_ports_.elements();
  }
  if (frame.via_value_of_ports_.is_value() &&
      !frame.via_value_of_ports_.elements().empty()) {
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
