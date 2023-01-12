/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <RedexContext.h>
#include <mariana-trench/Assert.h>

namespace marianatrench {

/**
 * RedexContexts are maintained via a single raw pointer called `g_redex`. This
 * class is an RAII object which manages the lifetime there and
 * prevents use-after-free.
 */
class GlobalRedexContext {
 public:
  explicit GlobalRedexContext(bool allow_class_duplicates) {
    this->redex_context_ =
        std::make_unique<RedexContext>(allow_class_duplicates);
    mt_assert(g_redex == nullptr);
    g_redex = this->redex_context_.get();
  }

  ~GlobalRedexContext() {
    g_redex = nullptr;
  }

 private:
  std::unique_ptr<RedexContext> redex_context_;
};

} // namespace marianatrench
