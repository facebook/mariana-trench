/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/OriginFactory.h>

namespace marianatrench {

const MethodOrigin* OriginFactory::method_origin(
    const Method* method,
    const AccessPath* port) const {
  return method_origins_.create(std::make_tuple(method, port), method, port);
}

const FieldOrigin* OriginFactory::field_origin(const Field* field) const {
  return field_origins_.create(field);
}

const CrtexOrigin* OriginFactory::crtex_origin(
    std::string_view canonical_name,
    const AccessPath* port) const {
  const auto* dex_canonical_name = DexString::make_string(canonical_name);
  return crtex_origins_.create(
      std::make_tuple(dex_canonical_name, port), dex_canonical_name, port);
}

const StringOrigin* OriginFactory::string_origin(std::string_view name) const {
  const auto* origin_name = DexString::make_string(name);
  return string_origins_.create(origin_name);
}

const OriginFactory& OriginFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static OriginFactory instance;
  return instance;
}

} // namespace marianatrench
