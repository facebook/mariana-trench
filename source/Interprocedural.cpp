/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <sparta/WorkQueue.h>

#include <ControlFlow.h>
#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/BackwardTaintEnvironment.h>
#include <mariana-trench/BackwardTaintFixpoint.h>
#include <mariana-trench/BackwardTaintTransfer.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/ForwardAliasEnvironment.h>
#include <mariana-trench/ForwardAliasFixpoint.h>
#include <mariana-trench/ForwardAliasTransfer.h>
#include <mariana-trench/ForwardTaintEnvironment.h>
#include <mariana-trench/ForwardTaintFixpoint.h>
#include <mariana-trench/ForwardTaintTransfer.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

namespace {

Model analyze(
    Context& global_context,
    const Registry& registry,
    const Model& previous_model) {
  Timer timer;

  auto* method = previous_model.method();
  if (!method) {
    return previous_model;
  }

  auto new_model = previous_model.initial_model_for_iteration();

  MethodContext method_context(
      global_context, registry, previous_model, new_model);

  LOG_OR_DUMP(
      &method_context, 3, "Analyzing `\033[33m{}\033[0m`...", method->show());

  auto* code = method->get_code();
  if (!code) {
    throw std::runtime_error(fmt::format(
        "Attempting to analyze method `{}` with no code!", method->show()));
  }
  if (!code->cfg_built()) {
    throw std::runtime_error(fmt::format(
        "Attempting to analyze method `{}` with no control flow graph!",
        method->show()));
  }
  if (code->cfg().exit_block() == nullptr) {
    throw std::runtime_error(fmt::format(
        "Attempting to analyze control flow graph for `{}` with no exit block!",
        method->show()));
  }

  LOG_OR_DUMP(
      &method_context,
      4,
      "Code:\n{}",
      Method::show_control_flow_graph(code->cfg()));

  bool analysis_timed_out = false;

  {
    // TODO(T144485000): This could potentially be done once as a pre-analysis
    // step and cached. The handling of inlining (`inline_as_getter`) might make
    // this impossible unfortunately.
    LOG_OR_DUMP(
        &method_context, 4, "Forward alias analysis of `{}`", method->show());
    auto forward_alias_fixpoint = ForwardAliasFixpoint(
        method_context,
        code->cfg(),
        InstructionAnalyzerCombiner<ForwardAliasTransfer>(&method_context));
    try {
      forward_alias_fixpoint.run(ForwardAliasEnvironment::initial());
    } catch (const TimeoutError& error) {
      analysis_timed_out = true;
      WARNING(1, "TimeoutError: {}", error.what());
      EventLogger::log_event(
          "method_timed_out_forward_alias_analysis",
          /* message */ method->show(),
          /* value */ error.duration_in_seconds(),
          /* verbosity_level */ 1);
    }

    LOG_OR_DUMP(
        &method_context,
        4,
        "Forward alias analysis of `{}` took {:.2f}s",
        method->show(),
        forward_alias_fixpoint.timer().duration_in_seconds());
  }

  if (!analysis_timed_out) {
    LOG_OR_DUMP(
        &method_context, 4, "Forward taint analysis of `{}`", method->show());
    auto forward_taint_fixpoint = ForwardTaintFixpoint(
        method_context,
        code->cfg(),
        InstructionAnalyzerCombiner<ForwardTaintTransfer>(&method_context));
    try {
      forward_taint_fixpoint.run(ForwardTaintEnvironment::initial());
    } catch (const TimeoutError& error) {
      analysis_timed_out = true;
      WARNING(1, "TimeoutError: {}", error.what());
      EventLogger::log_event(
          "method_timed_out_forward_taint_analysis",
          /* message */ method->show(),
          /* value */ error.duration_in_seconds(),
          /* verbosity_level */ 1);
    }
    LOG_OR_DUMP(
        &method_context,
        4,
        "Forward taint analysis of `{}` took {:.2f}s",
        method->show(),
        forward_taint_fixpoint.timer().duration_in_seconds());
  }

  if (!analysis_timed_out) {
    LOG_OR_DUMP(
        &method_context, 4, "Backward taint analysis of `{}`", method->show());
    auto backward_taint_fixpoint = BackwardTaintFixpoint(
        method_context,
        code->cfg(),
        InstructionAnalyzerCombiner<BackwardTaintTransfer>(&method_context));
    try {
      backward_taint_fixpoint.run(
          BackwardTaintEnvironment::initial(method_context));
    } catch (const TimeoutError& error) {
      analysis_timed_out = true;
      WARNING(1, "TimeoutError: {}", error.what());
      EventLogger::log_event(
          "method_timed_out_backward_taint_analysis",
          /* message */ method->show(),
          /* value */ error.duration_in_seconds(),
          /* verbosity_level */ 1);
    }
    LOG_OR_DUMP(
        &method_context,
        4,
        "Backward taint analysis of `{}` took {:.2f}s",
        method->show(),
        backward_taint_fixpoint.timer().duration_in_seconds());
  }

  new_model.collapse_invalid_paths(global_context);
  new_model.approximate(/* widening_features */
                        FeatureMayAlwaysSet{
                            global_context.feature_factory
                                ->get_widen_broadening_feature()},
                        *global_context.heuristics);

  LOG_OR_DUMP(
      &method_context,
      4,
      "Computed model for `{}`: {}",
      method->show(),
      new_model);

  global_context.statistics->log_time(method, timer);
  auto duration = timer.duration_in_seconds();
  if (duration > 10.0) {
    WARNING(1, "Analyzing `{}` took {:.2f}s!", method->show(), duration);
    EventLogger::log_event(
        "slow_method",
        /* message */ method->show(),
        /* value */ duration,
        /* verbosity_level */ 1);
  }

  if (analysis_timed_out) {
    LOG(1,
        "Analyzing `{}` exceeded maximum per-analyzer timeout duration of {}s, setting default taint-in-taint-out.",
        method->show(),
        global_context.options->maximum_method_analysis_time().value_or(
            std::numeric_limits<int>::max()));
    new_model.add_mode(Model::Mode::AddViaObscureFeature, global_context);
    new_model.add_mode(Model::Mode::SkipAnalysis, global_context);
    new_model.add_mode(Model::Mode::NoJoinVirtualOverrides, global_context);
    new_model.add_mode(Model::Mode::TaintInTaintOut, global_context);
    new_model.add_mode(Model::Mode::TaintInTaintThis, global_context);
  }

  return new_model;
}

} // namespace

void Interprocedural::run_analysis(Context& context, Registry& registry) {
  LOG(1, "Computing global fixpoint...");
  auto methods_to_analyze = std::make_unique<ConcurrentSet<const Method*>>();
  for (const auto* method : *context.methods) {
    methods_to_analyze->insert(method);
  }

  unsigned int threads = sparta::parallel::default_num_threads();
  if (context.options->sequential()) {
    WARNING(1, "Running sequentially!");
    threads = 1u;
  } else {
    LOG(1, "Using {} threads", threads);
  }

  std::size_t iteration = 0;
  while (methods_to_analyze->size() > 0) {
    Timer iteration_timer;
    iteration++;

    auto resident_set_size = resident_set_size_in_gb();
    context.statistics->log_resident_set_size(resident_set_size);
    LOG(1,
        "Global iteration {}. Analyzing {} methods... (Memory used, RSS: {:.2f}GB)",
        iteration,
        methods_to_analyze->size(),
        resident_set_size);

    if (iteration > context.heuristics->max_number_iterations()) {
      ERROR(1, "Too many iterations");
      std::string message = "Unstable methods are:";
      for (const auto* method : UnorderedIterable(*methods_to_analyze)) {
        message.append(fmt::format("\n`{}`", method->show()));
      }
      LOG(1, message);
      throw std::runtime_error("Too many iterations, exiting.");
    }

    auto new_methods_to_analyze =
        std::make_unique<ConcurrentSet<const Method*>>();

    std::atomic<std::size_t> method_iteration(0);
    auto queue = sparta::work_queue<const Method*>(
        [&](const Method* method) {
          method_iteration++;
          if (method_iteration % 10000 == 0) {
            LOG_IF_INTERACTIVE(
                1,
                "Processed {}/{} methods.",
                method_iteration.load(),
                methods_to_analyze->size());
          } else if (method_iteration % 100 == 0) {
            LOG_IF_INTERACTIVE(
                4,
                "Processed {}/{} methods.",
                method_iteration.load(),
                methods_to_analyze->size());
          }

          const auto previous_model = registry.get(method);
          if (previous_model.skip_analysis()) {
            LOG(3, "Skipping `{}`...", method->show());
            return;
          }

          auto new_model = analyze(context, registry, previous_model);

          new_model.join_with(previous_model);

          if (!new_model.leq(previous_model)) {
            if (context.call_graph->has_callees(method)) {
              new_methods_to_analyze->insert(method);
            }
            for (const auto* dependency :
                 context.dependencies->dependencies(method)) {
              new_methods_to_analyze->insert(dependency);
            }
          }

          registry.set(new_model);
        },
        threads);
    context.scheduler->schedule(
        *methods_to_analyze,
        [&](const Method* method, std::size_t worker_id) {
          queue.add_item(method, worker_id);
        },
        threads);
    queue.run_all();

    LOG(1,
        "Global iteration {} completed in {:.2f}s.",
        iteration,
        iteration_timer.duration_in_seconds());

    methods_to_analyze = std::move(new_methods_to_analyze);
  }

  context.statistics->log_number_iterations(iteration);
  LOG(2, "Global fixpoint reached.");
}

} // namespace marianatrench
