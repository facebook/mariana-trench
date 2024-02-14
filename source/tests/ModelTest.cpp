/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <gmock/gmock.h>

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ModelTest : public test::Test {};

TEST_F(ModelTest, remove_kinds) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* removable_kind = context.kind_factory->get("RemoveMe");

  Model model_with_removable_kind(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Producer, 0)),
        test::make_leaf_taint_config(source_kind)}},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(sink_kind)},
       {AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(removable_kind)}});

  model_with_removable_kind.remove_kinds({removable_kind});

  Model model_without_removable_kind(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Producer, 0)),
        test::make_leaf_taint_config(source_kind)}},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(sink_kind)}});

  EXPECT_EQ(model_with_removable_kind, model_without_removable_kind);
}

TEST_F(ModelTest, remove_kinds_call_effects) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* removable_source_kind =
      context.kind_factory->get("RemoveMeSource");
  const auto* removable_sink_kind = context.kind_factory->get("RemoveMeSink");

  Model model_with_removable_kind(
      /* method */ nullptr, context);

  // Add call effect sources
  auto call_effect_port = AccessPath(Root(Root::Kind::CallEffectCallChain));
  model_with_removable_kind.add_call_effect_source(
      call_effect_port, test::make_leaf_taint_config(source_kind));
  model_with_removable_kind.add_call_effect_source(
      call_effect_port, test::make_leaf_taint_config(removable_source_kind));

  // Add call effect sinks
  model_with_removable_kind.add_call_effect_sink(
      call_effect_port, test::make_leaf_taint_config(sink_kind));
  model_with_removable_kind.add_call_effect_sink(
      call_effect_port, test::make_leaf_taint_config(removable_sink_kind));

  model_with_removable_kind.remove_kinds(
      {removable_source_kind, removable_sink_kind});

  Model model_without_removable_kind(
      /* method */ nullptr, context);
  // Add expected call effect source
  model_without_removable_kind.add_call_effect_source(
      call_effect_port, test::make_leaf_taint_config(source_kind));
  // Add expected call effect sink
  model_without_removable_kind.add_call_effect_sink(
      call_effect_port, test::make_leaf_taint_config(sink_kind));

  EXPECT_EQ(model_with_removable_kind, model_without_removable_kind);
}

TEST_F(ModelTest, ModelConstructor) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* dex_untracked_constructor = redex::create_void_method(
      scope,
      /* class_name */ "LUntrackedClassWithConstructor;",
      /* method_name */ "<init>",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "V",
      /* super */ nullptr);
  auto* untracked_constructor =
      context.methods->create(dex_untracked_constructor);
  auto model_with_untracked_constructor = Model(
      /* method */ untracked_constructor,
      context,
      /* modes */ Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis);
  EXPECT_EQ(
      model_with_untracked_constructor.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
      }));

  auto* dex_untracked_method_returning_void = redex::create_void_method(
      scope,
      /* class_name */ "LUntrackedClassWithMeturnReturningVoid;",
      /* method_name */ "returns_void",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "V",
      /* super */ nullptr);
  auto* untracked_method_returning_void =
      context.methods->create(dex_untracked_method_returning_void);
  auto model_with_untracked_method_returning_void = Model(
      /* method */ untracked_method_returning_void,
      context,
      /* modes */ Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis);
  EXPECT_EQ(
      model_with_untracked_method_returning_void.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
      }));

  auto* dex_untracked_method_returning_data = redex::create_void_method(
      scope,
      /* class_name */ "LUntrackedClassWithMethodReturningData;",
      /* method_name */ "returns_data",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "LData;",
      /* super */ nullptr);
  auto* untracked_method_returning_data =
      context.methods->create(dex_untracked_method_returning_data);
  auto model_with_untracked_method_returning_data = Model(
      /* method */ untracked_method_returning_data,
      context,
      /* modes */ Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis);
  EXPECT_EQ(
      model_with_untracked_method_returning_data.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 0)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_argument(0))}},
      }));

  auto* dex_untracked_static_method = redex::create_void_method(
      scope,
      /* class_name */ "LUntrackedClassWithStaticMethod;",
      /* method_name */ "static_method",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "LData;",
      /* super */ nullptr,
      /* is_static */ true);
  auto* untracked_static_method =
      context.methods->create(dex_untracked_static_method);
  auto model_with_untracked_static_method = Model(
      /* method */ untracked_static_method,
      context,
      /* modes */ Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis);
  EXPECT_EQ(
      model_with_untracked_static_method.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 0)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
      }));
}

TEST_F(ModelTest, LessOrEqual) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  EXPECT_TRUE(Model().leq(Model()));

  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ {},
                  /* frozen */ {},
                  /* generations */ {})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ {},
                      /* generations */ {})));
  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ {},
                  /* frozen */ {},
                  /* generations */
                  {{AccessPath(Root(Root::Kind::Return)),
                    test::make_leaf_taint_config(source_kind)}})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ {},
                      /* frozen */ {},
                      /* generations */
                      {{AccessPath(Root(Root::Kind::Return)),
                        test::make_leaf_taint_config(source_kind)}})));

  const auto* other_source_kind = context.kind_factory->get("OtherTestSource");
  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ {},
                  /* frozen */ {},
                  /* generations */
                  {{AccessPath(Root(Root::Kind::Return)),
                    test::make_leaf_taint_config(source_kind)}})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ {},
                      /* frozen */ {},
                      /* generations */
                      {
                          {AccessPath(Root(Root::Kind::Return)),
                           test::make_leaf_taint_config(source_kind)},
                          {AccessPath(Root(Root::Kind::Return)),
                           test::make_leaf_taint_config(other_source_kind)},
                      })));
  EXPECT_FALSE(Model(
                   /* method */ nullptr,
                   context,
                   /* modes */ {},
                   /* frozen */ {},
                   /* generations */
                   {
                       {AccessPath(Root(Root::Kind::Return)),
                        test::make_leaf_taint_config(source_kind)},
                       {AccessPath(Root(Root::Kind::Return)),
                        test::make_leaf_taint_config(other_source_kind)},
                   })
                   .leq(Model(
                       /* method */ nullptr,
                       context,
                       /* modes */ {},
                       /* frozen */ {},
                       /* generations */
                       {{AccessPath(Root(Root::Kind::Return)),
                         test::make_leaf_taint_config(source_kind)}})));

  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ {},
                  /* frozen */ {},
                  /* generations */ {},
                  /* parameter_sources */
                  {{AccessPath(Root(Root::Kind::Argument, 1)),
                    test::make_leaf_taint_config(source_kind)}})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ {},
                      /* frozen */ {},
                      /* generations */ {},
                      /* parameter_sources */
                      {{AccessPath(Root(Root::Kind::Argument, 1)),
                        test::make_leaf_taint_config(source_kind)},
                       {AccessPath(Root(Root::Kind::Argument, 2)),
                        test::make_leaf_taint_config(source_kind)}})));
  EXPECT_FALSE(Model(
                   /* method */ nullptr,
                   context,
                   /* modes */ {},
                   /* frozen */ {},
                   /* generations */ {},
                   /* parameter_sources */
                   {{AccessPath(Root(Root::Kind::Argument, 1)),
                     test::make_leaf_taint_config(source_kind)}})
                   .leq(Model(
                       /* method */ nullptr,
                       context,
                       /* modes */ {},
                       /* frozen */ {},
                       /* generations */ {},
                       /* parameter_sources */
                       {{AccessPath(Root(Root::Kind::Argument, 2)),
                         test::make_leaf_taint_config(source_kind)}})));
  EXPECT_FALSE(Model(
                   /* method */ nullptr,
                   context,
                   /* modes */ {},
                   /* frozen */ {},
                   /* generations */
                   {{AccessPath(Root(Root::Kind::Return)),
                     test::make_leaf_taint_config(source_kind)}},
                   /* parameter_sources */
                   {{AccessPath(Root(Root::Kind::Argument, 1)),
                     test::make_leaf_taint_config(source_kind)}})
                   .leq(Model(
                       /* method */ nullptr,
                       context,
                       /* modes */ {},
                       /* frozen */ {},
                       /* generations */
                       {{AccessPath(Root(Root::Kind::Return)),
                         test::make_leaf_taint_config(source_kind)}},
                       /* parameter_sources */
                       {{AccessPath(Root(Root::Kind::Argument, 2)),
                         test::make_leaf_taint_config(source_kind)}})));

  EXPECT_TRUE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {
              PropagationConfig(
                  /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
                  /* kind */ context.kind_factory->local_return(),
                  /* output_paths */
                  PathTreeDomain{
                      {Path{PathElement::field("x")}, CollapseDepth::zero()}},
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ {}),
          })
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */
              {
                  PropagationConfig(
                      /* input_path */ AccessPath(
                          Root(Root::Kind::Argument, 1)),
                      /* kind */ context.kind_factory->local_return(),
                      /* output_paths */
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                      /* locally_inferred_features */
                      FeatureMayAlwaysSet::bottom(),
                      /* user_features */ {}),
              })));

  // Compare global_sanitizers
  EXPECT_FALSE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */
          {Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain::top())})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */
              {Sanitizer(
                  SanitizerKind::Propagations,
                  KindSetAbstractDomain::top())})));
  EXPECT_TRUE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */
          {Sanitizer(
              SanitizerKind::Sources,
              KindSetAbstractDomain({context.kind_factory->get("Kind")}))})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */
              {Sanitizer(
                  SanitizerKind::Sources, KindSetAbstractDomain::top())})));

  // Compare port sanitizers
  EXPECT_TRUE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */
          {{Root(Root::Kind::Return),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sources,
                KindSetAbstractDomain({context.kind_factory->get("Kind")})))}})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers*/
              {{Root(Root::Kind::Return),
                SanitizerSet(Sanitizer(
                    SanitizerKind::Sources, KindSetAbstractDomain::top()))},
               {Root(Root::Kind::Argument, 1),
                SanitizerSet(Sanitizer(
                    SanitizerKind::Propagations,
                    KindSetAbstractDomain::top()))}})));
  EXPECT_FALSE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */
          {{Root(Root::Kind::Return),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sources, KindSetAbstractDomain::top()))}})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers*/
              {{Root(Root::Kind::Argument, 1),
                SanitizerSet(Sanitizer(
                    SanitizerKind::Sources, KindSetAbstractDomain::top()))}})));

  // With Frozen
  EXPECT_FALSE(Model(
                   /* method */ nullptr,
                   context,
                   /* modes */ {},
                   /* frozen */ Model::FreezeKind::Generations,
                   /* generations */ {})
                   .leq(Model(
                       /* method */ nullptr,
                       context,
                       /* modes */ {},
                       /* generations */ {})));
  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ {},
                  /* frozen */ {},
                  /* generations */
                  {{AccessPath(Root(Root::Kind::Return)),
                    test::make_leaf_taint_config(source_kind)}})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ {},
                      /* frozen */ Model::FreezeKind::Generations,
                      /* generations */
                      {})));

  // Only the frozen FreezeKind is affected.
  Model model_with_frozen_generation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ Model::FreezeKind::Generations,
      /* generations */ {},
      /* parameter_sources */ {});
  Model model_with_frozen_parameter_sources(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ Model::FreezeKind::Generations,
      /* generations */ {},
      /* parameter_sources */
      {{AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(source_kind)}});

  EXPECT_TRUE(
      model_with_frozen_generation.leq(model_with_frozen_parameter_sources));
  EXPECT_FALSE(
      model_with_frozen_parameter_sources.leq(model_with_frozen_generation));
}

TEST_F(ModelTest, Join) {
  using PortTaint = std::pair<AccessPath, Taint>;

  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  auto rule = std::make_unique<SourceSinkRule>(
      "rule",
      1,
      "",
      Rule::KindSet{source_kind},
      Rule::KindSet{sink_kind},
      /* transforms */ nullptr);

  Model model;
  EXPECT_EQ(model.issues().size(), 0);
  EXPECT_TRUE(model.generations().is_bottom());
  EXPECT_TRUE(model.sinks().is_bottom());

  Model model_with_trace(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */ {},
      /* port_sanitizers */ {},
      /* attach_to_sources */ {},
      /* attach_to_sinks */ {},
      /* attach_to_propagations */ {},
      /* add_features_to_arguments */ {},
      /* inline_as_getter */ AccessPathConstantDomain::bottom(),
      /* inline_as_setter */ SetterAccessPathConstantDomain::bottom(),
      /* model_generators */ {},
      /* issues */
      IssueSet{Issue(
          /* source */ Taint{test::make_leaf_taint_config(source_kind)},
          /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
          rule.get(),
          /* callee */ std::string(k_return_callee),
          /* sink_index */ 0,
          context.positions->unknown())});
  model.join_with(model_with_trace);
  EXPECT_EQ(
      model.issues(),
      IssueSet{Issue(
          /* source */ Taint{test::make_leaf_taint_config(source_kind)},
          /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
          rule.get(),
          /* callee */ std::string(k_return_callee),
          /* sink_index */ 0,
          context.positions->unknown())});
  EXPECT_TRUE(model.generations().is_bottom());
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sources are added.
  Model model_with_source(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}});
  model.join_with(model_with_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{test::make_leaf_taint_config(source_kind)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  // Repeated application is idempotent.
  model.join_with(model_with_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{test::make_leaf_taint_config(source_kind)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  const auto* other_source_kind = context.kind_factory->get("OtherTestSource");
  Model model_with_other_source(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(other_source_kind)}});
  model.join_with(model_with_other_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{
              test::make_leaf_taint_config(source_kind),
              test::make_leaf_taint_config(other_source_kind)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sinks are added.
  Model model_with_sink(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink_kind)}});
  model.join_with(model_with_sink);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{
              test::make_leaf_taint_config(source_kind),
              test::make_leaf_taint_config(other_source_kind)}}));
  EXPECT_THAT(
      model.sinks().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Argument, 0)),
          Taint{test::make_leaf_taint_config(sink_kind)}}));

  // Taint-in-taint-out is added.
  Model model_with_propagation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              /* inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 2)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              /* inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
      });
  model.join_with(model_with_propagation);
  EXPECT_EQ(
      model.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
      }));
  Model model_with_more_propagation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{
                  {Path{PathElement::field("x")}, CollapseDepth::zero()}},
              /* inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 3)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{
                  {Path{PathElement::field("x")}, CollapseDepth::zero()}},
              /* inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
      });
  model.join_with(model_with_more_propagation);
  EXPECT_EQ(
      model.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 3)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return(),
               /* input_paths */
               PathTreeDomain{
                   {Path{PathElement::field("x")}, CollapseDepth::zero()}},
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ {})}},
      }));
  Model model_with_conflicting_propagation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {{PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{
              {Path{PathElement::field("y")}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {})}});
  model.join_with(model_with_conflicting_propagation);
  EXPECT_EQ(
      model.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 3)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return(),
               /* input_paths */
               PathTreeDomain{
                   {Path{PathElement::field("x")}, CollapseDepth::zero()}},
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ {})}},
      }));
  Model model_with_propagation_with_features(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              /* inferred_features */
              FeatureMayAlwaysSet{context.feature_factory->get("int-cast")},
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              /* inferred_features */
              FeatureMayAlwaysSet{context.feature_factory->get("sanitize")},
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
          PropagationConfig(
              /* input_path */ AccessPath(Root(Root::Kind::Argument, 3)),
              /* kind */ context.kind_factory->local_return(),
              /* output_paths */
              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              /* inferred_features */
              FeatureMayAlwaysSet{context.feature_factory->get("escape")},
              /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
              /* user_features */ {}),
      });
  model.join_with(model_with_propagation_with_features);
  EXPECT_EQ(
      model.propagations(),
      (TaintAccessPathTree{
          {/* input */ AccessPath(Root(Root::Kind::Argument, 1)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return(),
               /* input_paths */
               PathTreeDomain{{Path{}, CollapseDepth::zero()}},
               /* inferred_features */
               FeatureMayAlwaysSet::make_may(
                   {context.feature_factory->get("int-cast"),
                    context.feature_factory->get("sanitize")}),
               /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ {})}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 2)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return())}},
          {/* input */ AccessPath(Root(Root::Kind::Argument, 3)),
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return(),
               /* input_paths */
               PathTreeDomain{{Path{}, CollapseDepth::zero()}},
               /* inferred_features */
               FeatureMayAlwaysSet{context.feature_factory->get("escape")},
               /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ {})}},
      }));

  // Join models with global sanitizers
  const auto* kind1 = context.kind_factory->get("Kind1");
  const auto* kind2 = context.kind_factory->get("Kind2");
  auto model_with_sanitizers = Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */
      {Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain({kind1}))});
  model_with_sanitizers.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */
      {Sanitizer(SanitizerKind::Propagations, KindSetAbstractDomain::top())}));
  EXPECT_EQ(
      model_with_sanitizers,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */
          {Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain({kind1})),
           Sanitizer(
               SanitizerKind::Propagations, KindSetAbstractDomain::top())}));

  model_with_sanitizers.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */
      {Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain({kind2}))}));
  EXPECT_EQ(
      model_with_sanitizers,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */
          {Sanitizer(
               SanitizerKind::Sources, KindSetAbstractDomain({kind1, kind2})),
           Sanitizer(
               SanitizerKind::Propagations, KindSetAbstractDomain::top())}));

  // Join models with port sanitizers
  auto model_with_port_sanitizers = Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */ {},
      /* port_sanitizers */
      {{Root(Root::Kind::Return),
        SanitizerSet(Sanitizer(
            SanitizerKind::Sources, KindSetAbstractDomain({kind1})))}});
  model_with_port_sanitizers.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */ {},
      /* port_sanitizers */
      {{Root(Root::Kind::Argument, 1),
        SanitizerSet(
            Sanitizer(SanitizerKind::Sinks, KindSetAbstractDomain::top()))}}));
  EXPECT_EQ(
      model_with_port_sanitizers,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */
          {{Root(Root::Kind::Return),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sources, KindSetAbstractDomain({kind1})))},
           {Root(Root::Kind::Argument, 1),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sinks, KindSetAbstractDomain::top()))}}));
  model_with_port_sanitizers.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* global_sanitizers */ {},
      /* port_sanitizers */
      {{Root(Root::Kind::Return),
        SanitizerSet(Sanitizer(
            SanitizerKind::Sources, KindSetAbstractDomain::top()))}}));
  EXPECT_EQ(
      model_with_port_sanitizers,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */
          {{Root(Root::Kind::Return),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sources, KindSetAbstractDomain::top()))},
           {Root(Root::Kind::Argument, 1),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sinks, KindSetAbstractDomain::top()))}}));

  // Join with Frozen
  Model model_with_frozen_generation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ Model::FreezeKind::Generations,
      /* generations */ {});

  model_with_frozen_generation.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}}));

  EXPECT_EQ(
      model_with_frozen_generation,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ Model::FreezeKind::Generations,
          /* generations */ {}));

  // Only the frozen FreezeKind is affected.
  model_with_frozen_generation.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink_kind)}}));

  EXPECT_EQ(
      model_with_frozen_generation,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ Model::FreezeKind::Generations,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {{AccessPath(Root(Root::Kind::Argument, 0)),
            test::make_leaf_taint_config(sink_kind)}}));

  model_with_frozen_generation.join_with(Model(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ Model::FreezeKind::Sinks,
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}},
      /* parameter_sources */ {},
      /* sinks */
      {}));

  EXPECT_EQ(
      model_with_frozen_generation,
      Model(
          /* method */ nullptr,
          context,
          /* modes */ {},
          /* frozen */ Model::FreezeKind::Generations |
              Model::FreezeKind::Sinks,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {}));
}

TEST_F(ModelTest, SourceKinds) {
  auto context = test::make_empty_context();
  const auto* source_kind1 = context.kind_factory->get("TestSource1");
  const auto* source_kind2 = context.kind_factory->get("TestSource2");
  const auto* sink_kind = context.kind_factory->get("TestSink");

  Model model;
  EXPECT_EQ(0, model.source_kinds().size());

  Model model_with_generation(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind1)}});
  EXPECT_THAT(
      model_with_generation.source_kinds(),
      testing::UnorderedElementsAre(source_kind1));

  Model model_with_parameter_sources(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(source_kind1)},
       {AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(source_kind2)}});
  EXPECT_THAT(
      model_with_parameter_sources.source_kinds(),
      testing::UnorderedElementsAre(source_kind1, source_kind2));

  Model model_with_call_effect_source;
  model_with_call_effect_source.add_call_effect_source(
      AccessPath(Root(Root::Kind::CallEffectIntent)),
      test::make_leaf_taint_config(source_kind1));
  EXPECT_THAT(
      model_with_call_effect_source.source_kinds(),
      testing::UnorderedElementsAre(source_kind1));

  Model model_with_sources_and_sinks(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind1)}},
      /* parameter_sources */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(source_kind2)}},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink_kind)}});
  EXPECT_THAT(
      model_with_sources_and_sinks.source_kinds(),
      testing::UnorderedElementsAre(source_kind1, source_kind2));
}

TEST_F(ModelTest, SinkKinds) {
  auto context = test::make_empty_context();
  const auto* sink_kind1 = context.kind_factory->get("TestSink1");
  const auto* sink_kind2 = context.kind_factory->get("TestSink2");
  const auto* source_kind = context.kind_factory->get("TestSource");

  Model model;
  EXPECT_EQ(0, model.sink_kinds().size());

  Model model_with_sink(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(sink_kind1)}});
  EXPECT_THAT(
      model_with_sink.sink_kinds(), testing::UnorderedElementsAre(sink_kind1));

  Model model_with_call_effect_sink;
  model_with_call_effect_sink.add_call_effect_sink(
      AccessPath(Root(Root::Kind::CallEffectIntent)),
      test::make_leaf_taint_config(sink_kind1));
  EXPECT_THAT(
      model_with_call_effect_sink.sink_kinds(),
      testing::UnorderedElementsAre(sink_kind1));

  Model model_with_sources_and_sinks(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink_kind1)},
       {AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink_kind2)}});
  EXPECT_THAT(
      model_with_sources_and_sinks.sink_kinds(),
      testing::UnorderedElementsAre(sink_kind1, sink_kind2));
}

TEST_F(ModelTest, PropagationTransforms) {
  auto context = test::make_empty_context();

  const auto* local_return_kind = context.kind_factory->local_return();
  const auto* transform1 =
      context.transforms_factory->create_transform("Transform1");
  const auto* transform_kind1 = context.kind_factory->transform_kind(
      /* base_kind */ local_return_kind,
      /* local_transforms */
      context.transforms_factory->create({"Transform1"}, context),
      /* global_transforms */ nullptr);
  const auto* transform_kind2 = context.kind_factory->transform_kind(
      /* base_kind */ local_return_kind,
      /* local_transforms */ nullptr,
      /* global_transforms */
      context.transforms_factory->create({"Transform2"}, context));

  const auto input_path = AccessPath(Root(Root::Kind::Argument, 0));
  const auto output_path = AccessPath(Root(Root::Kind::Return));

  Model model;
  EXPECT_EQ(0, model.local_transform_kinds().size());

  Model model_with_transforms(
      /* method */ nullptr,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {test::make_propagation_config(transform_kind1, input_path, output_path),
       test::make_propagation_config(transform_kind2, input_path, output_path),
       test::make_propagation_config(
           local_return_kind, input_path, output_path)});
  EXPECT_THAT(
      model_with_transforms.local_transform_kinds(),
      testing::UnorderedElementsAre(transform1));
}

} // namespace marianatrench
