/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AccessPathFactory.h>

namespace marianatrench {

const AccessPath* AccessPathFactory::get(const AccessPath& access_path) const {
  return access_paths_.insert(access_path).first;
}

const AccessPathFactory& AccessPathFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static AccessPathFactory instance;
  return instance;
}

} // namespace marianatrench
