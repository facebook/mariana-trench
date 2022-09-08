/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CallEffects.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

std::string CallEffect::to_string() const {
  switch (kind()) {
    case CallEffect::Kind::CALL_CHAIN:
      return "call-chain";
    default:
      mt_unreachable();
  }
}

AccessPath CallEffect::access_path() const {
  switch (kind()) {
    case CallEffect::Kind::CALL_CHAIN: {
      static const auto call_chain_path = AccessPath{
          Root{Root::Kind::CallEffect},
          Path{DexString::make_string(to_string())}};

      return call_chain_path;
    }

    default:
      mt_unreachable();
  }
}

} // namespace marianatrench

namespace marianatrench {

bool CallEffectsAbstractDomain::leq(
    const CallEffectsAbstractDomain& other) const {
  return map_.leq(other.map_);
}

bool CallEffectsAbstractDomain::equals(
    const CallEffectsAbstractDomain& other) const {
  return map_.equals(other.map_);
}

void CallEffectsAbstractDomain::set_to_bottom() {
  map_.set_to_bottom();
}

void CallEffectsAbstractDomain::set_to_top() {
  map_.set_to_top();
}

void CallEffectsAbstractDomain::join_with(
    const CallEffectsAbstractDomain& other) {
  map_.join_with(other.map_);
}

void CallEffectsAbstractDomain::widen_with(
    const CallEffectsAbstractDomain& other) {
  map_.widen_with(other.map_);
}

void CallEffectsAbstractDomain::meet_with(
    const CallEffectsAbstractDomain& other) {
  map_.meet_with(other.map_);
}

void CallEffectsAbstractDomain::narrow_with(
    const CallEffectsAbstractDomain& other) {
  map_.narrow_with(other.map_);
}

const Taint& CallEffectsAbstractDomain::read(CallEffect effect) const {
  return map_.get(effect.encode());
}

void CallEffectsAbstractDomain::visit(
    std::function<void(const CallEffect&, const Taint&)> visitor) const {
  mt_assert(!is_top());

  for (const auto& [effect, taint] : *this) {
    visitor(effect, taint);
  }
}

void CallEffectsAbstractDomain::write(const CallEffect& effect, Taint value) {
  map_.update(
      effect.encode(), [&](const Taint& taint) { return taint.join(value); });
}

std::ostream& operator<<(
    std::ostream& out,
    const CallEffectsAbstractDomain& effects) {
  if (!effects.is_bottom()) {
    out << "{\n";
    for (const auto& [effect, taint] : effects) {
      out << "    "
          << "CallEffects(" << effect.to_string() << "): " << taint << ",\n";
    }
    out << "  }";
  }

  return out;
}

} // namespace marianatrench
