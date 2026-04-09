/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowTransfer.h>

#include <DexClass.h>
#include <DexPosition.h>
#include <IROpcode.h>
#include <Show.h>

#include <fmt/format.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/local_flow/LocalFlowLambdaDetection.h>

namespace marianatrench {
namespace local_flow {

LocalFlowTransfer::LocalFlowTransfer(
    const Method* method,
    TempIdGenerator& temp_gen,
    const Positions* MT_NULLABLE positions,
    const CallGraph* MT_NULLABLE call_graph)
    : method_(method),
      temp_gen_(temp_gen),
      positions_(positions),
      call_graph_(call_graph),
      next_param_index_(0),
      is_static_(method_->get_access() & ACC_STATIC) {}

void LocalFlowTransfer::analyze_instruction(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints,
    const DexPosition* MT_NULLABLE position) {
  auto opcode = instruction->opcode();

  switch (opcode) {
    case IOPCODE_LOAD_PARAM:
    case IOPCODE_LOAD_PARAM_OBJECT:
    case IOPCODE_LOAD_PARAM_WIDE:
      analyze_load_param(instruction, env);
      break;

    case OPCODE_MOVE:
    case OPCODE_MOVE_OBJECT:
    case OPCODE_MOVE_WIDE:
      analyze_move(instruction, env);
      break;

    case OPCODE_CONST:
    case OPCODE_CONST_WIDE:
    case OPCODE_CONST_STRING:
    case OPCODE_CONST_CLASS:
      analyze_const(instruction, env);
      break;

    case OPCODE_IGET:
    case OPCODE_IGET_BOOLEAN:
    case OPCODE_IGET_BYTE:
    case OPCODE_IGET_CHAR:
    case OPCODE_IGET_OBJECT:
    case OPCODE_IGET_SHORT:
    case OPCODE_IGET_WIDE:
      analyze_iget(instruction, env, constraints);
      break;

    case OPCODE_IPUT:
    case OPCODE_IPUT_BOOLEAN:
    case OPCODE_IPUT_BYTE:
    case OPCODE_IPUT_CHAR:
    case OPCODE_IPUT_OBJECT:
    case OPCODE_IPUT_SHORT:
    case OPCODE_IPUT_WIDE:
      analyze_iput(instruction, env, constraints);
      break;

    case OPCODE_SGET:
    case OPCODE_SGET_BOOLEAN:
    case OPCODE_SGET_BYTE:
    case OPCODE_SGET_CHAR:
    case OPCODE_SGET_OBJECT:
    case OPCODE_SGET_SHORT:
    case OPCODE_SGET_WIDE:
      analyze_sget(instruction, env);
      break;

    case OPCODE_SPUT:
    case OPCODE_SPUT_BOOLEAN:
    case OPCODE_SPUT_BYTE:
    case OPCODE_SPUT_CHAR:
    case OPCODE_SPUT_OBJECT:
    case OPCODE_SPUT_SHORT:
    case OPCODE_SPUT_WIDE:
      analyze_sput(instruction, env, constraints);
      break;

    case OPCODE_AGET:
    case OPCODE_AGET_BOOLEAN:
    case OPCODE_AGET_BYTE:
    case OPCODE_AGET_CHAR:
    case OPCODE_AGET_OBJECT:
    case OPCODE_AGET_SHORT:
    case OPCODE_AGET_WIDE:
      analyze_aget(instruction, env, constraints);
      break;

    case OPCODE_APUT:
    case OPCODE_APUT_BOOLEAN:
    case OPCODE_APUT_BYTE:
    case OPCODE_APUT_CHAR:
    case OPCODE_APUT_OBJECT:
    case OPCODE_APUT_SHORT:
    case OPCODE_APUT_WIDE:
      analyze_aput(instruction, env, constraints);
      break;

    case OPCODE_INVOKE_STATIC:
    case OPCODE_INVOKE_VIRTUAL:
    case OPCODE_INVOKE_DIRECT:
    case OPCODE_INVOKE_INTERFACE:
    case OPCODE_INVOKE_SUPER:
      analyze_invoke(instruction, env, constraints, position);
      break;

    case OPCODE_MOVE_RESULT:
    case OPCODE_MOVE_RESULT_OBJECT:
    case OPCODE_MOVE_RESULT_WIDE:
    case IOPCODE_MOVE_RESULT_PSEUDO:
    case IOPCODE_MOVE_RESULT_PSEUDO_OBJECT:
    case IOPCODE_MOVE_RESULT_PSEUDO_WIDE:
      analyze_move_result(instruction, env);
      break;

    case OPCODE_RETURN:
    case OPCODE_RETURN_OBJECT:
    case OPCODE_RETURN_WIDE:
      analyze_return(instruction, env, constraints);
      break;

    case OPCODE_RETURN_VOID:
      // No data flow for void return
      break;

    case OPCODE_NEW_INSTANCE:
      analyze_new_instance(instruction, env);
      break;

    case OPCODE_CHECK_CAST:
      analyze_check_cast(instruction, env);
      break;

    case OPCODE_FILLED_NEW_ARRAY:
      analyze_filled_new_array(instruction, env, constraints);
      break;

    case OPCODE_THROW:
      analyze_throw(instruction, env, constraints);
      break;

    default:
      // For unhandled opcodes that define a register, create a fresh temp
      if (instruction->has_dest()) {
        env.insert_or_assign(
            instruction->dest(),
            LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next())));
      }
      break;
  }
}

const std::optional<LocalFlowValueTree>& LocalFlowTransfer::pending_result()
    const {
  return pending_result_;
}

std::optional<reg_t> LocalFlowTransfer::this_register() const {
  if (is_static_) {
    return std::nullopt;
  }
  for (const auto& [reg, param_index] : param_registers_) {
    if (param_index == 0) {
      return reg;
    }
  }
  return std::nullopt;
}

void LocalFlowTransfer::analyze_load_param(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  auto reg = instruction->dest();
  auto param_index = next_param_index_++;

  // Track parameter register for this detection
  param_registers_[reg] = param_index;

  auto param_node = LocalFlowNode::make_param(param_index);
  env.insert_or_assign(reg, LocalFlowValueTree(std::move(param_node)));
}

void LocalFlowTransfer::analyze_move(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  auto src = instruction->src(0);
  auto dst = instruction->dest();

  auto it = env.find(src);
  if (it != env.end()) {
    env.insert_or_assign(dst, it->second);
    // Propagate param register tracking
    auto param_it = param_registers_.find(src);
    if (param_it != param_registers_.end()) {
      param_registers_[dst] = param_it->second;
    }
  } else {
    env.insert_or_assign(
        dst, LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next())));
  }
}

void LocalFlowTransfer::analyze_const(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  std::string value;

  if (instruction->opcode() == OPCODE_CONST_STRING) {
    value = show(instruction->get_string());
  } else if (instruction->opcode() == OPCODE_CONST_CLASS) {
    value = show(instruction->get_type());
  } else {
    value = std::to_string(instruction->get_literal());
  }

  auto tree =
      LocalFlowValueTree(LocalFlowNode::make_constant(std::move(value)));

  // CONST_STRING and CONST_CLASS produce their result via
  // IOPCODE_MOVE_RESULT_PSEUDO; CONST/CONST_WIDE have a dest register.
  if (instruction->opcode() == OPCODE_CONST_STRING ||
      instruction->opcode() == OPCODE_CONST_CLASS) {
    pending_result_ = std::move(tree);
  } else {
    env.insert_or_assign(instruction->dest(), std::move(tree));
  }
}

void LocalFlowTransfer::analyze_iget(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto obj_reg = instruction->src(0);
  auto* field_ref = instruction->get_field();
  std::string field_name(field_ref->get_name()->str());

  const auto& obj_tree = get_or_create(env, obj_reg);

  // dst = obj.select(Project:field)
  auto result_tree = obj_tree.select(field_name, temp_gen_);

  // Emit constraint: obj -[Project:field]-> result
  constraints.add_edge(
      obj_tree.root(),
      LabelPath{Label{LabelKind::Project, field_name}},
      result_tree.root());

  // Result is picked up by IOPCODE_MOVE_RESULT_PSEUDO.
  pending_result_ = std::move(result_tree);
}

void LocalFlowTransfer::analyze_iput(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto val_reg = instruction->src(0);
  auto obj_reg = instruction->src(1);
  auto* field_ref = instruction->get_field();
  std::string field_name(field_ref->get_name()->str());

  const auto& val_tree = get_or_create(env, val_reg);
  const auto& obj_tree = get_or_create(env, obj_reg);

  // Emit constraint: val -[Inject:field]-> obj
  constraints.add_edge(
      val_tree.root(),
      LabelPath{Label{LabelKind::Inject, field_name}},
      obj_tree.root());

  // Update obj tree with the new field value
  auto updated = obj_tree.update(field_name, val_tree);
  env.insert_or_assign(obj_reg, std::move(updated));
}

void LocalFlowTransfer::analyze_sget(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  (void)env;
  auto* field_ref = instruction->get_field();

  std::string global_name = fmt::format(
      "{}||{}", show(field_ref->get_class()), field_ref->get_name()->str());

  // Result is picked up by IOPCODE_MOVE_RESULT_PSEUDO.
  pending_result_ =
      LocalFlowValueTree(LocalFlowNode::make_global(std::move(global_name)));
}

void LocalFlowTransfer::analyze_sput(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto val_reg = instruction->src(0);
  auto* field_ref = instruction->get_field();

  std::string global_name = fmt::format(
      "{}||{}", show(field_ref->get_class()), field_ref->get_name()->str());

  const auto& val_tree = get_or_create(env, val_reg);
  auto global_node = LocalFlowNode::make_global(global_name);

  // Emit constraint: val -> Global(class.field)
  constraints.add_edge(val_tree.root(), global_node);
}

void LocalFlowTransfer::analyze_aget(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto arr_reg = instruction->src(0);

  const auto& arr_tree = get_or_create(env, arr_reg);

  // Wildcard index access: arr -[Project:*]-> result
  auto result_tree = arr_tree.select("", temp_gen_);

  constraints.add_edge(
      arr_tree.root(),
      LabelPath{Label{LabelKind::Project, "" /* wildcard */}},
      result_tree.root());

  // Result is picked up by IOPCODE_MOVE_RESULT_PSEUDO.
  pending_result_ = std::move(result_tree);
}

void LocalFlowTransfer::analyze_aput(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto val_reg = instruction->src(0);
  auto arr_reg = instruction->src(1);

  const auto& val_tree = get_or_create(env, val_reg);
  const auto& arr_tree = get_or_create(env, arr_reg);

  // Emit constraint: val -[Inject:*]-> arr
  constraints.add_edge(
      val_tree.root(),
      LabelPath{Label{LabelKind::Inject, "" /* wildcard */}},
      arr_tree.root());
}

void LocalFlowTransfer::analyze_invoke(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints,
    const DexPosition* MT_NULLABLE position) {
  // Skip intrinsic checks and trivial Object.<init>() — they add noise
  // without contributing meaningful data flow.
  if (should_skip_invoke(instruction)) {
    return;
  }

  auto opcode = instruction->opcode();
  auto* method_ref = instruction->get_method();
  auto callee_node = get_callee_node(instruction);

  // Collect receiver/callee type for demand-driven class models
  relevant_types_.insert(method_ref->get_class());

  // Format call site for position-annotated labels
  auto call_site = format_call_site(position);

  // Determine if this is a context-preserving call (receiver is `this`)
  bool preserves_context = false;
  bool is_static = (opcode == OPCODE_INVOKE_STATIC);
  bool is_init = false;

  if (!is_static && instruction->srcs_size() > 0) {
    auto receiver_reg = instruction->src(0);
    preserves_context = is_this_register(receiver_reg);

    // Check if this is a constructor call
    auto method_name = std::string(method_ref->get_name()->str());
    is_init = (method_name == "<init>");
  }

  // Constructors never preserve context (even when called on `this`)
  if (is_init) {
    preserves_context = false;
  }

  if (opcode == OPCODE_INVOKE_SUPER) {
    preserves_context = true;
  }

  // Check for lambda (higher-order) invocation
  bool is_lambda_invoke = LambdaDetection::is_lambda_invoke(method_ref);

  LabelKind call_kind = is_lambda_invoke ? LabelKind::HoCall : LabelKind::Call;
  LabelKind return_kind =
      is_lambda_invoke ? LabelKind::HoReturn : LabelKind::Return;

  bool call_preserves = is_static ? false : preserves_context;

  // Check if this is a lambda constructor (Capture labels for captured args)
  bool is_lambda_init = false;
  if (is_init && !is_static && instruction->srcs_size() > 0) {
    auto receiver_type = std::string(method_ref->get_class()->str());
    is_lambda_init = LambdaDetection::is_kotlin_lambda_class(receiver_type);
  }

  // Emit constraints for each argument
  uint32_t param_offset = is_static ? 0 : 1;
  for (uint32_t i = 0; i < instruction->srcs_size(); i++) {
    auto arg_reg = instruction->src(i);
    const auto& arg_tree = get_or_create(env, arg_reg);

    std::string param_name;
    if (i == 0 && !is_static) {
      param_name = "this";
    } else {
      param_name = fmt::format("p{}", i - param_offset);
    }

    // For lambda constructors, non-this args use Capture label
    Label arg_call_label = (is_lambda_init && i > 0)
        ? Label{LabelKind::Capture, {}, false, call_site}
        : Label{call_kind, {}, call_preserves, call_site};

    // arg -[ContraInject:param_i, Call/Capture]-> callee
    // ContraInject for arguments flowing INTO a callee. Cross-call edge
    // composition in temp elimination is enabled by removing the cross-contra
    // kill in cons_label (ContraInject + Project now stacks instead of
    // killing).
    constraints.add_edge(
        arg_tree.root(),
        LabelPath{Label{LabelKind::ContraInject, param_name}, arg_call_label},
        callee_node);
  }

  // Set up pending result
  // callee -[Project:result, Return]-> fresh_result_temp
  auto result_temp = LocalFlowNode::make_temp(temp_gen_.next());
  auto return_label = Label{return_kind, {}, call_preserves, call_site};

  constraints.add_edge(
      callee_node,
      LabelPath{Label{LabelKind::Project, "result"}, return_label},
      result_temp);

  pending_result_ = LocalFlowValueTree(result_temp);

  // For all non-static instance calls: this_out flows back to receiver.
  // Constructors mutate this via field init; setters/builders mutate via
  // iput. In both cases the caller needs the updated receiver state.
  if (!is_static && instruction->srcs_size() > 0) {
    auto receiver_reg = instruction->src(0);
    auto this_out_temp = LocalFlowNode::make_temp(temp_gen_.next());

    constraints.add_edge(
        callee_node,
        LabelPath{Label{LabelKind::Project, "this_out"}, return_label},
        this_out_temp);

    // Update receiver register with this_out, including p0 (`this`).
    // $this is overwritten with a fresh this_out temp after every instance
    // call. Temp elimination composes through the chain, preserving call
    // ordering and this-mutation tracking. (Previously p0 was pinned as a
    // workaround for the cons_label direction bug — now fixed, Call/Return
    // pairs cancel correctly.)
    env.insert_or_assign(receiver_reg, LocalFlowValueTree(this_out_temp));
  }

  // Emit ctrl constraints (two directions):
  //
  // 1. callee -[Project:ctrl, Return]-> Ctrl
  //    Return path: callee's termination flows into method's ctrl port.
  constraints.add_edge(
      callee_node,
      LabelPath{Label{LabelKind::Project, "ctrl"}, return_label},
      LocalFlowNode::make_ctrl());

  // 2. Ctrl -[ContraInject:ctrl, Call]-> callee
  //    Call path: method's control flow reaches this call site.
  //    This enables LFE's `.ctrl fwd` search to find callees.
  auto ctrl_call_label = Label{call_kind, {}, call_preserves, call_site};
  constraints.add_edge(
      LocalFlowNode::make_ctrl(),
      LabelPath{Label{LabelKind::ContraInject, "ctrl"}, ctrl_call_label},
      callee_node);
}

void LocalFlowTransfer::analyze_move_result(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  auto dst_reg = instruction->dest();

  if (pending_result_.has_value()) {
    env.insert_or_assign(dst_reg, std::move(*pending_result_));
    pending_result_.reset();
  } else {
    env.insert_or_assign(
        dst_reg,
        LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next())));
  }
}

void LocalFlowTransfer::analyze_return(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  auto ret_reg = instruction->src(0);
  const auto& ret_tree = get_or_create(env, ret_reg);

  // ret -> Result
  constraints.add_edge(ret_tree.root(), LocalFlowNode::make_result());
}

void LocalFlowTransfer::analyze_new_instance(
    const IRInstruction* instruction,
    RegisterEnvironment& /* env */) {
  // Collect type for demand-driven class models
  relevant_types_.insert(instruction->get_type());

  // In CFG mode, NEW_INSTANCE has no dest — the result goes to
  // IOPCODE_MOVE_RESULT_PSEUDO_OBJECT which picks up pending_result_.
  pending_result_ =
      LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next()));
}

void LocalFlowTransfer::analyze_check_cast(
    const IRInstruction* instruction,
    RegisterEnvironment& env) {
  // CHECK_CAST is identity flow in Redex -- src(0) is checked and
  // the result goes to MOVE_RESULT_PSEUDO. In the CFG representation,
  // it behaves as a move.
  auto src_reg = instruction->src(0);
  auto it = env.find(src_reg);
  if (it != env.end()) {
    pending_result_ = it->second;
  } else {
    pending_result_ =
        LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next()));
  }
}

void LocalFlowTransfer::analyze_filled_new_array(
    const IRInstruction* instruction,
    RegisterEnvironment& env,
    LocalFlowConstraintSet& constraints) {
  // Create a fresh temp for the array
  auto arr_node = LocalFlowNode::make_temp(temp_gen_.next());
  auto arr_tree = LocalFlowValueTree(arr_node);

  // Each argument gets an exact index
  for (uint32_t i = 0; i < instruction->srcs_size(); i++) {
    auto arg_reg = instruction->src(i);
    const auto& arg_tree = get_or_create(env, arg_reg);

    std::string index = std::to_string(i);
    constraints.add_edge(
        arg_tree.root(), LabelPath{Label{LabelKind::Inject, index}}, arr_node);
  }

  // Result is available via MOVE_RESULT
  pending_result_ = std::move(arr_tree);
}

void LocalFlowTransfer::analyze_throw(
    const IRInstruction* /* instruction */,
    RegisterEnvironment& /* env */,
    LocalFlowConstraintSet& /* constraints */) {
  // Deliberately a no-op. The thrown exception's data-flow edges are already
  // captured by the preceding invoke-direct <init> that constructs the
  // exception object. The throw instruction itself terminates normal flow
  // but does NOT produce an exception-output root — faithful to the v2 docs
  // which specify no exception channel at the callable level.
}

const LocalFlowValueTree& LocalFlowTransfer::get_or_create(
    RegisterEnvironment& env,
    reg_t reg) {
  auto it = env.find(reg);
  if (it != env.end()) {
    return it->second;
  }
  auto [inserted_it, _] = env.emplace(
      reg, LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen_.next())));
  return inserted_it->second;
}

LocalFlowNode LocalFlowTransfer::get_callee_node(
    const IRInstruction* instruction) const {
  auto* method_ref = instruction->get_method();
  auto opcode = instruction->opcode();

  std::string callee_sig;

  // Rich mode: use call graph for precise callee resolution
  if (call_graph_ != nullptr) {
    auto target = call_graph_->callee(method_, instruction);
    if (target.resolved()) {
      auto* callee_method = target.resolved_base_callee();
      if (callee_method != nullptr) {
        // Pointer comparison for SelfRec (Method* is unique per method)
        if (callee_method == method_) {
          return LocalFlowNode::make_self_rec();
        }
        callee_sig = callee_method->show();
      }
    }
  }

  // Lightweight mode: fall back to string-based resolution
  if (callee_sig.empty()) {
    callee_sig = show(method_ref);
  }

  // String-based self-recursion check (fallback when no call graph)
  if (call_graph_ == nullptr && callee_sig == method_->show()) {
    return LocalFlowNode::make_self_rec();
  }

  // Convert JVM separator "." to LFE separator "||"
  callee_sig = LocalFlowNode::jvm_to_lfe_separator(callee_sig);

  if (opcode == OPCODE_INVOKE_STATIC) {
    return LocalFlowNode::make_global(callee_sig);
  }

  // For direct calls: <init> is Global (statically resolved),
  // private methods on `this` are Meth (participate in class dispatch)
  if (opcode == OPCODE_INVOKE_DIRECT) {
    auto method_name = std::string(method_ref->get_name()->str());
    if (method_name != "<init>" && instruction->srcs_size() > 0) {
      auto receiver_reg = instruction->src(0);
      if (is_this_register(receiver_reg)) {
        return LocalFlowNode::make_meth(callee_sig);
      }
    }
    return LocalFlowNode::make_global(callee_sig);
  }

  // Super calls are monomorphic (statically resolved to superclass method)
  if (opcode == OPCODE_INVOKE_SUPER) {
    return LocalFlowNode::make_global(callee_sig);
  }

  // For virtual/interface calls, use Meth node (participates in dispatch)
  return LocalFlowNode::make_meth(callee_sig);
}

bool LocalFlowTransfer::is_this_register(reg_t reg) const {
  if (is_static_) {
    return false;
  }
  auto it = param_registers_.find(reg);
  return it != param_registers_.end() && it->second == 0;
}

const std::unordered_set<const DexType*>& LocalFlowTransfer::relevant_types()
    const {
  return relevant_types_;
}

std::string LocalFlowTransfer::format_call_site(
    const DexPosition* MT_NULLABLE position) {
  uint32_t index = invoke_counter_++;
  if (position == nullptr || position->file == nullptr) {
    return fmt::format("({},{})", 0, index);
  }
  return fmt::format("({},{})", position->line, index);
}

bool LocalFlowTransfer::should_skip_invoke(
    const IRInstruction* instruction) const {
  auto* method_ref = instruction->get_method();
  std::string_view class_name = method_ref->get_class()->str();
  std::string_view method_name = method_ref->get_name()->str();

  // Skip Kotlin intrinsic null-checks (checkNotNullParameter, etc.)
  if (class_name == "Lkotlin/jvm/internal/Intrinsics;" &&
      method_name.substr(0, 5) == "check") {
    return true;
  }

  // Skip trivial Object.<init>() — no data flow effect
  if (class_name == "Ljava/lang/Object;" && method_name == "<init>") {
    return true;
  }

  return false;
}

} // namespace local_flow
} // namespace marianatrench
