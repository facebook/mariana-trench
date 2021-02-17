/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
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
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
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
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 0)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 0)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
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
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 0)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ FeatureMayAlwaysSet::bottom(),
               /* user_features */ FeatureSet::bottom())}},
      }));
}

TEST_F(ModelTest, LessOrEqual) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  EXPECT_TRUE(Model().leq(Model()));

  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ Model::Mode::Normal,
                  /* generations */ {})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ Model::Mode::Normal,
                      /* generations */ {})));
  EXPECT_TRUE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)}})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */
              {{AccessPath(Root(Root::Kind::Return)),
                Frame::leaf(source_kind)}})));
  EXPECT_TRUE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)}})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */
              {
                  {AccessPath(Root(Root::Kind::Return)),
                   Frame::leaf(source_kind)},
                  {AccessPath(Root(Root::Kind::Return)),
                   Frame::artificial_source(2)},
              })));
  EXPECT_FALSE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */
          {
              {AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)},
              {AccessPath(Root(Root::Kind::Return)),
               Frame::artificial_source(2)},
          })
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */
              {{AccessPath(Root(Root::Kind::Return)),
                Frame::leaf(source_kind)}})));

  EXPECT_TRUE(Model(
                  /* method */ nullptr,
                  context,
                  /* modes */ Model::Mode::Normal,
                  /* generations */ {},
                  /* parameter_sources */
                  {{AccessPath(Root(Root::Kind::Argument, 1)),
                    Frame::leaf(source_kind)}})
                  .leq(Model(
                      /* method */ nullptr,
                      context,
                      /* modes */ Model::Mode::Normal,
                      /* generations */ {},
                      /* parameter_sources */
                      {{AccessPath(Root(Root::Kind::Argument, 1)),
                        Frame::leaf(source_kind)},
                       {AccessPath(Root(Root::Kind::Argument, 2)),
                        Frame::leaf(source_kind)}})));
  EXPECT_FALSE(Model(
                   /* method */ nullptr,
                   context,
                   /* modes */ Model::Mode::Normal,
                   /* generations */ {},
                   /* parameter_sources */
                   {{AccessPath(Root(Root::Kind::Argument, 1)),
                     Frame::leaf(source_kind)}})
                   .leq(Model(
                       /* method */ nullptr,
                       context,
                       /* modes */ Model::Mode::Normal,
                       /* generations */ {},
                       /* parameter_sources */
                       {{AccessPath(Root(Root::Kind::Argument, 2)),
                         Frame::leaf(source_kind)}})));
  EXPECT_FALSE(
      Model(
          /* method */ nullptr,
          context,
          /* modes */ Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)}},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            Frame::leaf(source_kind)}})
          .leq(Model(
              /* method */ nullptr,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */
              {{AccessPath(Root(Root::Kind::Return)),
                Frame::leaf(source_kind)}},
              /* parameter_sources */
              {{AccessPath(Root(Root::Kind::Argument, 2)),
                Frame::leaf(source_kind)}})));
}

TEST_F(ModelTest, Join) {
  using PortTaint = std::pair<AccessPath, Taint>;

  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");
  const auto* sink_kind = context.kinds->get("TestSink");
  auto rule = std::make_unique<SourceSinkRule>(
      "rule", 1, "", Rule::KindSet{source_kind}, Rule::KindSet{sink_kind});

  Model model;
  EXPECT_EQ(model.issues().size(), 0);
  EXPECT_TRUE(model.generations().is_bottom());
  EXPECT_TRUE(model.sinks().is_bottom());

  Model model_with_trace(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */ {},
      /* attach_to_sources */ {},
      /* attach_to_sinks */ {},
      /* attach_to_propagations */ {},
      /* add_features_to_arguments */ {},
      /* inline_as */ AccessPathConstantDomain::bottom(),
      /* issues */
      IssueSet{Issue(
          /* source */ Taint{Frame::leaf(source_kind)},
          /* sink */ Taint{Frame::leaf(sink_kind)},
          rule.get(),
          context.positions->unknown())});
  model.join_with(model_with_trace);
  EXPECT_EQ(
      model.issues(),
      IssueSet{Issue(
          /* source */ Taint{Frame::leaf(source_kind)},
          /* sink */ Taint{Frame::leaf(sink_kind)},
          rule.get(),
          context.positions->unknown())});
  EXPECT_TRUE(model.generations().is_bottom());
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sources are added.
  Model model_with_source(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)}});
  model.join_with(model_with_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{Frame::leaf(source_kind)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  // Repeated application is idempotent.
  model.join_with(model_with_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{Frame::leaf(source_kind)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  Model model_with_other_source(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)), Frame::artificial_source(2)}});
  model.join_with(model_with_other_source);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{Frame::leaf(source_kind), Frame::artificial_source(2)}}));
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sinks are added.
  Model model_with_sink(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)), Frame::leaf(sink_kind)}});
  model.join_with(model_with_sink);
  EXPECT_THAT(
      model.generations().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Return)),
          Taint{Frame::leaf(source_kind), Frame::artificial_source(2)}}));
  EXPECT_THAT(
      model.sinks().elements(),
      testing::UnorderedElementsAre(PortTaint{
          AccessPath(Root(Root::Kind::Argument, 0)),
          Taint{Frame::leaf(sink_kind)}}));

  // Taint-in-taint-out is added.
  Model model_with_propagation(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ {},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
      });
  model.join_with(model_with_propagation);
  EXPECT_EQ(
      model.propagations(),
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ {},
               /* user_features */ {})}},
      }));
  Model model_with_more_propagation(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 3)),
               /* inferred_features */ {},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
      });
  model.join_with(model_with_more_propagation);
  EXPECT_EQ(
      model.propagations(),
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 3)),
               /* inferred_features */ {},
               /* user_features */ {})}},
      }));
  Model model_with_conflicting_propagation(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {{Propagation(
            /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
            /* inferred_features */ {},
            /* user_features */ {}),
        /* output */ AccessPath(Root(Root::Kind::Argument, 1))}});
  model.join_with(model_with_conflicting_propagation);
  EXPECT_EQ(
      model.propagations(),
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 1)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 3)),
               /* inferred_features */ {},
               /* user_features */ {})}},
      }));
  Model model_with_propagation_with_features(
      /* method */ nullptr,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */
               FeatureMayAlwaysSet{context.features->get("int-cast")},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */
               FeatureMayAlwaysSet{context.features->get("sanitize")},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
          {Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 3)),
               /* inferred_features */
               FeatureMayAlwaysSet{context.features->get("escape")},
               /* user_features */ {}),
           /* output */ AccessPath(Root(Root::Kind::Return))},
      });
  model.join_with(model_with_propagation_with_features);
  EXPECT_EQ(
      model.propagations(),
      (PropagationAccessPathTree{
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */
               FeatureMayAlwaysSet::make_may(
                   {context.features->get("int-cast"),
                    context.features->get("sanitize")}),
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Argument, 1)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
               /* inferred_features */ {},
               /* user_features */ {})}},
          {/* output */ AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ AccessPath(Root(Root::Kind::Argument, 3)),
               /* inferred_features */
               FeatureMayAlwaysSet::make_may({context.features->get("escape")}),
               /* user_features */ {})}},
      }));
}

} // namespace marianatrench
