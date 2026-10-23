/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <ControlFlow.h>
#include <IRCode.h>
#include <TypeUtil.h>

#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/ForwardAliasTransfer.h>
#include <mariana-trench/ForwardTaintTransfer.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ForwardTaintTransferTest : public test::Test {
 protected:
  void SetUp() override {
    Scope scope;
    auto* parent = redex::create_methods_and_fields(
        scope,
        "LParent;",
        {},
        {{"inherited", type::java_lang_Object()}},
        /* super */ nullptr,
        /* fields_are_static */ true);
    redex::create_class(scope, "LChild;", parent->get_type());
    auto* klass = redex::create_methods_and_fields(
        scope,
        "LClass;",
        {{R"(
      (method (public static) "LClass;.method:()V"
        (
          (const v0 0)
          (sput-object v0 "LClass;.stored:Ljava/lang/Object;")
          (sput-object v0 "LClass;.stored:Ljava/lang/Object;")
          (sget-object "LClass;.stored:Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (sget-object "LClass;.clean:Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (sput-object v0 "LParent;.inherited:Ljava/lang/Object;")
          (sget-object "LChild;.inherited:Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (sput-object v0 "LChild;.inherited:Ljava/lang/Object;")
          (sget-object "LParent;.inherited:Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (return-void)
        )
      )
    )"}},
        {{"stored", type::java_lang_Object()},
         {"clean", type::java_lang_Object()}},
        /* super */ nullptr,
        /* fields_are_static */ true);
    auto* dex_method = klass->get_dmethods().at(0);
    DexStore store("store");
    store.add_classes(scope);
    context_ = std::make_unique<Context>(test::make_context(store));
    context_->class_intervals = std::make_unique<ClassIntervals>(
        *context_->options,
        context_->options->analysis_mode(),
        context_->stores);
    auto* method = context_->methods->get(dex_method);
    registry_ = std::make_unique<Registry>(*context_);
    previous_model_ = std::make_unique<Model>(method, *context_);
    new_model_ = std::make_unique<Model>(method, *context_);
    method_context_ = std::make_unique<MethodContext>(
        *context_,
        *registry_,
        *previous_model_,
        *new_model_);

    for (const auto& entry :
         cfg::InstructionIterable(dex_method->get_code()->cfg())) {
      if (entry.insn->opcode() == OPCODE_SGET_OBJECT) {
        reads_.push_back(entry.insn);
      } else if (entry.insn->opcode() == OPCODE_SPUT_OBJECT) {
        writes_.push_back(entry.insn);
      }
    }
    ASSERT_EQ(reads_.size(), 4);
    ASSERT_EQ(writes_.size(), 4);
    for (const auto* instruction : reads_) {
      ASSERT_TRUE(
          context_->call_graph->resolved_field_access(method, instruction)
              .has_value());
    }
  }

  TaintTree source_taint() const {
    return TaintTree{Taint{test::make_leaf_taint_config(
        context_->kind_factory->get("TestSource"))}};
  }

  MemoryLocationsDomain read_static_field(
      std::size_t index,
      ForwardAliasEnvironment& aliases,
      ForwardTaintEnvironment& environment) {
    const auto* instruction = reads_.at(index);
    ForwardAliasTransfer::analyze_sget(
        method_context_.get(),
        instruction,
        &aliases);
    auto result = aliases.memory_locations(k_result_register);
    method_context_->aliasing.store(
        instruction,
        InstructionAliasResults{
            {},
            aliases.make_widening_resolver(),
            result,
            nullptr});
    ForwardTaintTransfer::analyze_sget(
        method_context_.get(),
        instruction,
        &environment);
    return result;
  }

  void write_static_field(
      std::size_t index,
      ForwardAliasEnvironment& aliases,
      ForwardTaintEnvironment& environment) {
    const auto* instruction = writes_.at(index);
    auto registers = memory_location_map_from_environment(
        aliases.memory_location_environment(),
        instruction);
    ForwardAliasTransfer::analyze_sput(
        method_context_.get(),
        instruction,
        &aliases);
    method_context_->aliasing.store(
        instruction,
        InstructionAliasResults{
            std::move(registers),
            aliases.make_widening_resolver(),
            std::nullopt,
            nullptr});
    ForwardTaintTransfer::analyze_sput(
        method_context_.get(),
        instruction,
        &environment);
  }

  std::vector<const IRInstruction*> reads_;
  std::vector<const IRInstruction*> writes_;
  std::unique_ptr<Context> context_;
  std::unique_ptr<Registry> registry_;
  std::unique_ptr<Model> previous_model_;
  std::unique_ptr<Model> new_model_;
  std::unique_ptr<MethodContext> method_context_;
};

TEST_F(ForwardTaintTransferTest, StaticReadPreservesStoredTaint) {
  auto aliases = ForwardAliasEnvironment::initial();
  auto environment = ForwardTaintEnvironment::initial();
  ParameterMemoryLocation source(0);
  auto taint = source_taint();
  environment.write(&source, taint, UpdateKind::Strong);
  aliases.assign(0, &source);
  write_static_field(0, aliases, environment);

  auto result = read_static_field(0, aliases, environment);

  EXPECT_EQ(
      result,
      MemoryLocationsDomain{method_context_->memory_factory.make_static_field(
          reads_[0]->get_field())});
  EXPECT_EQ(environment.read(result), taint);
}

TEST_F(ForwardTaintTransferTest, StaticReadReplacesPreviouslyTaintedResult) {
  auto aliases = ForwardAliasEnvironment::initial();
  auto environment = ForwardTaintEnvironment::initial();
  ParameterMemoryLocation previous_result(0);
  auto taint = source_taint();
  environment.write(&previous_result, taint, UpdateKind::Strong);
  aliases.assign(k_result_register, &previous_result);

  auto result = read_static_field(1, aliases, environment);

  EXPECT_NE(result, MemoryLocationsDomain{&previous_result});
  EXPECT_TRUE(environment.read(result).is_bottom());
  EXPECT_EQ(environment.read(&previous_result), taint);
}

TEST_F(ForwardTaintTransferTest, StaticWriteRetainsTaintAfterCleanOverwrite) {
  auto aliases = ForwardAliasEnvironment::initial();
  auto environment = ForwardTaintEnvironment::initial();
  ParameterMemoryLocation source(0);
  ParameterMemoryLocation clean(1);
  auto taint = source_taint();
  environment.write(&source, taint, UpdateKind::Strong);
  aliases.assign(0, &source);
  write_static_field(0, aliases, environment);
  aliases.assign(0, &clean);
  write_static_field(1, aliases, environment);

  auto result = read_static_field(0, aliases, environment);

  EXPECT_EQ(environment.read(result), taint);
}

TEST_F(ForwardTaintTransferTest, StaticReadConservativelyObservesLaterWrites) {
  auto aliases = ForwardAliasEnvironment::initial();
  auto environment = ForwardTaintEnvironment::initial();
  auto saved_result = read_static_field(0, aliases, environment);
  ASSERT_TRUE(environment.read(saved_result).is_bottom());
  ParameterMemoryLocation source(0);
  auto taint = source_taint();
  environment.write(&source, taint, UpdateKind::Strong);
  aliases.assign(0, &source);

  write_static_field(0, aliases, environment);

  // Reads share field storage, so even an earlier read can observe later taint.
  // This records the conservative abstraction, not precise local-value
  // behavior.
  EXPECT_EQ(environment.read(saved_result), taint);
}

TEST_F(ForwardTaintTransferTest, InheritedStaticFieldReferencesShareStorage) {
  for (std::size_t index : {2, 3}) {
    SCOPED_TRACE(index);
    ASSERT_NE(writes_[index]->get_field(), reads_[index]->get_field());
    auto write_target = context_->call_graph->resolved_field_access(
        method_context_->method(),
        writes_[index]);
    auto read_target = context_->call_graph->resolved_field_access(
        method_context_->method(),
        reads_[index]);
    ASSERT_TRUE(write_target.has_value());
    ASSERT_TRUE(read_target.has_value());
    ASSERT_EQ(write_target->field, read_target->field);
    auto aliases = ForwardAliasEnvironment::initial();
    auto environment = ForwardTaintEnvironment::initial();
    ParameterMemoryLocation source(0);
    auto taint = source_taint();
    environment.write(&source, taint, UpdateKind::Strong);
    aliases.assign(0, &source);

    write_static_field(index, aliases, environment);
    auto result = read_static_field(index, aliases, environment);

    EXPECT_EQ(environment.read(result), taint);
  }
}

} // namespace marianatrench
