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
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

void Frame::set_origins(const MethodSet& origins) {
  origins_ = origins;
}

void Frame::set_field_origins(const FieldSet& field_origins) {
  field_origins_ = field_origins;
}

void Frame::set_field_callee(const Field* field) {
  field_callee_ = field;
}

void Frame::add_inferred_features(const FeatureMayAlwaysSet& features) {
  locally_inferred_features_.add(features);
}

FeatureMayAlwaysSet Frame::features() const {
  auto features = inferred_features_;
  features.add(locally_inferred_features_);

  if (features.is_bottom()) {
    return FeatureMayAlwaysSet::make_always(user_features_);
  }

  features.add_always(user_features_);
  mt_assert(!features.is_bottom());
  return features;
}

bool Frame::leq(const Frame& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  } else {
    return kind_ == other.kind_ && callee_port_ == other.callee_port_ &&
        callee_ == other.callee_ && call_position_ == other.call_position_ &&
        call_info_ == other.call_info_ && distance_ >= other.distance_ &&
        origins_.leq(other.origins_) &&
        field_origins_.leq(other.field_origins_) &&
        inferred_features_.leq(other.inferred_features_) &&
        locally_inferred_features_.leq(other.locally_inferred_features_) &&
        user_features_.leq(other.user_features_) &&
        via_type_of_ports_.leq(other.via_type_of_ports_) &&
        via_value_of_ports_.leq(other.via_value_of_ports_) &&
        canonical_names_.leq(other.canonical_names_) &&
        output_paths_.leq(other.output_paths_);
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
        call_info_ == other.call_info_ && distance_ == other.distance_ &&
        origins_ == other.origins_ && field_origins_ == other.field_origins_ &&
        inferred_features_ == other.inferred_features_ &&
        locally_inferred_features_ == other.locally_inferred_features_ &&
        user_features_ == other.user_features_ &&
        via_type_of_ports_ == other.via_type_of_ports_ &&
        via_value_of_ports_ == other.via_value_of_ports_ &&
        canonical_names_ == other.canonical_names_ &&
        output_paths_ == other.output_paths_;
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
    mt_assert(call_info_ == other.call_info_);

    distance_ = std::min(distance_, other.distance_);
    origins_.join_with(other.origins_);
    field_origins_.join_with(other.field_origins_);
    inferred_features_.join_with(other.inferred_features_);
    locally_inferred_features_.join_with(other.locally_inferred_features_);
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

Frame Frame::apply_transform(
    const KindFactory& kind_factory,
    const Transforms& transforms,
    const UsedKinds& used_kinds,
    AccessPath callee_port,
    const TransformList* local_transforms) const {
  const Kind* base_kind = kind_;
  const TransformList* global_transforms = nullptr;

  if (const auto* transform_kind = kind_->as<TransformKind>()) {
    // If the current kind is already a TransformKind, append existing
    // local_transforms.
    local_transforms =
        transforms.concat(local_transforms, transform_kind->local_transforms());
    global_transforms = transform_kind->global_transforms();
    base_kind = transform_kind->base_kind();
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

void Frame::append_to_propagation_output_paths(Path::Element path_element) {
  PathTreeDomain new_output_paths;
  for (const auto& [path, elements] : output_paths_.elements()) {
    auto new_path = path;
    new_path.append(path_element);
    new_output_paths.write(new_path, elements, UpdateKind::Weak);
  }
  output_paths_ = std::move(new_output_paths);
  output_paths_.collapse_deeper_than(Heuristics::kPropagationMaxOutputPathSize);
  output_paths_.limit_leaves(Heuristics::kPropagationMaxOutputPathLeaves);
}

Json::Value Frame::to_json(const LocalPositionSet& local_positions) const {
  auto value = Json::Value(Json::objectValue);

  mt_assert(kind_ != nullptr);
  auto kind_json = kind_->to_json();
  mt_assert(kind_json.isObject());
  for (const auto& member : kind_json.getMemberNames()) {
    value[member] = kind_json[member];
  }

  if (callee_ == nullptr && field_callee_ != nullptr) {
    value["field_callee"] = field_callee_->to_json();
  }

  if (distance_ != 0) {
    value["distance"] = Json::Value(distance_);
  }

  if (!origins_.empty()) {
    value["origins"] = origins_.to_json();
  }

  if (!field_origins_.empty()) {
    value["field_origins"] = field_origins_.to_json();
  }

  JsonValidation::update_object(value, features().to_json());

  auto all_local_features = locally_inferred_features();
  all_local_features.add_always(user_features_);
  if (!all_local_features.is_bottom() && !all_local_features.empty()) {
    value["local_features"] = all_local_features.to_json();
  }

  if (via_type_of_ports_.is_value() && !via_type_of_ports_.elements().empty()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& root : via_type_of_ports_.elements()) {
      ports.append(root.to_json());
    }
    value["via_type_of"] = ports;
  }

  if (via_value_of_ports_.is_value() &&
      !via_value_of_ports_.elements().empty()) {
    auto ports = Json::Value(Json::arrayValue);
    for (const auto& root : via_value_of_ports_.elements()) {
      ports.append(root.to_json());
    }
    value["via_value_of"] = ports;
  }

  // TODO(T91357916): Emit local positions in Frame json until parser is updated
  // to read from CalleePortFrames.
  if (local_positions.is_value() && !local_positions.empty()) {
    value["local_positions"] = local_positions.to_json();
  }

  if (canonical_names_.is_value() && !canonical_names_.elements().empty()) {
    auto canonical_names = Json::Value(Json::arrayValue);
    for (const auto& canonical_name : canonical_names_.elements()) {
      canonical_names.append(canonical_name.to_json());
    }
    value["canonical_names"] = canonical_names;
  }

  value["call_info"] = Json::Value(std::string(show_call_info(call_info_)));

  if (!output_paths_.is_bottom()) {
    auto output_paths_value = Json::Value(Json::arrayValue);
    for (const auto& [output_path, _] : output_paths_.elements()) {
      output_paths_value.append(output_path.to_json());
    }
    value["output_paths"] = output_paths_value;
  }
  return value;
}

bool Frame::GroupEqual::operator()(const Frame& left, const Frame& right)
    const {
  return left.kind() == right.kind() &&
      left.callee_port() == right.callee_port() &&
      left.callee() == right.callee() &&
      left.call_position() == right.call_position();
}

std::size_t Frame::GroupHash::operator()(const Frame& frame) const {
  std::size_t seed = 0;
  boost::hash_combine(seed, frame.kind());
  boost::hash_combine(seed, std::hash<AccessPath>()(frame.callee_port()));
  boost::hash_combine(seed, frame.callee());
  boost::hash_combine(seed, frame.call_position());
  return seed;
}

const std::string_view show_call_info(CallInfo call_info) {
  switch (call_info) {
    case CallInfo::Declaration:
      return "Declaration";
    case CallInfo::Origin:
      return "Origin";
    case CallInfo::CallSite:
      return "CallSite";
    case CallInfo::Propagation:
      return "Propagation";
    default:
      mt_unreachable();
  }
}

CallInfo propagate_call_info(CallInfo call_info) {
  switch (call_info) {
    case CallInfo::Declaration:
      return CallInfo::Origin;
    case CallInfo::Origin:
      return CallInfo::CallSite;
    case CallInfo::CallSite:
      return CallInfo::CallSite;
    case CallInfo::Propagation:
      mt_unreachable();
    default:
      mt_unreachable();
  }
}

std::ostream& operator<<(std::ostream& out, const Frame& frame) {
  out << "Frame(kind=`" << show(frame.kind_)
      << "`, callee_port=" << frame.callee_port_;
  if (frame.callee_ != nullptr) {
    out << ", callee=`" << show(frame.callee_) << "`";
  } else if (frame.field_callee_ != nullptr) {
    out << ", field_callee=`" << show(frame.field_callee_) << "`";
  }
  if (frame.call_position_ != nullptr) {
    out << ", call_position=" << show(frame.call_position_);
  }
  out << ", call_info=" << show_call_info(frame.call_info_);
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
  if (!frame.locally_inferred_features_.empty()) {
    out << ", locally_inferred_features=" << frame.locally_inferred_features_;
  }
  if (!frame.user_features_.empty()) {
    out << ", user_features=" << frame.user_features_;
  }
  if (frame.via_type_of_ports_.is_value() &&
      !frame.via_type_of_ports_.elements().empty()) {
    out << ", via_type_of_ports=" << frame.via_type_of_ports_;
  }
  if (frame.via_value_of_ports_.is_value() &&
      !frame.via_value_of_ports_.elements().empty()) {
    out << ", via_value_of_ports=" << frame.via_value_of_ports_;
  }
  if (frame.canonical_names_.is_value() &&
      !frame.canonical_names_.elements().empty()) {
    out << ", canonical_names=" << frame.canonical_names_;
  }
  if (!frame.output_paths_.is_bottom()) {
    out << ", output_paths=" << frame.output_paths_;
  }
  return out << ")";
}

} // namespace marianatrench
