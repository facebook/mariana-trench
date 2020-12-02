// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
