/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowAnalysis.h>
#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>
#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>

#include <atomic>
#include <unordered_set>

#include <ConcurrentContainers.h>
#include <DexClass.h>

#include <sparta/WorkQueue.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {
namespace local_flow {

namespace {

MethodMetadata extract_metadata(const Method* method) {
  MethodMetadata meta;

  // Parameter types (skip `this` for instance methods)
  for (ParameterPosition i = method->first_parameter_index();
       i < method->number_of_parameters();
       ++i) {
    const auto* param_type = method->parameter_type(i);
    if (param_type != nullptr) {
      meta.param_types.emplace_back(param_type->str());
    }
  }

  // Return type
  meta.return_type = std::string(method->return_type()->str());

  // External status
  const auto* dex_method = method->dex_method();
  meta.is_external = (dex_method != nullptr && dex_method->is_external());

  // Method-level annotations
  if (dex_method != nullptr) {
    const auto* anno_set = dex_method->get_anno_set();
    if (anno_set != nullptr) {
      for (const auto& anno : anno_set->get_annotations()) {
        if (anno->type() != nullptr) {
          meta.annotations.emplace_back(anno->type()->str());
        }
      }
    }
  }

  return meta;
}

} // namespace

void LocalFlowAnalysis::run(
    const Context& context,
    LocalFlowRegistry& registry,
    std::size_t max_structural_depth,
    bool sequential) {
  Timer timer;
  LOG(1, "Running local flow analysis...");

  const auto* positions = context.positions.get();
  const auto* call_graph = context.call_graph.get();
  const auto total_methods = context.methods->size();
  std::atomic<std::size_t> method_count(0);

  auto analyze_method = [&](const Method* method) {
    method_count++;
    if (method_count % 10000 == 0) {
      LOG(1, "Analyzed {}/{} methods.", method_count.load(), total_methods);
    }

    auto result = LocalFlowMethodAnalyzer::analyze(
        method, max_structural_depth, positions, call_graph);
    if (result.has_value()) {
      result->method_metadata = extract_metadata(method);
      registry.set(method, std::move(*result));
    }
  };

  if (sequential) {
    for (const auto* method : *context.methods) {
      analyze_method(method);
    }
  } else {
    auto queue = sparta::work_queue<const Method*>(
        [&](const Method* method) { analyze_method(method); });
    for (const auto* method : *context.methods) {
      queue.add_item(method);
    }
    queue.run_all();
  }

  // Generate TITO stubs for external/framework methods.
  // These allow LFE to traverse through framework method boundaries
  // (Android SDK, java.*, kotlin.*) that have no analyzed code.
  std::atomic<std::size_t> stub_count(0);
  auto generate_stub = [&](const Method* method) {
    auto* dex_method = method->dex_method();
    if (dex_method == nullptr || !dex_method->is_external()) {
      return;
    }
    if (dex_method->get_code() != nullptr) {
      return;
    }

    LocalFlowConstraintSet constraints;
    bool is_static = method->is_static();
    bool returns_void = method->returns_void();

    // Instance methods: this (p0) → this_out
    if (!is_static) {
      constraints.add_edge(
          LocalFlowNode::make_param(0), LocalFlowNode::make_this_out());
    }

    // All parameters → result (for non-void methods)
    if (!returns_void) {
      auto num_params = method->number_of_parameters();
      for (uint32_t i = 0; i < num_params; ++i) {
        constraints.add_edge(
            LocalFlowNode::make_param(i), LocalFlowNode::make_result());
      }
    }

    if (constraints.empty()) {
      return;
    }

    auto stub_result = LocalFlowMethodResult{
        LocalFlowNode::jvm_to_lfe_separator(method->signature()),
        std::move(constraints),
        {},
        "",
        0,
        is_static};
    stub_result.method_metadata = extract_metadata(method);
    registry.set(method, std::move(stub_result));
    stub_count++;
  };

  if (sequential) {
    for (const auto* method : *context.methods) {
      generate_stub(method);
    }
  } else {
    auto stub_queue = sparta::work_queue<const Method*>(
        [&](const Method* method) { generate_stub(method); });
    for (const auto* method : *context.methods) {
      stub_queue.add_item(method);
    }
    stub_queue.run_all();
  }
  LOG(1, "Generated {} framework method stubs.", stub_count.load());

  // Collect relevant types from all method results for demand-driven
  // class model generation.
  std::unordered_set<const DexType*> all_relevant_types;
  for (const auto& [_, result] : UnorderedIterable(registry.method_results())) {
    all_relevant_types.insert(
        result.relevant_types.begin(), result.relevant_types.end());
  }

  registry.set_class_results(
      LocalFlowClassAnalyzer::analyze_classes(context, &all_relevant_types));

  LOG(1,
      "Local flow analysis complete in {:.2f}s. Analyzed {} methods, "
      "{} relevant types for class dispatch.",
      timer.duration_in_seconds(),
      registry.method_results().size(),
      all_relevant_types.size());
}

} // namespace local_flow
} // namespace marianatrench
