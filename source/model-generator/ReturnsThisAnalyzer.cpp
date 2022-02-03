/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <ControlFlow.h>
#include <HashedAbstractEnvironment.h>
#include <HashedSetAbstractDomain.h>
#include <InstructionAnalyzer.h>
#include <MonotonicFixpointIterator.h>
#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/model-generator/ReturnsThisAnalyzer.h>

namespace marianatrench {
namespace returns_this_analyzer {
namespace {

enum class Location { ThisParameter, Default };

std::ostream& operator<<(std::ostream& out, const Location location) {
  out << "Location(";
  switch (location) {
    case Location::ThisParameter:
      out << "ThisParameter";
      break;

    case Location::Default:
      out << "Default";
      break;
  }

  return out << ")";
}

struct LocationHash {
  std::size_t operator()(const Location location) const {
    return static_cast<std::size_t>(location);
  }
};

using ParameterPosition = std::uint32_t;
using Domain = sparta::HashedSetAbstractDomain<Location, LocationHash>;
using ReturnsThisEnvironment = sparta::HashedAbstractEnvironment<reg_t, Domain>;

class ReturnsThisContext final {
 public:
  ReturnsThisContext() : last_parameter_load_(0), return_locations_({}) {}

  ReturnsThisContext(const ReturnsThisContext&) = delete;
  ReturnsThisContext(ReturnsThisContext&&) = delete;
  ReturnsThisContext& operator=(const ReturnsThisContext&) = delete;
  ReturnsThisContext& operator=(ReturnsThisContext&&) = delete;
  ~ReturnsThisContext() = default;

  ParameterPosition last_parameter_loaded() const {
    return last_parameter_load_;
  }

  void increment_last_parameter_loaded() {
    last_parameter_load_ += 1;
  }

  void join_return_location(Domain locations) {
    return_locations_.join_with(locations);
  }

  const Domain& return_locations() const {
    return return_locations_;
  }

 private:
  ParameterPosition last_parameter_load_;
  Domain return_locations_;
};

class Transfer final : public InstructionAnalyzerBase<
                           Transfer,
                           ReturnsThisEnvironment,
                           ReturnsThisContext*> {
 public:
  static bool analyze_default(
      ReturnsThisContext* context,
      const IRInstruction* instruction,
      ReturnsThisEnvironment* current_state) {
    LOG(4, "Analyzing instruction: {}", show(instruction));

    if (opcode::is_a_load_param(instruction->opcode())) {
      auto current_parameter_position = context->last_parameter_loaded();
      context->increment_last_parameter_loaded();
      auto location = current_parameter_position == 0 ? Location::ThisParameter
                                                      : Location::Default;
      LOG(4,
          "load-param: {}. Setting register: {} to Location: {}",
          current_parameter_position,
          instruction->dest(),
          location);

      current_state->set(instruction->dest(), Domain(location));
    } else if (opcode::is_a_return_value(instruction->opcode())) {
      mt_assert(instruction->srcs().size() == 1);

      auto reg = instruction->srcs()[0];
      auto return_locations = current_state->get(reg);
      LOG(4, "Return register {} points to {}", reg, return_locations);
      context->join_return_location(return_locations);
    } else if (opcode::is_move_result_any(instruction->opcode())) {
      auto result_locations = current_state->get(RESULT_REGISTER);

      LOG(4,
          "is-move-result-any. Setting dest register {} to location: {}",
          instruction->dest(),
          result_locations);

      current_state->set(instruction->dest(), result_locations);
      current_state->set(RESULT_REGISTER, Domain::top());
    } else if (instruction->has_move_result_any()) {
      auto result_location = Domain(Location::Default);
      LOG(4, "has-move-result. Setting result register to {}", result_location);

      current_state->set(RESULT_REGISTER, result_location);
    } else if (instruction->has_dest()) {
      auto result_location = Domain(Location::Default);
      LOG(4,
          "has-dest. Setting dest register {} to {}",
          instruction->dest(),
          result_location);
      current_state->set(instruction->dest(), result_location);
    }

    return false;
  }
};

class ReturnsThisFixpointIterator final
    : public sparta::MonotonicFixpointIterator<
          cfg::GraphInterface,
          ReturnsThisEnvironment> {
 public:
  ReturnsThisFixpointIterator(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<ReturnsThisEnvironment> instruction_analyzer)
      : MonotonicFixpointIterator(cfg),
        instruction_analyzer_(instruction_analyzer) {}

  void analyze_node(const NodeId& block, ReturnsThisEnvironment* current_state)
      const override {
    for (auto& entry : *block) {
      switch (entry.type) {
        case MFLOW_OPCODE:
          instruction_analyzer_(entry.insn, current_state);
          break;

        default:
          break;
      }
    }
  }

  ReturnsThisEnvironment analyze_edge(
      const cfg::GraphInterface::EdgeId&,
      const ReturnsThisEnvironment& exit_state) const override {
    return exit_state;
  }

 private:
  InstructionAnalyzer<ReturnsThisEnvironment> instruction_analyzer_;
};

} // namespace

bool method_returns_this(const Method* method) {
  if (method->is_static() ||
      method->get_proto()->get_rtype() != method->get_class()) {
    return false;
  }

  auto* code = method->get_code();
  if (code == nullptr) {
    return false;
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate ReturnsThisConstraint.",
        method->show());
    return false;
  }

  LOG(4, "Testing ReturnsThisConstraint for: {}", method->show());
  auto context = std::make_unique<ReturnsThisContext>();
  auto fixpoint = ReturnsThisFixpointIterator(
      code->cfg(), InstructionAnalyzerCombiner<Transfer>(context.get()));
  fixpoint.run(ReturnsThisEnvironment());

  if (context->return_locations().size() == 0) {
    LOG(1, "{} does not have return locations!", method->show());
    return false;
  }

  const auto& return_locations = context->return_locations().elements();
  // Over-estimate if multiple return statements present.
  return std::any_of(
      return_locations.cbegin(), return_locations.cend(), [](auto location) {
        return location == Location::ThisParameter;
      });
}

} // namespace returns_this_analyzer
} // namespace marianatrench
