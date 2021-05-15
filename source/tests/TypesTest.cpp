/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <gtest/gtest.h>

#include <Creators.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <RedexContext.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class TypesTest : public test::Test {};

Context test_types(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_model_generation */ true);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.types = std::make_unique<Types>(context.stores);
  return context;
}

std::unordered_map<int, const DexType*> register_types_for_method(
    const Context& context,
    const Method* method) {
  auto* code = method->get_code();
  mt_assert(code->cfg_built());

  std::unordered_map<int, const DexType*> register_types;
  for (const auto* block : code->cfg().blocks()) {
    for (const auto& entry : *block) {
      const IRInstruction* instruction = entry.insn;
      for (auto register_id : instruction->srcs()) {
        auto* dex_type =
            context.types->register_type(method, instruction, register_id);
        if (dex_type) {
          register_types[register_id] = dex_type;
        }
      }
    }
  }
  return register_types;
}

} // anonymous namespace

TEST_F(TypesTest, LocalTypes) {
  Scope scope;

  auto* dex_method = redex::create_method(scope, "LClass;", R"(
    (method (public) "LClass;.foo:()V"
     (
      (new-instance "Ljava/lang/Object;")
      (move-result-pseudo-object v0)

      (load-param v1)
      (iput-object v1 v0 "LClass;.field:Ljava/lang/Object;")

      (return-void)
     )
    )
  )");

  auto context = test_types(scope);
  auto* method = context.methods->get(dex_method);
  auto register_types = register_types_for_method(context, method);

  EXPECT_EQ(
      register_types.at(0),
      DexType::make_type(DexString::make_string("Ljava/lang/Object;")));
  EXPECT_TRUE(register_types[1] == nullptr);
}
