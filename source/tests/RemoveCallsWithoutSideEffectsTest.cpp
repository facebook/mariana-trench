/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <filesystem>

#include <unistd.h>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <DexStore.h>
#include <IRAssembler.h>
#include <ReachableClasses.h>

#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class RemoveCallsWithoutSideEffectsTest : public test::Test {};

std::unique_ptr<Options> make_options(
    const std::vector<std::string>&
        assume_no_side_effects_configuration_paths) {
  return std::make_unique<Options>(
      /* models_paths */ std::vector<std::string>{},
      /* field_models_paths */ std::vector<std::string>{},
      /* literal_models_paths */ std::vector<std::string>{},
      /* rules_paths */ std::vector<std::string>{},
      /* lifecycles_paths */ std::vector<std::string>{},
      /* shims_paths */ std::vector<std::string>{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false,
      /* source_root_directory */ ".",
      /* enable_cross_component_analysis */ false,
      /* export_origins_mode */ ExportOriginsMode::Always,
      /* analysis_mode */ AnalysisMode::Normal,
      /* propagate_across_arguments */ false,
      assume_no_side_effects_configuration_paths);
}

std::string write_configuration(const std::string& contents) {
  auto path = std::filesystem::temp_directory_path() /
      fmt::format("assume_no_side_effects_{}.pro", ::getpid());
  filesystem::save_string_file(path, contents);
  return path.native();
}

constexpr auto k_configuration = R"(
  -assumenosideeffects class Log {
    public static void d(java.lang.String);
    public static int v(java.lang.String);
  }
  -assumenosideeffects class Format {
    public static java.lang.String format(java.lang.String);
    public <init>();
  }
  -keep class Caller { *; }
)";

DexStoresVector create_stores() {
  Scope scope;
  marianatrench::redex::create_methods(
      scope,
      "LLog;",
      {
          R"((method (public static) "LLog;.d:(Ljava/lang/String;)V"
            ((return-void))))",
          R"((method (public static) "LLog;.e:(Ljava/lang/String;)V"
            ((return-void))))",
          R"((method (public static) "LLog;.v:(Ljava/lang/String;)I"
            ((const v0 0) (return v0))))",
      });
  marianatrench::redex::create_methods(
      scope,
      "LFormat;",
      {
          R"((method (public constructor) "LFormat;.<init>:()V"
            ((load-param-object v0) (return-void))))",
          R"((method (public static) "LFormat;.format:(Ljava/lang/String;)Ljava/lang/String;"
            ((load-param-object v0) (return-object v0))))",
      });
  marianatrench::redex::create_methods(
      scope,
      "LCaller;",
      {
          R"((method (public static) "LCaller;.caller:(Ljava/lang/String;)I"
            (
              (load-param-object v0)

              ; Matched, no result: removed.
              (invoke-static (v0) "LLog;.d:(Ljava/lang/String;)V")
              ; Not matched: kept.
              (invoke-static (v0) "LLog;.e:(Ljava/lang/String;)V")
              ; Result discarded: removed.
              (invoke-static (v0) "LLog;.v:(Ljava/lang/String;)I")
              ; Result moved but never read: removed.
              (invoke-static (v0) "LLog;.v:(Ljava/lang/String;)I")
              (move-result v1)
              ; Result only read by a removed call: both removed.
              (invoke-static (v0) "LFormat;.format:(Ljava/lang/String;)Ljava/lang/String;")
              (move-result-object v3)
              (invoke-static (v3) "LLog;.d:(Ljava/lang/String;)V")
              ; Constructors are never removed.
              (new-instance "LFormat;")
              (move-result-pseudo-object v4)
              (invoke-direct (v4) "LFormat;.<init>:()V")
              ; Result read: kept.
              (invoke-static (v0) "LLog;.v:(Ljava/lang/String;)I")
              (move-result v2)
              (return v2)
            )
          ))",
      });

  DexStore store("test_store");
  store.add_classes(scope);
  return {store};
}

std::string code_of(std::string_view method) {
  auto* code = marianatrench::redex::get_method(method)->get_code();
  code->clear_cfg();
  auto result = assembler::to_string(code);
  code->build_cfg();
  return result;
}

} // namespace

TEST_F(RemoveCallsWithoutSideEffectsTest, RemovesCallsWithUnusedResults) {
  auto stores = create_stores();
  auto options = make_options({write_configuration(k_configuration)});
  ControlFlowGraphs control_flow_graphs(stores);

  EXPECT_EQ(
      marianatrench::redex::remove_calls_without_side_effects(*options, stores),
      5);

  auto expected = assembler::ircode_from_string(R"(
    (
      (load-param-object v0)
      (invoke-static (v0) "LLog;.e:(Ljava/lang/String;)V")
      (new-instance "LFormat;")
      (move-result-pseudo-object v4)
      (invoke-direct (v4) "LFormat;.<init>:()V")
      (invoke-static (v0) "LLog;.v:(Ljava/lang/String;)I")
      (move-result v2)
      (return v2)
    )
  )");
  EXPECT_EQ(
      code_of("LCaller;.caller:(Ljava/lang/String;)I"),
      assembler::to_string(expected.get()));

  // Only the `-assumenosideeffects` rules are processed.
  EXPECT_FALSE(root(
      marianatrench::redex::get_method(
          "LCaller;.caller:(Ljava/lang/String;)I")));
}

TEST_F(RemoveCallsWithoutSideEffectsTest, KeepsCallsWithoutConfiguration) {
  auto stores = create_stores();
  auto options = make_options({});
  ControlFlowGraphs control_flow_graphs(stores);
  auto before = code_of("LCaller;.caller:(Ljava/lang/String;)I");

  EXPECT_EQ(
      marianatrench::redex::remove_calls_without_side_effects(*options, stores),
      0);
  EXPECT_EQ(code_of("LCaller;.caller:(Ljava/lang/String;)I"), before);
}
