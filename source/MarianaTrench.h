/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/program_options.hpp>

#include <Tool.h>

#include <mariana-trench/Context.h>

namespace marianatrench {

class MarianaTrench : public Tool {
 public:
  MarianaTrench();

  void run(const boost::program_options::variables_map& variables) override;

  // Build shared infrastructure needed by all analysis passes:
  // Methods, Fields, Positions.
  static void initialize_context(Context& context);
};

} // namespace marianatrench
