/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/string_file.hpp>

#include <optional>

#include <gtest/gtest.h>

#include <Creators.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <RedexContext.h>
#include <TypeUtil.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class TypesTest : public test::Test {
 public:
  std::string temporary_directory() {
    return (boost::filesystem::path(__FILE__).parent_path() / "temp").string();
  }

 private:
  void SetUp() override {
    boost::filesystem::create_directory(this->temporary_directory());
  }

  void TearDown() override {
    boost::filesystem::remove_all(this->temporary_directory());
  }
};

std::string create_proguard_configuration_file(
    const std::string& directory,
    const std::string& file_name,
    const std::string& contents) {
  auto configuration_file = boost::filesystem::path(directory) / file_name;
  boost::filesystem::save_string_file(configuration_file, contents);
  return configuration_file.native();
}

Context test_types(
    const Scope& scope,
    std::optional<std::string> proguard_configuration_file = std::nullopt) {
  Context context;
  auto proguard_configuration_paths = std::vector<std::string>{};
  if (proguard_configuration_file) {
    proguard_configuration_paths.push_back(*proguard_configuration_file);
  }
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* proguard_configuration_paths */
      proguard_configuration_paths,
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_model_generation */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  redex::process_proguard_configurations(*context.options, context.stores);
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  return context;
}

std::unordered_map<int, const DexType*> register_types_for_method(
    const Context& context,
    const Method* method,
    bool resolve_reflection = false) {
  auto* code = method->get_code();
  mt_assert(code->cfg_built());

  std::unordered_map<int, const DexType*> register_types;
  for (const auto* block : code->cfg().blocks()) {
    for (const auto& entry : *block) {
      const IRInstruction* instruction = entry.insn;
      for (auto register_id : instruction->srcs()) {
        auto* dex_type =
            context.types->register_type(method, instruction, register_id);
        if (resolve_reflection && dex_type == type::java_lang_Class()) {
          auto* resolved_dex_type = context.types->register_const_class_type(
              method, instruction, register_id);
          dex_type =
              resolved_dex_type != nullptr ? resolved_dex_type : dex_type;
        }
        if (dex_type) {
          register_types[register_id] = dex_type;
        }
      }
    }
  }
  return register_types;
}

} // anonymous namespace

TEST_F(TypesTest, LocalIputTypes) {
  Scope scope;

  auto* dex_method = redex::create_method(scope, "LClass;", R"(
    (method (public) "LClass;.foo:()V"
     (
      (load-param-object v0)
      (new-instance "Ljava/lang/Object;")
      (move-result-pseudo-object v1)

      (iput-object v0 v1 "LClass;.field:Ljava/lang/Object;")

      (return-void)
     )
    )
  )");

  auto context = test_types(scope);
  auto* method = context.methods->get(dex_method);
  auto register_types = register_types_for_method(context, method);

  EXPECT_EQ(register_types.size(), 2);
  EXPECT_EQ(
      register_types.at(0),
      DexType::make_type(DexString::make_string("LClass;")));
  EXPECT_EQ(
      register_types.at(1),
      DexType::make_type(DexString::make_string("Ljava/lang/Object;")));
}

TEST_F(TypesTest, LocalInvokeDirectTypes) {
  Scope scope;

  redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_caller = redex::create_method(
      scope,
      "LCaller;",
      R"(
          (method (public) "LCaller;.caller:()V"
            (
              (new-instance "LCallee;")
              (move-result-object v0)
              (invoke-direct (v0) "LCallee;.callee:()V")
              (return-void)
            )
          )
      )");

  auto context = test_types(scope);
  auto* method = context.methods->get(dex_caller);
  auto register_types = register_types_for_method(context, method);

  EXPECT_EQ(register_types.size(), 1);
  EXPECT_EQ(
      register_types.at(0),
      DexType::make_type(DexString::make_string("LCallee;")));
}

TEST_F(TypesTest, LocalInvokeVirtualTypes) {
  Scope scope;

  auto* dex_callee = redex::create_void_method(scope, "LCallee;", "callee");
  redex::create_void_method(
      scope,
      /* class_name */ "LSubclass;",
      /* method */ "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_caller = redex::create_method(
      scope,
      "LCaller;",
      R"(
        (method (public) "LCaller;.caller:(LCallee;)V"
        (
          (load-param-object v0)
          (load-param-object v1)

          (invoke-virtual (v1) "LCallee;.callee:()V")
          (return-void)
        )
        )
      )");
  auto* dex_missing_invoke = redex::create_method(
      scope,
      "LNotACaller;",
      R"(
        (method (public) "LNotACaller;.caller:(LCallee;)V"
        (
          (load-param-object v0)
          (load-param-object v1)
          (return-void)
        )
        )
      )");

  auto context = test_types(scope);
  auto* method = context.methods->get(dex_caller);
  auto register_types = register_types_for_method(context, method);

  EXPECT_EQ(
      register_types.at(1),
      DexType::make_type(DexString::make_string("LCallee;")));
  EXPECT_TRUE(register_types[0] == nullptr);

  auto* missing_invoke_method = context.methods->get(dex_missing_invoke);
  auto missing_invoke_register_types =
      register_types_for_method(context, missing_invoke_method);
  assert(!root(dex_missing_invoke));
  EXPECT_TRUE(missing_invoke_register_types[1] == nullptr);
}

TEST_F(TypesTest, GlobalInvokeVirtualTypes) {
  Scope scope;

  auto* dex_callee = redex::create_void_method(scope, "LCallee;", "callee");
  redex::create_void_method(
      scope,
      /* class_name */ "LCalleeSubclass;",
      /* method */ "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_caller = redex::create_method(
      scope,
      "LCaller;",
      R"(
          (method (public) "LCaller;.caller:(LCallee;)V"
            (
              (load-param-object v0)
              (load-param-object v1)

              (new-instance "LCalleeSubclass;")
              (move-result-object v2)
              (invoke-virtual (v2) "LCallee;.callee:()V")

              (invoke-virtual (v1) "LCallee;.callee:()V")

              (return-void)
            )
          )
      )");
  auto* dex_entry_caller = redex::create_method(
      scope,
      "LEntryCaller;",
      R"(
          (method (public) "LEntryCaller;.entrycaller:()V"
          (
            (new-instance "LCalleeSubclass;")
            (move-result-object v0)
            (invoke-direct (v0) "LCalleeSubclass;.<init>:()V")

            (new-instance "LCaller;")
            (move-result-object v1)
            (invoke-direct (v1) "LCaller;.<init>:()V")

            (invoke-virtual (v1 v0) "LCaller;.caller:(LCallee;)V")
            (return-void)
          )
        )
      )");

  auto proguard_configuration = R"(
    -keep class EntryCaller {
        public void entrycaller();
    }
  )";
  auto proguard_configuration_file = create_proguard_configuration_file(
      this->temporary_directory(), "proguard.pro", proguard_configuration);

  auto context = test_types(scope, proguard_configuration_file);
  assert(!root(dex_callee));
  assert(!root(dex_caller));
  assert(root(dex_entry_caller));

  auto* caller = context.methods->get(dex_caller);
  auto caller_register_types = register_types_for_method(context, caller);

  EXPECT_EQ(
      caller_register_types.at(1),
      DexType::make_type(DexString::make_string("LCalleeSubclass;")));
  EXPECT_EQ(
      caller_register_types.at(2),
      DexType::make_type(DexString::make_string("LCalleeSubclass;")));

  auto* entry_method = context.methods->get(dex_entry_caller);
  auto entry_register_types = register_types_for_method(context, entry_method);

  EXPECT_EQ(
      entry_register_types.at(0),
      DexType::make_type(DexString::make_string("LCalleeSubclass;")));
  EXPECT_EQ(
      entry_register_types.at(1),
      DexType::make_type(DexString::make_string("LCaller;")));
}

TEST_F(TypesTest, NoProguardNarrowedGlobalFieldTypes) {
  Scope scope;
  auto* dex_virtual_method1 =
      redex::create_void_method(scope, "LSuper;", "virtual_method");
  redex::create_void_method(
      scope,
      "LSubclass;",
      "virtual_method",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_virtual_method1->get_class());

  // This example shows how global type analysis can be useful even with an
  // empty/absent proguard file. The benefits are limited, however, and show up
  // only in virtual method bodies/clinit methods and their callees (which redex
  // sets as entry points in addtion to the entry points specified in the
  // proguard)
  auto* dex_virtual_method2 = redex::create_methods_and_fields(
      scope,
      "LBase;",
      {{
          R"((
          method (public) "LBase;.storeField:()V"
          (
            (return-void)
          )
        ))"}},
      {{"field", dex_virtual_method1->get_class()}});
  std::vector<std::string> method_bodies = {
      {R"((
          method (public) "LClass;.storeField:()V"
          (
            (load-param-object v1)
            (new-instance "LSubclass;")
            (move-result-object v0)
            (invoke-direct (v0) "LSubclass;.<init>:()V")

            (iput-object v0 v1 "LClass;.field:LSuper;")
            (return-void)
          )
        ))"},
      {R"((
          method (public) "LClass;.readField:()LSuper;"
          (
            (load-param-object v1)
            (iget-object v1 "LClass;.field:LSuper;")
            (move-result-pseudo-object v0)

            (invoke-virtual (v0) "LSuper;.virtual_method:()V")

            (return-object v0)
          )
        ))"}};
  auto klass = redex::create_methods(
      scope, "LClass;", method_bodies, dex_virtual_method2->get_type());

  auto* dex_method2 = klass.at(1);

  DexStore store("test_store");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto method2 = context.methods->get(dex_method2);
  auto register_types2 = register_types_for_method(context, method2);
  // The type is narrowed from LSuper
  EXPECT_EQ(
      register_types2.at(0),
      DexType::make_type(DexString::make_string("LSubclass;")));
}

TEST_F(TypesTest, InvokeWithLocalReflectionArgument) {
  Scope scope;

  redex::create_class(scope, "LReflect;");
  auto dex_methods = redex::create_methods(
      scope,
      "LCaller;",
      {
          R"(
            (method (private) "LCaller;.reflect:(Ljava/lang/Class;)V"
            (
              (return-void)
            )
            ))",
          R"(
          (method (public) "LCaller;.caller:()V"
            (
              (load-param-object v0)

              (const-class "LReflect;")
              (move-result-pseudo-object v1)

              (invoke-direct (v0 v1) "LCaller;.reflect:(Ljava/lang/Class;)V")
              (return-void)
            )
          )
      )"});

  auto context = test_types(scope);
  auto* method = context.methods->get(dex_methods[1]);
  auto register_types =
      register_types_for_method(context, method, /* resolve_reflection */ true);

  EXPECT_EQ(
      register_types.at(0),
      DexType::make_type(DexString::make_string("LCaller;")));
  EXPECT_EQ(
      register_types.at(1),
      DexType::make_type(DexString::make_string("LReflect;")));
}

TEST_F(TypesTest, InvokeWithHopReflectionArgument) {
  Scope scope;

  redex::create_class(scope, "LReflect;");
  redex::create_method(
      scope,
      "LCallee;",
      R"(
            (method (public) "LCallee;.callee:()Ljava/lang/Class;"
            (
              (load-param-object v0)

              (const-class "LReflect;")
              (move-result-pseudo-object v1)
              (return-object v1)
            )
            ))");

  auto dex_caller = redex::create_method(
      scope,
      "LCaller;",
      R"(
          (method (public) "LCaller;.caller:()V"
            (
              (load-param-object v0)

              (new-instance "LCallee;")
              (move-result-object v1)

              (invoke-virtual (v1) "LCallee;.callee:()Ljava/lang/Class;")
              (move-result-pseudo-object v2)

              (invoke-direct (v0 v2) "LCaller;.reflect:(Ljava/lang/Class;)V")
              (return-void)
            )
          )
      )");

  auto context = test_types(scope);
  auto register_types_caller = register_types_for_method(
      context,
      context.methods->get(dex_caller),
      /* resolve_reflection */ true);
  EXPECT_EQ(register_types_caller.size(), 3);
  EXPECT_EQ(
      register_types_caller.at(0),
      DexType::make_type(DexString::make_string("LCaller;")));
  EXPECT_EQ(
      register_types_caller.at(1),
      DexType::make_type(DexString::make_string("LCallee;")));
  // Need interprocedural reflection analysis to resolve the type of `v2` to
  // `LReflect;`
  EXPECT_EQ(
      register_types_caller.at(2),
      DexType::make_type(DexString::make_string("Ljava/lang/Class;")));
}
