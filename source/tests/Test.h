/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <boost/filesystem/path.hpp>
#include <gmock/gmock.h>
#include <json/json.h>

#include <DexStore.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GlobalRedexContext.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/Types.h>

namespace marianatrench {
namespace test {

class Test : public testing::Test {
 public:
  Test();
  virtual ~Test() = default;

 private:
  GlobalRedexContext global_redex_context_;
};

class ContextGuard {
 public:
  ContextGuard();
  ~ContextGuard() = default;

 private:
  GlobalRedexContext global_redex_context_;
};

Context make_empty_context();
Context make_context(const DexStore& store);

struct FrameProperties {
  AccessPath callee_port = AccessPath(Root(Root::Kind::Leaf));
  const Method* MT_NULLABLE callee = nullptr;
  const Field* MT_NULLABLE field_callee = nullptr;
  const Position* MT_NULLABLE call_position = nullptr;
  int distance = 0;
  MethodSet origins = {};
  FieldSet field_origins = {};
  FeatureMayAlwaysSet inferred_features = {};
  FeatureMayAlwaysSet locally_inferred_features = {};
  FeatureSet user_features = {};
  RootSetAbstractDomain via_type_of_ports = {};
  RootSetAbstractDomain via_value_of_ports = {};
  CanonicalNameSetAbstractDomain canonical_names = {};
  PathTreeDomain input_paths = {};
  PathTreeDomain output_paths = {};
  LocalPositionSet local_positions = {};
  CallInfo call_info = CallInfo::Declaration;
};

Frame make_taint_frame(const Kind* kind, const FrameProperties& properties);

TaintConfig make_taint_config(
    const Kind* kind,
    const FrameProperties& properties);
TaintConfig make_leaf_taint_config(const Kind* kind);
TaintConfig make_leaf_taint_config(
    const Kind* kind,
    FeatureMayAlwaysSet inferred_features,
    FeatureMayAlwaysSet locally_inferred_features,
    FeatureSet user_features,
    MethodSet origins);
TaintConfig make_crtex_leaf_taint_config(
    const Kind* kind,
    AccessPath callee_port,
    CanonicalNameSetAbstractDomain canonical_names);

boost::filesystem::path find_repository_root();

Json::Value parse_json(std::string input);
Json::Value sorted_json(const Json::Value& value);

boost::filesystem::path find_dex_path(
    const boost::filesystem::path& test_directory);

std::vector<std::string> sub_directories(
    const boost::filesystem::path& directory);

/**
 * Normalizes input in json-lines form where the json-lines themselves can
 * span multiple lines to make it easier to read the test output.
 */
std::string normalize_json_lines(const std::string& input);

#define MT_INSTANTIATE_TEST_SUITE_P(                                        \
    INSTANTIATION_NAME, TEST_SUITE_NAME, PARAM_GENERATOR)                   \
  INSTANTIATE_TEST_SUITE_P(                                                 \
      INSTANTIATION_NAME,                                                   \
      TEST_SUITE_NAME,                                                      \
      PARAM_GENERATOR,                                                      \
      ([](const ::testing::TestParamInfo<TEST_SUITE_NAME::ParamType>& info) \
           -> std::string {                                                 \
        boost::filesystem::path name = info.param;                          \
        std::string test_name = name.stem().string();                       \
        return test_name;                                                   \
      }));

} // namespace test
} // namespace marianatrench
