/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/string_file.hpp>
#include <fmt/format.h>

#include <AbstractDomain.h>
#include <ControlFlow.h>
#include <InstructionAnalyzer.h>
#include <MonotonicFixpointIterator.h>
#include <Show.h>
#include <SpartaWorkQueue.h>
#include <TypeInference.h>
#include <Walkers.h>

#include <mariana-trench/AnalysisEnvironment.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/Transfer.h>

namespace marianatrench {

namespace {

using CombinedTransfer = InstructionAnalyzerCombiner<Transfer>;

class FixpointIterator final
    : public sparta::
          MonotonicFixpointIterator<cfg::GraphInterface, AnalysisEnvironment> {
 public:
  FixpointIterator(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<AnalysisEnvironment> instruction_analyzer)
      : MonotonicFixpointIterator(cfg),
        instruction_analyzer_(instruction_analyzer) {}
  virtual ~FixpointIterator() {}

  void analyze_node(const NodeId& block, AnalysisEnvironment* taint)
      const override {
    LOG(4, "Analyzing block {}\n{}", block->id(), *taint);
    for (auto& instruction : *block) {
      switch (instruction.type) {
        case MFLOW_OPCODE:
          instruction_analyzer_(instruction.insn, taint);
          break;
        case MFLOW_POSITION:
          taint->set_last_position(instruction.pos.get());
          break;
        default:
          break;
      }
    }
  }

  AnalysisEnvironment analyze_edge(
      const EdgeId& /*edge*/,
      const AnalysisEnvironment& taint) const override {
    return taint;
  }

 private:
  InstructionAnalyzer<AnalysisEnvironment> instruction_analyzer_;
};

std::string show_control_flow_graph(const cfg::ControlFlowGraph& cfg) {
  std::string string;
  for (const auto* block : cfg.blocks()) {
    string.append(fmt::format("Block {}", block->id()));
    if (block == cfg.entry_block()) {
      string.append(" (entry)");
    }
    string.append(":\n");
    for (const auto& instruction : *block) {
      if (instruction.type == MFLOW_OPCODE) {
        string.append(fmt::format("  {}\n", show(instruction.insn)));
      }
    }
    const auto& successors = block->succs();
    if (!successors.empty()) {
      string.append("  Successors: {");
      for (auto iterator = successors.begin(), end = successors.end();
           iterator != end;) {
        string.append(fmt::format("{}", (*iterator)->target()->id()));
        ++iterator;
        if (iterator != end) {
          string.append(", ");
        }
      }
      string.append("}\n");
    }
  }
  return string;
}

double resident_set_size_in_gb() {
  try {
    std::ifstream infile("/proc/self/status");
    std::string line;
    while (std::getline(infile, line)) {
      if (boost::starts_with(line, "VmRSS:")) {
        return (double)std::stoi(line.substr(7), nullptr) / 1000 / 1000;
      }
    }
    ERROR(1, "Resident set size not found.");
  } catch (const std::exception& error) {
    ERROR(1, "Unable to get resident set size: {}", error.what());
  }
  return -1.0;
}

Model analyze(
    Context& global_context,
    const Registry& registry,
    const Model& old_model) {
  Timer timer;

  Model model = old_model;

  auto* method = model.method();
  if (!method) {
    return model;
  }

  auto method_context =
      std::make_unique<MethodContext>(global_context, registry, model);

  LOG_OR_DUMP(
      method_context, 3, "Analyzing `\033[33m{}\033[0m`...", show(method));

  auto* code = method->get_code();
  if (!code) {
    throw std::runtime_error(fmt::format(
        "Attempting to analyze method `{}` with no code!", show(method)));
  }
  if (!code->cfg_built()) {
    throw std::runtime_error(fmt::format(
        "Attempting to analyze method `{}` with no control flow graph!",
        show(method)));
  } else {
    LOG_OR_DUMP(
        method_context, 4, "Code:\n{}", show_control_flow_graph(code->cfg()));
  }

  auto fixpoint =
      FixpointIterator(code->cfg(), CombinedTransfer(method_context.get()));
  fixpoint.run(AnalysisEnvironment::initial());
  model.approximate();

  LOG_OR_DUMP(
      method_context, 4, "Computed model for `{}`: {}", show(method), model);

  global_context.statistics->log_time(method, timer);
  auto duration = timer.duration_in_seconds();
  if (duration > 10.0) {
    WARNING(1, "Analyzing `{}` took {:.2f}s!", show(method), duration);
  }

  return model;
}

} // namespace

void Interprocedural::run_analysis(Context& context, Registry& registry) {
  LOG(1, "Computing global fixpoint...");
  auto methods_to_analyze = std::make_unique<ConcurrentSet<const Method*>>();
  for (const auto* method : *context.methods) {
    methods_to_analyze->insert(method);
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

    if (iteration > Heuristics::kMaxNumberIterations) {
      ERROR(1, "Too many iterations");
      std::string message = "Unstable methods are:";
      for (const auto* method : *methods_to_analyze) {
        message.append(fmt::format("\n`{}`", show(method)));
      }
      LOG(1, message);
      throw std::runtime_error("Too many iterations, exiting.");
    }

    auto new_methods_to_analyze =
        std::make_unique<ConcurrentSet<const Method*>>();

    unsigned int threads = sparta::parallel::default_num_threads();
    if (context.options->sequential()) {
      WARNING(1, "Running sequentially!");
      threads = 1u;
    }

    std::atomic<std::size_t> method_iteration(0);
    auto queue = sparta::work_queue<const Method*>(
        [&](const Method* method) {
          method_iteration++;
          if (method_iteration % 10000 == 0) {
            LOG(1,
                "Processed {}/{} methods.",
                method_iteration.load(),
                methods_to_analyze->size());
          } else if (method_iteration % 100 == 0) {
            LOG(4,
                "Processed {}/{} methods.",
                method_iteration.load(),
                methods_to_analyze->size());
          }

          const auto old_model = registry.get(method);
          if (old_model.skip_analysis()) {
            LOG(3, "Skipping `{}`...", show(method));
            return;
          }

          auto new_model = analyze(context, registry, old_model);

          new_model.join_with(old_model);

          if (!new_model.leq(old_model)) {
            if (!context.call_graph->callees(method).empty() ||
                !context.call_graph->artificial_callees(method).empty()) {
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

    LOG(2,
        "Global fixpoint iteration completed in {:.2f}s.",
        iteration_timer.duration_in_seconds());

    methods_to_analyze = std::move(new_methods_to_analyze);
  }

  context.statistics->log_number_iterations(iteration);
  LOG(2, "Global fixpoint reached.");
}

} // namespace marianatrench
