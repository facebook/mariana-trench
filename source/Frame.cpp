/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

void Frame::add_local_position(const Position* position) {
  local_positions_.add(position);
}

void Frame::set_local_positions(LocalPositionSet positions) {
  mt_assert(!positions.is_bottom());
  local_positions_ = std::move(positions);
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
        local_positions_.leq(other.local_positions_) &&
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
        local_positions_ == other.local_positions_ &&
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
    local_positions_.join_with(other.local_positions_);
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

Frame Frame::artificial_source(ParameterPosition parameter_position) {
  return Frame::artificial_source(
      AccessPath(Root(Root::Kind::Argument, parameter_position)));
}

Frame Frame::artificial_source(AccessPath access_path) {
  return Frame(
      /* kind */ Kinds::artificial_source(),
      /* callee_port */ access_path,
      /* callee */ nullptr,
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {});
}

void Frame::callee_port_append(Path::Element path_element) {
  callee_port_.append(path_element);
}

Frame Frame::with_kind(const Kind* kind) const {
  Frame new_frame(*this);
  new_frame.kind_ = kind;
  return new_frame;
}

AccessPath validate_and_infer_crtex_callee_port(
    const Json::Value& value,
    const AccessPath& callee_port,
    const CanonicalNameSetAbstractDomain& canonical_names,
    const RootSetAbstractDomain& via_type_of_ports) {
  mt_assert(canonical_names.is_value() && !canonical_names.elements().empty());

  // Anchor ports only go with templated canonical names. Producer ports only
  // go with instantiated canonical names. No other ports are allowed.
  bool is_templated = false;
  bool is_instantiated = false;
  for (const auto& canonical_name : canonical_names.elements()) {
    if (canonical_name.instantiated_value()) {
      is_instantiated = true;
    } else {
      is_templated = true;
    }
  }

  if (is_instantiated == is_templated) {
    throw JsonValidationError(
        value,
        /* field */ "canonical_names",
        "all instantiated, or all templated values, not mix of both");
  }

  if (is_templated) {
    auto num_via_type_of_ports =
        via_type_of_ports.is_value() ? via_type_of_ports.elements().size() : 0;
    for (const auto& canonical_name : canonical_names.elements()) {
      auto is_via_type_of = canonical_name.is_via_type_of_template();
      if (is_via_type_of && num_via_type_of_ports != 1) {
        throw JsonValidationError(
            value,
            /* field */ std::nullopt,
            "exactly one 'via_type_of' port when canonical name contains 'via_type_of' template");
      }
    }
  }

  // If callee_port is user-specified and not Leaf, validate it.
  if (callee_port.root().is_anchor() && is_instantiated) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        "`Anchor` callee ports to go with templated canonical names.");
  } else if (callee_port.root().is_producer() && is_templated) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        "`Producer` callee ports to go with instantiated canonical names.");
  } else if (!callee_port.root().is_leaf_port()) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        "`Anchor` or `Producer` callee port for crtex frame with canonical_names defined.");
  }

  if (callee_port.root().is_leaf()) {
    if (is_instantiated) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "Instantiated canonical names must have callee_port defined as `Producer.<producer_id>.<canonical_port>`");
    }

    // If the callee_port is defaulted to Leaf, it should be updated to an
    // Anchor to enable detection that this comes from a CRTEX producer.
    return AccessPath(Root(Root::Kind::Anchor));
  }

  return callee_port;
}

Frame Frame::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);

  const Kind* kind = Kind::from_json(value, context);

  auto callee_port = AccessPath(Root(Root::Kind::Leaf));
  if (value.isMember("callee_port")) {
    JsonValidation::string(value, /* field */ "callee_port");
    callee_port = AccessPath::from_json(value["callee_port"]);
  }

  const Method* callee = nullptr;
  if (value.isMember("callee")) {
    callee = Method::from_json(
        JsonValidation::object_or_string(value, /* field */ "callee"), context);
  }

  const Position* call_position = nullptr;
  if (value.isMember("call_position")) {
    call_position = Position::from_json(
        JsonValidation::object(value, /* field */ "call_position"), context);
  }

  int distance = 0;
  if (value.isMember("distance")) {
    distance = JsonValidation::integer(value, /* field */ "distance");
  }

  JsonValidation::null_or_array(value, /* field */ "origins");
  auto origins = MethodSet::from_json(value["origins"], context);

  JsonValidation::null_or_array(value, /* field */ "field_origins");
  auto field_origins = FieldSet::from_json(value["field_origins"], context);

  // Inferred may_features and always_features. Technically, user-specified
  // features should go under "user_features", but this gives a way to override
  // that behavior and specify "may/always" features. Note that local inferred
  // features cannot be user-specified.
  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);

  // User specified always-features.
  FeatureSet user_features;
  if (value.isMember("features")) {
    JsonValidation::null_or_array(value, /* field */ "features");
    user_features = FeatureSet::from_json(value["features"], context);
  }

  RootSetAbstractDomain via_type_of_ports;
  if (value.isMember("via_type_of")) {
    for (const auto& root :
         JsonValidation::null_or_array(value, /* field */ "via_type_of")) {
      via_type_of_ports.add(Root::from_json(root));
    }
  }

  RootSetAbstractDomain via_value_of_ports;
  if (value.isMember(constants::kViaValueOf)) {
    for (const auto& root : JsonValidation::null_or_array(
             value, /* field */ constants::kViaValueOf)) {
      via_value_of_ports.add(Root::from_json(root));
    }
  }

  LocalPositionSet local_positions;
  if (value.isMember("local_positions")) {
    JsonValidation::null_or_array(value, /* field */ "local_positions");
    local_positions =
        LocalPositionSet::from_json(value["local_positions"], context);
  }

  CanonicalNameSetAbstractDomain canonical_names;
  if (value.isMember("canonical_names")) {
    for (const auto& canonical_name :
         JsonValidation::nonempty_array(value, /* field */ "canonical_names")) {
      canonical_names.add(CanonicalName::from_json(canonical_name));
    }
  }

  if (canonical_names.is_value() && !canonical_names.elements().empty()) {
    callee_port = validate_and_infer_crtex_callee_port(
        value, callee_port, canonical_names, via_type_of_ports);
  } else if (
      callee_port.root().is_anchor() || callee_port.root().is_producer()) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        "canonical_names to be specified with `Anchor` or `Producer` callee_port.");
  }

  // Sanity checks.
  if (callee == nullptr) {
    if (!callee_port.root().is_leaf_port()) {
      throw JsonValidationError(
          value,
          /* field */ "callee_port",
          /* expected */ "`Leaf`, `Anchor` or `Producer`");
    } else if (call_position != nullptr) {
      throw JsonValidationError(
          value,
          /* field */ "call_position",
          /* expected */ "unspecified position for leaf taint");
    } else if (distance != 0) {
      throw JsonValidationError(
          value,
          /* field */ "distance",
          /* expected */ "a value of 0");
    }
  } else {
    if (callee_port.root().is_leaf_port()) {
      throw JsonValidationError(
          value,
          /* field */ "callee_port",
          /* expected */ "`Argument(x)` or `Return`");
    } else if (call_position == nullptr) {
      throw JsonValidationError(
          value,
          /* field */ "call_position",
          /* expected */ "non-null position");
    } else if (distance == 0) {
      throw JsonValidationError(
          value,
          /* field */ "distance",
          /* expected */ "non-zero distance");
    }
  }

  return Frame(
      kind,
      std::move(callee_port),
      callee,
      /* field_callee */ nullptr, // A field callee can never be set from a json
                                  // model generator
      call_position,
      distance,
      std::move(origins),
      std::move(field_origins),
      std::move(inferred_features),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      std::move(user_features),
      std::move(via_type_of_ports),
      std::move(via_value_of_ports),
      std::move(local_positions),
      std::move(canonical_names));
}

Json::Value Frame::to_json() const {
  auto value = Json::Value(Json::objectValue);

  mt_assert(kind_ != nullptr);
  auto kind_json = kind_->to_json();
  mt_assert(kind_json.isObject());
  for (const auto& member : kind_json.getMemberNames()) {
    value[member] = kind_json[member];
  }

  value["callee_port"] = callee_port_.to_json();

  if (callee_ != nullptr) {
    value["callee"] = callee_->to_json();
  } else if (field_callee_ != nullptr) {
    value["field_callee"] = field_callee_->to_json();
  }

  if (call_position_ != nullptr) {
    value["call_position"] = call_position_->to_json();
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

  if (!locally_inferred_features_.is_bottom() &&
      !locally_inferred_features_.empty()) {
    value["local_features"] = locally_inferred_features_.to_json();
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
    value[constants::kViaValueOf] = ports;
  }

  if (local_positions_.is_value() && !local_positions_.empty()) {
    value["local_positions"] = local_positions_.to_json();
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
  if (!frame.local_positions_.empty()) {
    out << ", local_positions=" << frame.local_positions_;
  }
  if (frame.canonical_names_.is_value() &&
      !frame.canonical_names_.elements().empty()) {
    out << ", canonical_names=" << frame.canonical_names_;
  }
  return out << ")";
}

} // namespace marianatrench
