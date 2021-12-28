/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/program_options.hpp>

#include <gtest/gtest_prod.h>

#include <Tool.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class MarianaTrench : public Tool {
 public:
  MarianaTrench();

  void add_options(
      boost::program_options::options_description& options) const override;
  void run(const boost::program_options::variables_map& variables) override;

 private:
  FRIEND_TEST(IntegrationTest, CompareFlows);
  Registry analyze(Context& context);
};

} // namespace marianatrench
