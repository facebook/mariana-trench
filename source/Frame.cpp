/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>

#include <Show.h>

#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

void Frame::set_origins(const MethodSet& origins) {
  origins_ = origins;
}

void Frame::add_inferred_features(const FeatureMayAlwaysSet& features) {
  inferred_features_.add(features);
}

FeatureMayAlwaysSet Frame::features() const {
  if (inferred_features_.is_bottom()) {
    return FeatureMayAlwaysSet::make_always(user_features_);
  }

  auto features = inferred_features_;
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
        inferred_features_.leq(other.inferred_features_) &&
        user_features_.leq(other.user_features_) &&
        local_positions_.leq(other.local_positions_);
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
        inferred_features_ == other.inferred_features_ &&
        user_features_ == other.user_features_ &&
        local_positions_ == other.local_positions_;
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
    inferred_features_.join_with(other.inferred_features_);
    user_features_.join_with(other.user_features_);
    local_positions_.join_with(other.local_positions_);
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
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ {},
      /* user_features */ {},
      /* local_positions */ {});
}

void Frame::callee_port_append(Path::Element path_element) {
  callee_port_.append(path_element);
}

Frame Frame::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);

  const auto* kind =
      context.kinds->get(JsonValidation::string(value, /* field */ "kind"));

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

  // Inferred may_features and always_features. Technically, user-specified
  // features should go under "user_features", but this gives a way to override
  // that behavior and specify "may/always" features.
  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);

  // User specified always-features.
  FeatureSet user_features;
  if (value.isMember("features")) {
    JsonValidation::null_or_array(value, /* field */ "features");
    user_features = FeatureSet::from_json(value["features"], context);
  }

  LocalPositionSet local_positions;
  if (value.isMember("local_positions")) {
    JsonValidation::null_or_array(value, /* field */ "local_positions");
    local_positions =
        LocalPositionSet::from_json(value["local_positions"], context);
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
      call_position,
      distance,
      std::move(origins),
      std::move(inferred_features),
      std::move(user_features),
      std::move(local_positions));
}

Json::Value Frame::to_json() const {
  auto value = Json::Value(Json::objectValue);

  mt_assert(kind_ != nullptr);
  value["kind"] = kind_->to_json();

  value["callee_port"] = callee_port_.to_json();

  if (callee_ != nullptr) {
    value["callee"] = callee_->to_json();
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

  JsonValidation::update_object(value, features().to_json());

  if (local_positions_.is_value() && !local_positions_.empty()) {
    value["local_positions"] = local_positions_.to_json();
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
  if (!frame.inferred_features_.empty()) {
    out << ", inferred_features=" << frame.inferred_features_;
  }
  if (!frame.user_features_.empty()) {
    out << ", user_features=" << frame.user_features_;
  }
  if (!frame.local_positions_.empty()) {
    out << ", local_positions=" << frame.local_positions_;
  }
  return out << ")";
}

} // namespace marianatrench
