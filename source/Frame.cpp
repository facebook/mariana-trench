/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>
#include "mariana-trench/Constants.h"

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

void Frame::set_origins(const MethodSet& origins) {
  origins_ = origins;
}

void Frame::set_field_origins(const FieldSet& field_origins) {
  field_origins_ = field_origins;
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
    return kind_ == other.kind_ &&
        (is_artificial_source() ? callee_port_.leq(other.callee_port_)
                                : callee_port_ == other.callee_port_) &&
        callee_ == other.callee_ && call_position_ == other.call_position_ &&
        distance_ >= other.distance_ && origins_.leq(other.origins_) &&
        field_origins_.leq(other.field_origins_) &&
        inferred_features_.leq(other.inferred_features_) &&
        locally_inferred_features_.leq(other.locally_inferred_features_) &&
        user_features_.leq(other.user_features_) &&
        via_type_of_ports_.leq(other.via_type_of_ports_) &&
        via_value_of_ports_.leq(other.via_value_of_ports_) &&
        canonical_names_.leq(other.canonical_names_);
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
        distance_ == other.distance_ && origins_ == other.origins_ &&
        field_origins_ == other.field_origins_ &&
        inferred_features_ == other.inferred_features_ &&
        locally_inferred_features_ == other.locally_inferred_features_ &&
        user_features_ == other.user_features_ &&
        via_type_of_ports_ == other.via_type_of_ports_ &&
        via_value_of_ports_ == other.via_value_of_ports_ &&
        canonical_names_ == other.canonical_names_;
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

    if (is_artificial_source()) {
      callee_port_.join_with(other.callee_port_);
    } else {
      mt_assert(callee_port_ == other.callee_port_);
    }

    distance_ = std::min(distance_, other.distance_);
    origins_.join_with(other.origins_);
    field_origins_.join_with(other.field_origins_);
    inferred_features_.join_with(other.inferred_features_);
    locally_inferred_features_.join_with(other.locally_inferred_features_);
    user_features_.join_with(other.user_features_);
    via_type_of_ports_.join_with(other.via_type_of_ports_);
    via_value_of_ports_.join_with(other.via_value_of_ports_);
    canonical_names_.join_with(other.canonical_names_);
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

void Frame::callee_port_append(Path::Element path_element) {
  callee_port_.append(path_element);
}

Frame Frame::with_kind(const Kind* kind) const {
  Frame new_frame(*this);
  new_frame.kind_ = kind;
  return new_frame;
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

  return value;
}

bool Frame::GroupEqual::operator()(const Frame& left, const Frame& right)
    const {
  return left.kind() == right.kind() &&
      (left.is_artificial_source()
           ? left.callee_port().root() == right.callee_port().root()
           : left.callee_port() == right.callee_port()) &&
      left.callee() == right.callee() &&
      left.call_position() == right.call_position();
}

std::size_t Frame::GroupHash::operator()(const Frame& frame) const {
  std::size_t seed = 0;
  boost::hash_combine(seed, frame.kind());
  if (frame.is_artificial_source()) {
    boost::hash_combine(seed, frame.callee_port().root().encode());
  } else {
    boost::hash_combine(seed, std::hash<AccessPath>()(frame.callee_port()));
  }
  boost::hash_combine(seed, frame.callee());
  boost::hash_combine(seed, frame.call_position());
  return seed;
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
  return out << ")";
}

} // namespace marianatrench
