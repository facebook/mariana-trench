/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>

#include <mutex>
#include <string_view>

#include <DexAccess.h>
#include <DexClass.h>
#include <DexStore.h>
#include <Show.h>
#include <TypeUtil.h>
#include <Walkers.h>

#include <sparta/WorkQueue.h>

#include <fmt/format.h>

#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Log.h>

namespace marianatrench {
namespace local_flow {

namespace {

std::string format_interval(const ClassIntervals::Interval& interval) {
  if (interval.is_bottom()) {
    return "Interval:[]";
  }
  return fmt::format(
      "Interval:[{},{}]", interval.lower_bound(), interval.upper_bound());
}

} // namespace

std::vector<LocalFlowClassResult> LocalFlowClassAnalyzer::analyze_classes(
    const Context& context,
    const std::unordered_set<const DexType*>* relevant_types) {
  std::vector<LocalFlowClassResult> results;
  std::mutex results_mutex;

  const auto* class_intervals = context.class_intervals.get();

  for (const auto& scope : DexStoreClassesIterator(context.stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      // Skip pure interfaces — they participate via OVar edges from
      // implementing classes, not via their own CVar.
      if (is_interface(klass)) {
        return;
      }

      // Demand-driven: skip classes not referenced during method analysis
      if (relevant_types != nullptr &&
          relevant_types->count(klass->get_type()) == 0) {
        return;
      }

      auto* class_type = klass->get_type();
      std::string class_name = show(class_type);

      // Collect concrete instance methods (virtual + non-static direct)
      std::vector<const DexMethod*> instance_methods;
      for (const auto* vmethod : klass->get_vmethods()) {
        if (!is_abstract(vmethod)) {
          instance_methods.push_back(vmethod);
        }
      }
      for (const auto* dmethod : klass->get_dmethods()) {
        // Include private instance methods, exclude static and <init>/<clinit>
        if (!is_static(dmethod) &&
            std::string_view(dmethod->get_name()->c_str()) != "<init>" &&
            std::string_view(dmethod->get_name()->c_str()) != "<clinit>") {
          instance_methods.push_back(dmethod);
        }
      }

      if (instance_methods.empty()) {
        return;
      }

      auto cvar_node = LocalFlowNode::make_cvar(class_name);
      auto ovar_node = LocalFlowNode::make_ovar(class_name);

      std::vector<DispatchEdge> edges;

      // 1. Meth -> CVar edges for each declared method
      for (const auto* method : instance_methods) {
        std::string method_sig =
            LocalFlowNode::jvm_to_lfe_separator(show(method));
        auto meth_node = LocalFlowNode::make_meth(method_sig);

        // Use full vtable name (including signature for Java overload
        // disambiguation) to match self-loop callee Inject labels.
        auto vtable_name = LocalFlowNode::extract_vtable_name(meth_node);
        edges.push_back(
            DispatchEdge{
                std::move(meth_node),
                StructuralLabelPath{
                    StructuralLabel{StructuralOp::Inject, vtable_name}},
                cvar_node});
      }

      // 2. CVar -> OVar edge (exact dispatch subsumes override dispatch)
      edges.push_back(
          DispatchEdge{cvar_node, StructuralLabelPath{}, ovar_node});

      // 3. OVar -> OVar edges for supertypes with interval labels
      auto* super_type = klass->get_super_class();
      if (super_type != nullptr && super_type != type::java_lang_Object()) {
        std::string interval_label;
        if (class_intervals != nullptr) {
          const auto& interval = class_intervals->get_interval(class_type);
          interval_label = format_interval(interval);
        } else {
          interval_label = "Interval:[]";
        }

        edges.push_back(
            DispatchEdge{
                ovar_node,
                StructuralLabelPath{
                    StructuralLabel{StructuralOp::Inject, interval_label}},
                LocalFlowNode::make_ovar(show(super_type))});
      }

      // 4. OVar -> OVar edges for implemented interfaces
      auto* interfaces = klass->get_interfaces();
      if (interfaces != nullptr) {
        std::string interval_label;
        if (class_intervals != nullptr) {
          const auto& interval = class_intervals->get_interval(class_type);
          interval_label = format_interval(interval);
        } else {
          interval_label = "Interval:[]";
        }

        for (auto* intf_type : *interfaces) {
          edges.push_back(
              DispatchEdge{
                  ovar_node,
                  StructuralLabelPath{
                      StructuralLabel{StructuralOp::Inject, interval_label}},
                  LocalFlowNode::make_ovar(show(intf_type))});
        }
      }

      // Populate path from DexClass source file
      std::string class_path;
      auto* source_file = klass->get_source_file();
      if (source_file != nullptr) {
        class_path = source_file->str();
      }

      // Populate interval from ClassIntervals
      std::string interval_str;
      if (class_intervals != nullptr) {
        const auto& class_interval = class_intervals->get_interval(class_type);
        if (!class_interval.is_bottom()) {
          interval_str = fmt::format(
              "[{},{}]",
              class_interval.lower_bound(),
              class_interval.upper_bound());
        }
      }

      std::lock_guard<std::mutex> lock(results_mutex);
      results.push_back(
          LocalFlowClassResult{
              std::move(class_name),
              std::move(edges),
              std::move(class_path),
              /* callable_line */ 0,
              std::move(interval_str)});
    });
  }

  LOG(1,
      "LocalFlowClassAnalyzer: generated dispatch edges for {} classes.",
      results.size());

  return results;
}

} // namespace local_flow
} // namespace marianatrench
