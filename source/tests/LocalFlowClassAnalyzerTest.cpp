/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexStore.h>
#include <Show.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;
using namespace marianatrench::local_flow;

namespace {

class LocalFlowClassAnalyzerTest : public test::Test {};

Context make_class_analyzer_context(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.options->analysis_mode(), context.stores);
  context.class_intervals = std::make_unique<ClassIntervals>(
      *context.options, context.options->analysis_mode(), context.stores);
  return context;
}

// Helper: find a class result by name
const LocalFlowClassResult* find_class_result(
    const std::vector<LocalFlowClassResult>& results,
    const std::string& class_name) {
  for (const auto& r : results) {
    if (r.class_name == class_name) {
      return &r;
    }
  }
  return nullptr;
}

// Helper: check if a dispatch edge exists with given source/target patterns
bool has_dispatch_edge(
    const LocalFlowClassResult& result,
    const std::string& src_pattern,
    const std::string& tgt_pattern) {
  for (const auto& edge : result.dispatch_edges) {
    if (edge.source.to_string().find(src_pattern) != std::string::npos &&
        edge.target.to_string().find(tgt_pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Helper: check if a dispatch edge exists with given source/target/label
bool has_dispatch_edge_with_label(
    const LocalFlowClassResult& result,
    const std::string& src_pattern,
    const std::string& tgt_pattern,
    const std::string& label_pattern) {
  for (const auto& edge : result.dispatch_edges) {
    if (edge.source.to_string().find(src_pattern) != std::string::npos &&
        edge.target.to_string().find(tgt_pattern) != std::string::npos) {
      for (const auto& label : edge.labels) {
        if (label.to_string().find(label_pattern) != std::string::npos) {
          return true;
        }
      }
      // Also match empty label patterns
      if (label_pattern.empty() && edge.labels.empty()) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

TEST_F(LocalFlowClassAnalyzerTest, SimpleClassWithOneMethod) {
  Scope scope;

  marianatrench::redex::create_void_method(scope, "LSimple;", "doSomething");

  auto context = make_class_analyzer_context(scope);
  auto results = LocalFlowClassAnalyzer::analyze_classes(context);

  const auto* result = find_class_result(results, "LSimple;");
  ASSERT_NE(result, nullptr);

  // Meth -> CVar edge (format: M{...} -> C{...})
  EXPECT_TRUE(has_dispatch_edge(*result, "M{", "C{"));

  // CVar -> OVar edge (format: C{...} -> O{...})
  EXPECT_TRUE(has_dispatch_edge(*result, "C{", "O{"));
}

TEST_F(LocalFlowClassAnalyzerTest, ClassWithSuperclassHasOVarEdge) {
  Scope scope;

  auto* dex_base =
      marianatrench::redex::create_void_method(scope, "LBase;", "method");

  marianatrench::redex::create_void_method(
      scope,
      "LDerived;",
      "method",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_base->get_class());

  auto context = make_class_analyzer_context(scope);
  auto results = LocalFlowClassAnalyzer::analyze_classes(context);

  const auto* derived_result = find_class_result(results, "LDerived;");
  ASSERT_NE(derived_result, nullptr);

  // Derived should have self edges but no O->O override edge (moved to parent)
  EXPECT_TRUE(has_dispatch_edge(*derived_result, "M{", "C{"));
  EXPECT_TRUE(has_dispatch_edge(*derived_result, "C{", "O{"));
  EXPECT_FALSE(
      has_dispatch_edge_with_label(*derived_result, "O{", "O{", "Interval:"));

  // OVar(LDerived;) -> OVar(LBase;) with Interval label is in Base's entry
  // (parent-centric model: override edges stored in the parent's Class entry)
  const auto* base_result = find_class_result(results, "LBase;");
  ASSERT_NE(base_result, nullptr);
  EXPECT_TRUE(
      has_dispatch_edge_with_label(*base_result, "O{", "O{", "Interval:"));
}

TEST_F(LocalFlowClassAnalyzerTest, InterfaceClassIsSkipped) {
  Scope scope;

  // Create an interface (using create_class with interface flag)
  // Note: create_void_method creates a class, but we need an interface.
  // We can simulate by creating the interface separately then a class
  // implementing it.
  marianatrench::redex::create_void_method(
      scope, "LMyInterface;", "interfaceMethod");

  // Make it an interface by setting access flags
  auto* intf_class = type_class(DexType::get_type("LMyInterface;"));
  if (intf_class != nullptr) {
    intf_class->set_access(
        intf_class->get_access() | ACC_INTERFACE | ACC_ABSTRACT);
  }

  auto context = make_class_analyzer_context(scope);
  auto results = LocalFlowClassAnalyzer::analyze_classes(context);

  // Interface should not appear as a class result
  const auto* result = find_class_result(results, "LMyInterface;");
  EXPECT_EQ(result, nullptr);
}

TEST_F(LocalFlowClassAnalyzerTest, ClassNoConcreteMethodsIsSkipped) {
  Scope scope;

  // Create a class with only static methods
  marianatrench::redex::create_void_method(
      scope,
      "LStaticOnly;",
      "staticMethod",
      "",
      "V",
      nullptr,
      /* is_static */ true);

  auto context = make_class_analyzer_context(scope);
  auto results = LocalFlowClassAnalyzer::analyze_classes(context);

  // Class with only static methods has no concrete instance methods
  const auto* result = find_class_result(results, "LStaticOnly;");
  EXPECT_EQ(result, nullptr);
}
