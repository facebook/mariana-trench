/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Fields.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/TaggedRootSet.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FrameTest : public test::Test {};

TEST_F(FrameTest, FrameConstructor) {
  EXPECT_TRUE(Frame().is_bottom());
  EXPECT_TRUE(Frame::bottom().is_bottom());
}

TEST_F(FrameTest, FrameLeq) {
  Scope scope;
  auto dex_fields = redex::create_fields(
      scope,
      /* class_name */ "LClassThree;",
      /* fields */
      {{"field1", type::java_lang_Boolean()},
       {"field2", type::java_lang_String()}});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(one, leaf);
  auto* two_origin = context.origin_factory->method_origin(two, leaf);

  auto* field_one = context.fields->get(dex_fields[0]);
  auto* field_two = context.fields->get(dex_fields[1]);

  EXPECT_TRUE(Frame::bottom().leq(Frame::bottom()));
  EXPECT_TRUE(Frame::bottom().leq(test::make_taint_frame(
      /* kind */ context.kind_factory->get("TestSource"),
      test::FrameProperties{})));
  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{})
                   .leq(Frame::bottom()));

  // Compare kind.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{})));
  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{})
                   .leq(test::make_taint_frame(
                       /* kind */ context.kind_factory->get("TestSink"),
                       test::FrameProperties{})));

  // Compare distances.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{.distance = 1})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{.distance = 0})));
  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{.distance = 0})
                   .leq(test::make_taint_frame(
                       /* kind */ context.kind_factory->get("TestSource"),
                       test::FrameProperties{.distance = 1})));

  // Compare origins.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{.origins = OriginSet{one_origin}})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{
                          .origins = OriginSet{one_origin, two_origin}})));
  EXPECT_FALSE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{.origins = OriginSet{one_origin, two_origin}})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.origins = OriginSet{one_origin}})));

  // Compare field origins.
  EXPECT_TRUE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .origins =
                  OriginSet{context.origin_factory->field_origin(field_one)}})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .origins = OriginSet{
                      context.origin_factory->field_origin(field_one),
                      context.origin_factory->field_origin(field_two)}})));
  EXPECT_FALSE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .origins =
                  OriginSet{
                      context.origin_factory->field_origin(field_one),
                      context.origin_factory->field_origin(field_two)}})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .origins = OriginSet{
                      context.origin_factory->field_origin(field_one)}})));

  // Compare inferred features.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{
                      .inferred_features = FeatureMayAlwaysSet::make_may(
                          {context.feature_factory->get("FeatureOne")})})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{
                          .inferred_features = FeatureMayAlwaysSet::make_may(
                              {context.feature_factory->get("FeatureOne"),
                               context.feature_factory->get("FeatureTwo")})})));
  EXPECT_FALSE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::make_may(
                  {context.feature_factory->get("FeatureOne"),
                   context.feature_factory->get("FeatureTwo")})})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet::make_may(
                      {context.feature_factory->get("FeatureOne")})})));

  // Compare user features.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{
                      .user_features = FeatureSet{context.feature_factory->get(
                          "FeatureOne")}})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{
                          .locally_inferred_features = {},
                          .user_features = FeatureSet{
                              context.feature_factory->get("FeatureOne"),
                              context.feature_factory->get("FeatureTwo")}})));
  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{
                       .user_features =
                           FeatureSet{
                               context.feature_factory->get("FeatureOne"),
                               context.feature_factory->get("FeatureTwo")}})
                   .leq(test::make_taint_frame(
                       /* kind */ context.kind_factory->get("TestSource"),
                       test::FrameProperties{
                           .user_features = FeatureSet{
                               context.feature_factory->get("FeatureOne")}})));

  // Compare via_type_of_ports
  EXPECT_TRUE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .via_type_of_ports = TaggedRootSet(
                  {TaggedRoot(Root(Root::Kind::Return), /* tag */ nullptr)})})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .via_type_of_ports = TaggedRootSet(
                      {TaggedRoot(Root(Root::Kind::Return), /* tag */ nullptr),
                       TaggedRoot(
                           Root(Root::Kind::Argument, 1),
                           /* tag */ nullptr)})})));
  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{
                       .via_type_of_ports = TaggedRootSet({TaggedRoot(
                           Root(Root::Kind::Return), /* tag */ nullptr)})})
                   .leq(test::make_taint_frame(
                       /* kind */ context.kind_factory->get("TestSource"),
                       test::FrameProperties{
                           .via_type_of_ports = TaggedRootSet({TaggedRoot(
                               Root(Root::Kind::Argument, 1),
                               /* tag */ nullptr)})})));

  // Compare canonical names.
  EXPECT_TRUE(test::make_taint_frame(
                  /* kind */ context.kind_factory->get("TestSource"),
                  test::FrameProperties{})
                  .leq(test::make_taint_frame(
                      /* kind */ context.kind_factory->get("TestSource"),
                      test::FrameProperties{
                          .canonical_names = CanonicalNameSetAbstractDomain{
                              CanonicalName(CanonicalName::TemplateValue{
                                  "%programmatic_leaf_name%"})}})));
  EXPECT_FALSE(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}})
          .leq(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{})));

  // Compare output paths.
  auto x = PathElement::field("x");
  EXPECT_TRUE(
      test::make_taint_frame(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          })
          .leq(test::make_taint_frame(
              context.kind_factory->local_return(),
              test::FrameProperties{test::FrameProperties{
                  .output_paths =
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              }})));
  EXPECT_FALSE(
      test::make_taint_frame(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          })
          .leq(test::make_taint_frame(
              context.kind_factory->local_return(),
              test::FrameProperties{test::FrameProperties{
                  .output_paths =
                      PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
              }})));
}

TEST_F(FrameTest, FrameEquals) {
  auto context = test::make_empty_context();

  EXPECT_TRUE(Frame::bottom().equals(Frame::bottom()));
  EXPECT_FALSE(Frame::bottom().equals(test::make_taint_frame(
      /* kind */ context.kind_factory->get("TestSource"),
      test::FrameProperties{})));

  EXPECT_FALSE(test::make_taint_frame(
                   /* kind */ context.kind_factory->get("TestSource"),
                   test::FrameProperties{})
                   .equals(Frame::bottom()));
}

TEST_F(FrameTest, FrameJoin) {
  Scope scope;
  auto dex_fields = redex::create_fields(
      scope,
      /* class_name */ "LClassThree;",
      /* fields */
      {{"field1", type::java_lang_Boolean()},
       {"field2", type::java_lang_String()}});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));
  auto* field_one = context.fields->get(dex_fields[0]);
  auto* field_two = context.fields->get(dex_fields[1]);
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(one, leaf);
  auto* two_origin = context.origin_factory->method_origin(two, leaf);

  EXPECT_EQ(Frame::bottom().join(Frame::bottom()), Frame::bottom());
  EXPECT_EQ(
      Frame::bottom().join(test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}));
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{})
          .join(Frame::bottom()),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}));

  // Test incompatible joins.
  EXPECT_THROW(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{})
          .join_with(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSink"),
              test::FrameProperties{})),
      std::exception);

  // Minimum distance.
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{.distance = 2})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.distance = 1})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{.distance = 1}));

  // Join origins.
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 1, .origins = OriginSet{one_origin}})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .distance = 1, .origins = OriginSet{two_origin}})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 1, .origins = OriginSet{one_origin, two_origin}}));

  // Join field origins
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 1,
              .origins =
                  OriginSet{context.origin_factory->field_origin(field_one)}})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .distance = 1,
                  .origins = OriginSet{context.origin_factory->field_origin(
                      field_two)}})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 1,
              .origins = OriginSet{
                  context.origin_factory->field_origin(field_one),
                  context.origin_factory->field_origin(field_two)}}));

  // Join inferred features.
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .inferred_features =
                  FeatureMayAlwaysSet{
                      context.feature_factory->get("FeatureOne")}})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .distance = 2,
                  .inferred_features =
                      FeatureMayAlwaysSet{
                          context.feature_factory->get("FeatureTwo")}})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .inferred_features = FeatureMayAlwaysSet::make_may(
                  {context.feature_factory->get("FeatureOne"),
                   context.feature_factory->get("FeatureTwo")})}));

  // Join user features.
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .user_features =
                  FeatureSet{context.feature_factory->get("FeatureOne")}})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .distance = 2,
                  .user_features =
                      FeatureSet{context.feature_factory->get("FeatureTwo")}})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .user_features = FeatureSet{
                  context.feature_factory->get("FeatureOne"),
                  context.feature_factory->get("FeatureTwo")}}));

  // Join via_type_of_ports
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .via_type_of_ports = TaggedRootSet(
                  {TaggedRoot(Root(Root::Kind::Return), /* tag */ nullptr)}),
          })
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .distance = 2,
                  .via_type_of_ports = TaggedRootSet({TaggedRoot(
                      Root(Root::Kind::Argument, 1), /* tag */ nullptr)}),
              })),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .distance = 2,
              .via_type_of_ports = TaggedRootSet(
                  {TaggedRoot(Root(Root::Kind::Return), /* tag */ nullptr),
                   TaggedRoot(
                       Root(Root::Kind::Argument, 1), /* tag */ nullptr)}),
          }));

  // Join canonical names.
  EXPECT_EQ(
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}})
          .join(test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{CanonicalName(
                          CanonicalName::TemplateValue{"%via_type_of%"})}})),
      test::make_taint_frame(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .canonical_names = CanonicalNameSetAbstractDomain{
                  CanonicalName(
                      CanonicalName::TemplateValue{"%programmatic_leaf_name%"}),
                  CanonicalName(
                      CanonicalName::TemplateValue{"%via_type_of%"})}}));

  // Join output paths.
  auto x = PathElement::field("x");
  auto y = PathElement::field("y");
  EXPECT_EQ(
      test::make_taint_frame(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          })
          .join(test::make_taint_frame(
              context.kind_factory->local_return(),
              test::FrameProperties{test::FrameProperties{
                  .output_paths =
                      PathTreeDomain{{Path{y}, CollapseDepth::zero()}},
              }})),
      test::make_taint_frame(
          context.kind_factory->local_return(),
          test::FrameProperties{test::FrameProperties{
              .output_paths =
                  PathTreeDomain{
                      {Path{x}, CollapseDepth::zero()},
                      {Path{y}, CollapseDepth::zero()},
                  },
          }})

  );
}

TEST_F(FrameTest, FrameWithKind) {
  Scope scope;
  auto dex_fields = redex::create_field(
      scope,
      /* class_name */ "LClassThree;",
      /* fields */
      {"field1", type::java_lang_Boolean()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  auto* field = context.fields->get(dex_fields);
  auto kind_a = context.kind_factory->get("TestSourceA");
  auto kind_b = context.kind_factory->get("TestSourceB");

  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* two_origin = context.origin_factory->method_origin(two, leaf);
  auto* field_origin = context.origin_factory->field_origin(field);

  auto frame1 = test::make_taint_frame(
      /* kind */ kind_a,
      test::FrameProperties{
          .distance = 5,
          .origins = OriginSet{two_origin, field_origin},
          .inferred_features = FeatureMayAlwaysSet::make_may(
              {context.feature_factory->get("FeatureOne"),
               context.feature_factory->get("FeatureTwo")}),
      });

  auto frame2 = frame1.with_kind(kind_b);
  EXPECT_EQ(frame1.distance(), frame2.distance());
  EXPECT_EQ(frame1.origins(), frame2.origins());
  EXPECT_EQ(frame1.inferred_features(), frame2.inferred_features());

  EXPECT_NE(frame1.kind(), frame2.kind());
  EXPECT_EQ(frame1.kind(), kind_a);
  EXPECT_EQ(frame2.kind(), kind_b);
}

TEST_F(FrameTest, SerializationDeserialization) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  const auto* callee = context.methods->create(
      redex::create_void_method(scope, "LClass;", "callee"));
  const auto* callee_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));

  auto declaration_call_info = CallInfo(
      /* callee */ nullptr,
      CallKind::declaration(),
      /* callee_port */ nullptr,
      /* position */ nullptr);
  auto origin_call_info = CallInfo(
      /* callee */ nullptr,
      CallKind::origin(),
      /* callee_port */ nullptr,
      /* position */ nullptr);
  auto callsite_call_info = CallInfo(
      callee, CallKind::callsite(), callee_port, /* position */ nullptr);

  {
    // Default frame
    auto frame = test::make_taint_frame(
        context.kind_factory->get("TestKind"),
        test::FrameProperties{
            .inferred_features = FeatureMayAlwaysSet::bottom()});
    auto frame_json =
        frame.to_json(origin_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(Frame::from_json(frame_json, origin_call_info, context), frame);
  }

  {
    // Frame with inferred and user features.
    auto frame = test::make_taint_frame(
        context.kind_factory->get("TestKind"),
        test::FrameProperties{
            .inferred_features =
                FeatureMayAlwaysSet{context.feature_factory->get("FeatureOne")},
            .user_features =
                FeatureSet{context.feature_factory->get("FeatureTwo")},
        });

    // For declaration, all (always) features are considered user features
    auto frame_json =
        frame.to_json(declaration_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, declaration_call_info, context),
        test::make_taint_frame(
            frame.kind(),
            test::FrameProperties{
                .inferred_features = FeatureMayAlwaysSet::bottom(),
                .user_features =
                    FeatureSet{
                        context.feature_factory->get("FeatureOne"),
                        context.feature_factory->get("FeatureTwo"),
                    },
            }));

    // For origin (and callsite), all features will be treated as inferred
    // features.
    frame_json = frame.to_json(origin_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, origin_call_info, context),
        test::make_taint_frame(
            frame.kind(),
            test::FrameProperties{
                .inferred_features =
                    FeatureMayAlwaysSet{
                        context.feature_factory->get("FeatureOne"),
                        context.feature_factory->get("FeatureTwo"),
                    },
            }));

    // For origin (and callsite), features will be treated as inferred features.
    frame_json = frame.to_json(callsite_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, callsite_call_info, context),
        test::make_taint_frame(
            frame.kind(),
            test::FrameProperties{
                .inferred_features =
                    FeatureMayAlwaysSet{
                        context.feature_factory->get("FeatureOne"),
                        context.feature_factory->get("FeatureTwo"),
                    },
            }));
  }

  {
    // Frame with user features only
    auto frame = test::make_taint_frame(
        context.kind_factory->get("TestKind"),
        test::FrameProperties{
            .inferred_features = FeatureMayAlwaysSet::bottom(),
            .user_features =
                FeatureSet{context.feature_factory->get("FeatureOne")},
        });

    // User features are retained for declaration frames.
    auto frame_json =
        frame.to_json(declaration_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, declaration_call_info, context), frame);

    // For origin (and callsite), features will be treated as inferred features.
    frame_json = frame.to_json(origin_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, origin_call_info, context),
        test::make_taint_frame(
            frame.kind(),
            test::FrameProperties{
                .inferred_features =
                    FeatureMayAlwaysSet{
                        context.feature_factory->get("FeatureOne"),
                    },
            }));

    frame_json = frame.to_json(callsite_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(
        Frame::from_json(frame_json, callsite_call_info, context),
        test::make_taint_frame(
            frame.kind(),
            test::FrameProperties{
                .inferred_features =
                    FeatureMayAlwaysSet{
                        context.feature_factory->get("FeatureOne"),
                    },
            }));
  }

  {
    // Frame with inferred may features.
    auto frame = test::make_taint_frame(
        context.kind_factory->get("TestKind"),
        test::FrameProperties{
            .inferred_features = FeatureMayAlwaysSet::make_may(
                {context.feature_factory->get("FeatureOne")}),
        });

    // For declaration, this frame cannot be parsed. May features are not
    // expected in declarations.
    auto frame_json =
        frame.to_json(declaration_call_info, ExportOriginsMode::Always);
    EXPECT_THROW(
        Frame::from_json(frame_json, declaration_call_info, context),
        JsonValidationError);

    // For origin and callsite, the features remain as inferred features.
    frame_json = frame.to_json(origin_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(Frame::from_json(frame_json, origin_call_info, context), frame);

    frame_json = frame.to_json(callsite_call_info, ExportOriginsMode::Always);
    EXPECT_EQ(Frame::from_json(frame_json, callsite_call_info, context), frame);
  }
}

} // namespace marianatrench
