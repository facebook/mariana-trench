/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/OriginFactory.h>

namespace marianatrench {

const MethodOrigin* OriginFactory::method_origin(const Method* method) const {
  return method_origins_.create(method);
}

const FieldOrigin* OriginFactory::field_origin(const Field* field) const {
  return field_origins_.create(field);
}

const OriginFactory& OriginFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static OriginFactory instance;
  return instance;
}

} // namespace marianatrench
