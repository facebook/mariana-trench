/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_set>

#include <gmock/gmock.h>

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaintTest : public test::Test {};

TEST_F(TaintTest, Insertion) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* method_two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto one_origin = context.origin_factory->method_origin(method_one, leaf);
  auto two_origin = context.origin_factory->method_origin(method_two, leaf);

  auto* position_one = context.positions->get(std::nullopt, 1);
  auto* position_two = context.positions->get(std::nullopt, 2);

  Taint taint;
  EXPECT_EQ(taint, Taint());

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("TestSource"),
      test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
      }));

  // Add frame with a different kind.
  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("OtherSource"),
      test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
      }));

  // Add frame with yet another kind.
  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = method_one,
          .call_position = position_one,
          .distance = 2,
          .origins = OriginSet{one_origin},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = position_one,
                  .distance = 2,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Add frame with different distance.
  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = method_one,
          .call_position = position_one,
          .distance = 3,
          .origins = OriginSet{two_origin},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = position_one,
                  .distance = 2,
                  .origins = OriginSet{one_origin, two_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Add frame with different callee.
  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = method_two,
          .call_position = position_one,
          .distance = 3,
          .origins = OriginSet{two_origin},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = position_one,
                  .distance = 2,
                  .origins = OriginSet{one_origin, two_origin},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_two,
                  .call_position = position_one,
                  .distance = 3,
                  .origins = OriginSet{two_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Add frame with different call position.
  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = method_one,
          .call_position = position_two,
          .distance = 2,
          .origins = OriginSet{one_origin},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = position_one,
                  .distance = 2,
                  .origins = OriginSet{one_origin, two_origin},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = position_two,
                  .distance = 2,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_two,
                  .call_position = position_one,
                  .distance = 3,
                  .origins = OriginSet{two_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Verify local positions are shared for a given call info.
  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("A"),
          test::FrameProperties{
              .local_positions = LocalPositionSet{position_one}}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("B"), test::FrameProperties{})};
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("A"),
              test::FrameProperties{
                  .local_positions = LocalPositionSet{position_one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("B"),
              test::FrameProperties{
                  .local_positions = LocalPositionSet{position_one}})}));
}

TEST_F(TaintTest, Leq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);

  // Comparison to bottom
  EXPECT_TRUE(Taint::bottom().leq(Taint::bottom()));
  EXPECT_TRUE(Taint::bottom().leq(
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((Taint{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .leq(Taint::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .leq(Taint{test::make_taint_config(
              test_kind_one, test::FrameProperties{})}));

  // Same position, different kinds
  EXPECT_TRUE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(Taint{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{.call_position = test_position_one}),
          }));
  EXPECT_FALSE(
      (Taint{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{.call_position = test_position_one}),
       })
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));

  // Different positions
  EXPECT_TRUE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(Taint{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_two}),
          }));
  EXPECT_FALSE(
      (Taint{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_two})})
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));

  // Same kind, different port
  EXPECT_TRUE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = method_one,
               .call_position = test_position_one,
               .distance = 1,
               .origins = OriginSet{one_origin},
               .call_kind = CallKind::callsite(),
           })})
          .leq(Taint{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = method_one,
                      .call_position = test_position_one,
                      .distance = 1,
                      .origins = OriginSet{one_origin},
                      .call_kind = CallKind::callsite(),
                  }),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                      .callee = method_one,
                      .call_position = test_position_one,
                      .distance = 1,
                      .origins = OriginSet{one_origin},
                      .call_kind = CallKind::callsite(),
                  }),
          }));
  EXPECT_FALSE(
      (Taint{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = method_one,
                   .call_position = test_position_one,
                   .distance = 1,
                   .origins = OriginSet{one_origin},
                   .call_kind = CallKind::callsite(),
               }),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .callee = method_one,
                   .call_position = test_position_one,
                   .distance = 1,
                   .origins = OriginSet{one_origin},
                   .call_kind = CallKind::callsite(),
               }),
       })
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_one,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              })}));

  // Different kinds
  EXPECT_TRUE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = method_one,
               .call_position = test_position_one,
               .distance = 1,
               .origins = OriginSet{one_origin},
               .call_kind = CallKind::callsite(),
           })})
          .leq(Taint{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = method_one,
                      .call_position = test_position_one,
                      .distance = 1,
                      .origins = OriginSet{one_origin},
                      .call_kind = CallKind::callsite(),
                  }),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = method_one,
                      .call_position = test_position_one,
                      .distance = 1,
                      .origins = OriginSet{one_origin},
                      .call_kind = CallKind::callsite(),
                  }),
          }));
  EXPECT_FALSE(
      (Taint{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = method_one,
                   .call_position = test_position_one,
                   .distance = 1,
                   .origins = OriginSet{one_origin},
                   .call_kind = CallKind::callsite(),
               }),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = method_one,
                   .call_position = test_position_one,
                   .distance = 1,
                   .origins = OriginSet{one_origin},
                   .call_kind = CallKind::callsite(),
               })})
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_one,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              })}));

  // Different callee ports
  EXPECT_TRUE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})})
          .leq(Taint{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0))}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 1))}),
          }));
  EXPECT_FALSE(
      (Taint{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})})
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));

  // Different callee ports
  EXPECT_TRUE(
      Taint{test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = AccessPath(Root(Root::Kind::Return))})}
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return))})}));
  EXPECT_FALSE(
      Taint{test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = AccessPath(Root(Root::Kind::Return))})}
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
  EXPECT_FALSE(
      Taint{test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = AccessPath(
                        Root(Root::Kind::Argument, 0),
                        Path{PathElement::field("x")})})}
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
  EXPECT_FALSE(
      Taint{test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}
          .leq(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{PathElement::field("x")})})}));

  // Local kinds.
  EXPECT_TRUE(Taint{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{
                      {Path{PathElement::field("x")}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })}
                  .leq(Taint{test::make_taint_config(
                      context.kind_factory->local_return(),
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .output_paths =
                              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                          .call_kind = CallKind::propagation(),
                      })}));
  EXPECT_FALSE(Taint{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })}
                   .leq(Taint{test::make_taint_config(
                       context.kind_factory->local_return(),
                       test::FrameProperties{
                           .callee_port = AccessPath(Root(Root::Kind::Return)),
                           .output_paths =
                               PathTreeDomain{
                                   {Path{PathElement::field("x")},
                                    CollapseDepth::zero()}},
                           .call_kind = CallKind::propagation(),
                       })}));

  // Local kinds with output paths.
  EXPECT_TRUE(Taint{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{
                      {Path{PathElement::field("x")}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })}
                  .leq(Taint{test::make_taint_config(
                      context.kind_factory->local_return(),
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .output_paths =
                              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                          .call_kind = CallKind::propagation(),
                      })}));
  EXPECT_FALSE(Taint{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })}
                   .leq(Taint{test::make_taint_config(
                       context.kind_factory->local_return(),
                       test::FrameProperties{
                           .callee_port = AccessPath(Root(Root::Kind::Return)),
                           .output_paths =
                               PathTreeDomain{
                                   {Path{PathElement::field("x")},
                                    CollapseDepth::zero()}},
                           .call_kind = CallKind::propagation(),
                       })}));
}

TEST_F(TaintTest, Equals) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  // Comparison to bottom
  EXPECT_TRUE(Taint::bottom().equals(Taint::bottom()));
  EXPECT_FALSE(Taint::bottom().equals(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_one})}));
  EXPECT_FALSE((Taint{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .equals(Taint::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .equals(Taint{test::make_taint_config(
              test_kind_one, test::FrameProperties{})}));

  // Different positions
  EXPECT_FALSE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .equals(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Different ports
  EXPECT_FALSE(
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = method_one,
               .call_position = test_position_one,
               .distance = 1,
               .origins = OriginSet{one_origin},
               .call_kind = CallKind::callsite(),
           })})
          .equals(Taint{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_one,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              })}));

  // Different kinds
  EXPECT_FALSE(
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .equals(Taint{test::make_taint_config(
              test_kind_two, test::FrameProperties{})}));

  // Different output paths
  auto two_input_paths = Taint(
      {test::make_taint_config(
           context.kind_factory->local_return(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Return)),
               .output_paths =
                   PathTreeDomain{{Path{x, y}, CollapseDepth::zero()}},
               .call_kind = CallKind::propagation(),
           }),
       test::make_taint_config(
           context.kind_factory->local_return(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Return)),
               .output_paths =
                   PathTreeDomain{{Path{x, z}, CollapseDepth::zero()}},
               .call_kind = CallKind::propagation(),
           })});
  EXPECT_FALSE(two_input_paths.equals(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })}));
}

TEST_F(TaintTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);

  // Join with bottom
  EXPECT_EQ(
      Taint::bottom().join(Taint{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(Taint::bottom()),
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})});

  auto taint =
      (Taint{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee = method_one, .call_kind = CallKind::callsite()})})
          .join(Taint::bottom());
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one, .call_kind = CallKind::callsite()})});

  taint = Taint::bottom().join(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee = method_one, .call_kind = CallKind::callsite()})});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one, .call_kind = CallKind::callsite()})});

  // Join different kinds
  taint =
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})};
  taint.join_with(
      Taint{test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(test_kind_one, test::FrameProperties{}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // Join same kind
  auto frame_one = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = method_one,
          .distance = 1,
          .origins = OriginSet{one_origin},
          .call_kind = CallKind::callsite(),
      });
  auto frame_two = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = method_one,
          .distance = 2,
          .origins = OriginSet{one_origin},
          .call_kind = CallKind::callsite(),
      });
  taint = Taint{frame_one};
  taint.join_with(Taint{frame_two});
  EXPECT_EQ(taint, (Taint{frame_one}));

  // Join different positions
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_one})};
  taint.join_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_two})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Join same position, same kind, different frame properties.
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_one}})};
  taint.join_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_two}})});
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one, feature_two},
                  /* always */ FeatureSet{}),
          })}));

  // Join different ports
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})};
  taint.join_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1))}),
      }));

  // Join same ports (different kinds)
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})};
  taint.join_with(Taint{test::make_taint_config(
      test_kind_two,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));

  // Callee ports only consist of the root. Input paths trees are joined.
  taint = Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths =
              PathTreeDomain{
                  {Path{PathElement::field("x")}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })};
  taint.join_with(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })});

  // Join different ports with same prefix, for non-artificial kinds
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(
              Root(Root::Kind::Argument, 0), Path{PathElement::field("x")})})};
  taint.join_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{PathElement::field("x")})}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));
  EXPECT_NE(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{PathElement::field("x")})}),
      }));
  EXPECT_NE(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));
}

TEST_F(TaintTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* method_three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* feature_three = context.feature_factory->get("FeatureThree");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");
  auto* user_feature_three = context.feature_factory->get("UserFeatureThree");
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);
  auto* two_origin = context.origin_factory->method_origin(method_two, leaf);
  auto* three_origin =
      context.origin_factory->method_origin(method_three, leaf);
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  Taint taint;
  Taint initial_taint;

  // Tests with empty left hand side.
  taint.difference_with(Taint{});
  EXPECT_TRUE(taint.is_bottom());

  taint.difference_with(Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(taint.is_bottom());

  initial_taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite(),
          }),
  };

  taint = initial_taint;
  taint.difference_with(Taint{});
  EXPECT_EQ(taint, initial_taint);

  taint = initial_taint;
  taint.difference_with(initial_taint);
  EXPECT_TRUE(taint.is_bottom());

  // Left hand side is bigger than right hand side in terms of the `Frame.leq`
  // operation.
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(taint, initial_taint);

  // Left hand side is smaller than right hand side in terms of the `Frame.leq`
  // operation.
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  // Left hand side and right hand side are incomparably different at the
  // `Frame` level (different features).
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(taint, initial_taint);

  // Left hand side and right hand side have different positions.
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(taint, initial_taint);

  // Left hand side is smaller than right hand side (by one position).
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  // Left hand side has more positions than right hand side.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee = method_one,
          .call_position = test_position_one,
          .call_kind = CallKind::callsite(),
      })});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee = method_one,
                  .call_position = test_position_two,
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Left hand side is smaller for one position, and larger for another.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          })});
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          })}));

  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_three,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_three},
              .user_features = FeatureSet{user_feature_three},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 2,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_three,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{two_origin},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_three,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{three_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("SomeOtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_three,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("SomeOtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{two_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  initial_taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
  };

  // Left hand side and right hand side have different user features.
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(taint, initial_taint);

  // Left hand side and right hand side have different callee_ports.
  taint = initial_taint;
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(taint, initial_taint);

  // Left hand side is smaller than right hand side (with one kind).
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  // Left hand side has more kinds than right hand side.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = method_one,
          .call_position = test_position_one,
          .distance = 1,
          .origins = OriginSet{one_origin},
          .call_kind = CallKind::callsite(),
      })});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_one,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Left hand side is smaller for one kind, and larger for another.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          })});
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .call_kind = CallKind::callsite(),
          })}));

  // Both sides contain access paths
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  // lhs.local_positions <= rhs.local_positions
  // lhs.frames <= rhs.frames
  taint =
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})};
  taint.difference_with(Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_TRUE(taint.is_bottom());

  // lhs.local_positions <= rhs.local_positions
  // lhs.frames > rhs.frames
  taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  taint.difference_with(Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .local_positions = LocalPositionSet{test_position_one}})});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(test_kind_two, test::FrameProperties{})});

  // lhs.local_positions > rhs.local_positions
  // lhs.frames > rhs.frames
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  taint.difference_with(
      Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .local_positions = LocalPositionSet{test_position_one}}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // lhs.local_positions > rhs.local_positions
  // lhs.frames <= rhs.frames
  taint = Taint{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .local_positions = LocalPositionSet{test_position_one}})};
  taint.difference_with(Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}})});

  // lhs.output_paths <= rhs.output_paths
  // lhs.frames <= rhs.frames
  taint = Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })};
  taint.difference_with(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })});
  EXPECT_TRUE(taint.is_bottom());

  // lhs.output_paths <= rhs.output_paths
  // lhs.frames > rhs.frames
  taint = Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .inferred_features = FeatureMayAlwaysSet{feature_one},
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })};
  taint.difference_with(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })});

  // lhs.output_paths > rhs.output_paths
  // lhs.frames <= rhs.frames
  taint = Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })};
  taint.difference_with(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })});

  // lhs.output_paths > rhs.output_paths
  // lhs.frames > rhs.frames
  taint = Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .inferred_features = FeatureMayAlwaysSet{feature_one},
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })};
  taint.difference_with(Taint{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_kind = CallKind::propagation(),
      })});
  EXPECT_EQ(
      taint,
      Taint{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })});
}

TEST_F(TaintTest, AddMethodOrigins) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);
  auto* two_origin = context.origin_factory->method_origin(method_two, leaf);

  auto taint = Taint{
      // Only the Declaration frames should be affected
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{.origins = OriginSet{two_origin}}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = method_two, .call_kind = CallKind::callsite()}),
  };
  taint.add_origins_if_declaration(method_one, leaf);
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.origins = OriginSet{one_origin}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{
                  .origins = OriginSet{one_origin, two_origin}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = method_two, .call_kind = CallKind::callsite()}),
      }));
}

TEST_F(TaintTest, AddFieldOrigins) {
  Scope scope;
  const auto* field_one = redex::create_field(
      scope, "LClassA", {"field_one", type::java_lang_String()});
  const auto* field_two = redex::create_field(
      scope, "LClassB", {"field_two", type::java_lang_String()});

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* one = context.fields->get(field_one);
  auto* two = context.fields->get(field_two);

  auto* one_origin = context.origin_factory->field_origin(one);
  auto* two_origin = context.origin_factory->field_origin(two);

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{.origins = OriginSet{two_origin}}),
  };

  taint.add_origins_if_declaration(one);
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.origins = OriginSet{one_origin}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{
                  .origins = OriginSet{one_origin, two_origin}}),
      }));
}

TEST_F(TaintTest, LocallyInferredFeatures) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  auto* position_one = context.positions->get(std::nullopt, 1);

  // Basic case with only one frame.
  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ method_one,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet{feature_one});

  // Port does not have features
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ method_one,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Argument))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet::bottom());

  // Position does not have features
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ method_one,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ position_one)),
      FeatureMayAlwaysSet::bottom());

  // Frames with different call info.
  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::origin()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ nullptr,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet{feature_one});
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ nullptr,
          /* call_kind */ CallKind::origin(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet{feature_two});

  // Frames with different callee.
  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ method_one,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet{feature_one});
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ nullptr,
          /* call_kind */ CallKind::callsite(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      taint.locally_inferred_features(CallInfo(
          /* callee */ method_one,
          /* call_kind */ CallKind::origin(),
          /* callee_port */
          AccessPathFactory::singleton().get(
              AccessPath(Root(Root::Kind::Leaf))),
          /* position */ nullptr)),
      FeatureMayAlwaysSet::bottom());
}

TEST_F(TaintTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* method_three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));
  auto* four = context.methods->create(
      redex::create_void_method(scope, "LFour;", "four"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get("Test.java", 2);

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* feature_three = context.feature_factory->get("FeatureThree");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");
  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);
  auto* two_origin = context.origin_factory->method_origin(method_two, leaf);
  auto* three_origin =
      context.origin_factory->method_origin(method_three, leaf);

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .origins = OriginSet{one_origin},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::origin()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 2,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_three,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two},
              .call_kind = CallKind::callsite()})};

  // When propagating, all user features become inferred features.
  EXPECT_EQ(
      taint.propagate(
          /* callee */ four,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
          /* call_position */ context.positions->get("Test.java", 1),
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ FeatureMayAlwaysSet{feature_three},
          /* context */ context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .callee = four,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{user_feature_one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .callee = four,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 2,
                  .origins = OriginSet{two_origin, three_origin},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{user_feature_two, feature_two},
                      /* always */
                      FeatureSet{user_feature_one, feature_one}),
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three},
                  .call_kind = CallKind::callsite()}),
      }));

  /**
   * The following `Taint` looks like (callee == nullptr):
   *
   * position_one  -> kind_one -> Frame(port=arg(0), distance=1,
   *                                    local_features=Always(feature_one))
   *                  kind_two -> Frame(port=arg(0), distance=1)
   */
  auto non_crtex_frames = Taint{
      /* call_position == test_position_one */
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position_one,
              .distance = 1,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
      /* call_position == nullptr */
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .distance = 1,
              .call_kind = CallKind::callsite()}),
  };
  /**
   * After propagating with
   *   (callee=two, callee_port=arg(1), call_position=test_position_two):
   *
   * Taint: (callee == method_one)
   * position_two -> kind_one -> Frame(port=arg(1), distance=2)
   *                                   inferred_feature=May(feature_one))
   *                 kind_two -> Frame(port=arg(1), distance=2)
   *
   * Intuition:
   * `frames.propagate(one, arg(1), test_position_two)` is called when we see a
   * callsite like `one(arg0, arg1)`, and are processing the models for arg1 at
   * that callsite (which is at test_position_two).
   *
   * The callee, position, and ports after `propagate` should be what is passed
   * to propagate.
   *
   * For each kind in the original `frames`, the propagated frame should have
   * distance = min(all_distances_for_that_kind) + 1, with the exception of
   * "Anchor" frames which always have distance = 0.
   *
   * Locally inferred features are explicitly set to `bottom()` because these
   * should be propagated into inferred features (joined across each kind).
   */
  EXPECT_EQ(
      non_crtex_frames.propagate(
          /* callee */ method_one,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* call_position */ test_position_two,
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ {},
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_one,
                  .call_position = test_position_two,
                  .distance = 2,
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one},
                      /* always */ FeatureSet{feature_one}),
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_one,
                  .call_position = test_position_two,
                  .distance = 2,
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::callsite()}),
      }));
  /**
   * The following `Taint` looks like (callee == nullptr):
   *
   * position_one  -> kind_one -> Frame(port=anchor, distance=0)
   * null position -> Frame(port=anchor, distance=0)
   *
   * NOTE: Realistically, we wouldn't normally have frames with distance > 0 if
   * callee == nullptr. However, we need callee == nullptr to test the "Anchor"
   * port scenarios (otherwise they are ignored and treated as regular ports).
   */
  auto crtex_frames = Taint{
      /* call_position == test_position_one */
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .call_position = test_position_one,
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_kind = CallKind::declaration()}),
      /* call_position == nullptr */
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},

              .call_kind = CallKind::declaration()}),
  };

  /**
   * After propagating with
   *   (callee=two, callee_port=arg(1), call_position=test_position_two):
   *
   * Taint: (callee == method_one)
   * position_two -> kind_one -> Frame(port=anchor:arg(0), distance=0)
   *                 kind_two -> Frame(port=anchor:arg(0), distance=0)
   *
   * Intuition:
   * `frames.propagate(method_one, arg(1), test_position_two)` is called when we
   * see a callsite like `method_one(arg0, arg1)`, and are processing the models
   * for arg1 at that callsite (which is at test_position_two).
   *
   * "Anchor" frames behave slightly differently, in that the port
   * is "canonicalized" such that the `this` parameter has index arg(-1) for
   * non-static methods, and the first parameter starts at index arg(0).
   *
   * For each kind in the original `frames`, the propagated frame should have
   * distance = min(all_distances_for_that_kind) + 1, with the exception of
   * "Anchor" frames which always have distance = 0.
   *
   * Locally inferred features are explicitly set to `bottom()` because these
   * should be propagated into inferred features (joined across each kind).
   */
  auto expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{method_one->signature()});
  auto expected_callee_port = AccessPath(
      Root(Root::Kind::Anchor), Path{PathElement::field("Argument(0)")});
  auto* expected_origin = context.origin_factory->crtex_origin(
      *expected_instantiated_name.instantiated_value(),
      context.access_path_factory->get(expected_callee_port));
  EXPECT_EQ(
      crtex_frames.propagate(
          /* callee */ method_one,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* call_position */ test_position_two,
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ {},
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = expected_callee_port,
                  .callee = nullptr,
                  .call_position = test_position_two,
                  .class_interval_context = CallClassIntervalContext(
                      ClassIntervals::Interval::top(),
                      /* preserves_type_context*/ true),
                  .origins = OriginSet{expected_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_kind = CallKind::origin()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = expected_callee_port,
                  .callee = nullptr,
                  .call_position = test_position_two,
                  .class_interval_context = CallClassIntervalContext(
                      ClassIntervals::Interval::top(),
                      /* preserves_type_context*/ true),
                  .origins = OriginSet{expected_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_kind = CallKind::origin()}),
      }));

  // It is generally expected (though not enforced) that frames within
  // `Taint` have the same callee because of the `Taint`
  // structure. They typically also share the same "callee_port" because they
  // share the same `Position`. However, for testing purposes, we use
  // different callees and callee ports.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::origin()}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_one,
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = OriginSet{one_origin},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_kind = CallKind::declaration()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::origin()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = OriginSet{one_origin},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_kind = CallKind::declaration()}),
  };

  expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{method_two->signature()});
  expected_callee_port = AccessPath(
      Root(Root::Kind::Anchor), Path{PathElement::field("Argument(-1)")});
  expected_origin = context.origin_factory->crtex_origin(
      *expected_instantiated_name.instantiated_value(),
      context.access_path_factory->get(expected_callee_port));
  EXPECT_EQ(
      taint.propagate(
          /* callee */ method_two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          test_position_one,
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ {},
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin, two_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::callsite()}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = expected_callee_port,
                  .callee = nullptr,
                  .call_position = test_position_one,
                  .class_interval_context = CallClassIntervalContext(
                      ClassIntervals::Interval::top(),
                      /* preserves_type_context */ true),
                  .origins = OriginSet{one_origin, expected_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_kind = CallKind::origin()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{one_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = expected_callee_port,
                  .callee = nullptr,
                  .call_position = test_position_one,
                  .class_interval_context = CallClassIntervalContext(
                      ClassIntervals::Interval::top(),
                      /* preserves_type_context */ true),
                  .origins = OriginSet{one_origin, expected_origin},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_kind = CallKind::origin()}),
      }));

  // Propagating this frame will give it a distance of 2. It is expected to be
  // dropped as it exceeds the maximum distance allowed.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .distance = 1,
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.propagate(
          /* callee */ method_two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          test_position_one,
          /* maximum_source_sink_distance */ 1,
          /* extra_features */ {},
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      Taint::bottom());

  // One of the two frames will be ignored during propagation because its
  // distance exceeds the maximum distance allowed.
  taint = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = method_one,
              .distance = 2,
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = method_one,
              .distance = 1,
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.propagate(
          /* callee */ method_two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          test_position_one,
          /* maximum_source_sink_distance */ 2,
          /* extra_features */ {},
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 2,
                  .inferred_features = FeatureMayAlwaysSet{user_feature_two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::callsite()}),
      }));
}

TEST_F(TaintTest, TransformKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two = context.methods->create(
      redex::create_void_method(scope, "LTwo;", "method_two"));
  auto* method_three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");

  auto* leaf =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* one_origin = context.origin_factory->method_origin(method_one, leaf);
  auto* two_origin = context.origin_factory->method_origin(method_two, leaf);
  auto* three_origin =
      context.origin_factory->method_origin(method_three, leaf);

  auto* test_source = context.kind_factory->get("TestSource");
  auto* transformed_test_source =
      context.kind_factory->get("TransformedTestSource");
  auto* transformed_test_source2 =
      context.kind_factory->get("TransformedTestSource2");

  auto initial_taint = Taint{
      test::make_taint_config(
          /* kind */ test_source,
          test::FrameProperties{
              .origins = OriginSet{one_origin},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_two,
              .call_position = test_position_one,
              .distance = 2,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = method_three,
              .call_position = test_position_one,
              .distance = 1,
              .origins = OriginSet{three_origin},
              .inferred_features =
                  FeatureMayAlwaysSet{feature_one, feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };

  // This works the same way as filter.
  auto empty_taint = initial_taint;
  empty_taint.transform_kind_with_features(
      [](const auto* /* unused kind */) { return std::vector<const Kind*>(); },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(empty_taint, Taint::bottom());

  // This actually performs a transformation.
  auto map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source,
       transformed_test_source](const auto* kind) -> std::vector<const Kind*> {
        if (kind == test_source) {
          return {transformed_test_source};
        }
        return {kind};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 2,
                  .origins = OriginSet{two_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_three,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{three_origin},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Another transformation. Covers mapping transformed frames.
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source](const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{transformed_test_source};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_two](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_two};
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_two,
                  .call_position = test_position_one,
                  .distance = 2,
                  .origins = OriginSet{two_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = method_three,
                  .call_position = test_position_one,
                  .distance = 1,
                  .origins = OriginSet{three_origin},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Tests one -> many transformations (with features).
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source, transformed_test_source2](
          const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{
              test_source, transformed_test_source, transformed_test_source2};
        }
        return std::vector<const Kind*>{};
      },
      [feature_one](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_one};
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ test_source,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source2,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests transformations with features added to specific kinds.
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source, transformed_test_source2](
          const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{
              transformed_test_source, transformed_test_source2};
        }
        return std::vector<const Kind*>{};
      },
      [&](const auto* transformed_kind) {
        if (transformed_kind == transformed_test_source) {
          return FeatureMayAlwaysSet{feature_one};
        }
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source2,
              test::FrameProperties{
                  .origins = OriginSet{one_origin},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Transformation where multiple old kinds map to the same new kind
  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource1"),
          test::FrameProperties{
              .callee = method_two,
              .call_position = test_position_one,
              .origins = OriginSet{two_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource2"),
          test::FrameProperties{
              .callee = method_two,
              .call_position = test_position_one,
              .origins = OriginSet{three_origin},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.transform_kind_with_features(
      [&](const auto* /* unused kind */) -> std::vector<const Kind*> {
        return {transformed_test_source};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              transformed_test_source,
              test::FrameProperties{
                  .callee = method_two,
                  .call_position = test_position_one,
                  .origins = OriginSet{two_origin, three_origin},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, AppendOutputPaths) {
  auto context = test::make_empty_context();

  const auto path_element1 = PathElement::field("field1");
  const auto path_element2 = PathElement::field("field2");

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{{Path{path_element1}, CollapseDepth(4)}},
              .call_kind = CallKind::propagation(),
          })};

  taint.append_to_propagation_output_paths(path_element2);
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .output_paths =
                      PathTreeDomain{
                          {Path{path_element1, path_element2},
                           CollapseDepth(3)}},
                  .call_kind = CallKind::propagation(),
              })}));
}

TEST_F(TaintTest, UpdateNonDeclarationPositions) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* method_three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* leaf_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Leaf)));
  auto* anchor_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Anchor)));
  auto* method1_origin =
      context.origin_factory->method_origin(method_one, leaf_port);
  auto* method2_origin =
      context.origin_factory->method_origin(method_two, leaf_port);
  auto* crtex_origin =
      context.origin_factory->crtex_origin("canonical_name", anchor_port);

  auto dex_position1 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 1);
  auto dex_position2 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 2);
  auto dex_position3 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 3);

  auto position1 = context.positions->get(method_one, &dex_position1);
  auto position2 = context.positions->get(method_two, &dex_position2);
  auto position3 = context.positions->get(method_two, &dex_position3);

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("LeafFrame"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame1"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = method_one,
              .call_position = position1,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame2"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method_two,
              .call_position = position2,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame3"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method_three,
              .call_position = position3,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OriginFrame1"),
          test::FrameProperties{
              .callee_port = *leaf_port,
              .callee = nullptr,
              .call_position = position1,
              .origins = OriginSet{method1_origin, method2_origin},
              .call_kind = CallKind::origin(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OriginFrame2"),
          test::FrameProperties{
              .callee_port = *anchor_port,
              .callee = nullptr,
              .call_position = position1,
              .origins = OriginSet{crtex_origin},
              .call_kind = CallKind::origin(),
          }),
  };

  taint = taint.update_non_declaration_positions(
      [&](const Method* callee,
          const AccessPath* callee_port,
          const Position* position) {
        if (callee == method_one) {
          return context.positions->get(
              position, /* line */ 10, /* start */ 11, /* end */ 12);
        } else if (*callee_port == AccessPath(Root(Root::Kind::Argument))) {
          return context.positions->get(
              position, /* line */ 20, /* start */ 21, /* end */ 22);
        }
        return position;
      },
      [&](const LocalPositionSet& local_positions) {
        LocalPositionSet new_local_positions = local_positions;
        new_local_positions.add(position1);
        return new_local_positions;
      });
  EXPECT_EQ(taint.local_positions(), LocalPositionSet{position1});

  // Verify that local positions were updated only in non-declaration frames.
  // Verify that call positions were updated only in non-leaf frames.
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("LeafFrame"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("NonLeafFrame1"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .call_position = context.positions->get(
                      position1, /* line */ 10, /* start */ 11, /* end */ 12),
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("NonLeafFrame2"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument)),
                  .callee = method_two,
                  .call_position = context.positions->get(
                      position2, /* line */ 20, /* start */ 21, /* end */ 22),
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("NonLeafFrame3"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = method_three,
                  .call_position = position3, // no change in position
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OriginFrame1"),
              test::FrameProperties{
                  .callee_port = *leaf_port,
                  .callee = nullptr,
                  // Only method1_origin's position is changed.
                  .call_position = context.positions->get(
                      position1, /* line */ 10, /* start */ 11, /* end */ 12),
                  .origins = OriginSet{method1_origin},
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::origin(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OriginFrame1"),
              test::FrameProperties{
                  .callee_port = *leaf_port,
                  .callee = nullptr,
                  // No change to method2_origin's position.
                  .call_position = position1,
                  .origins = OriginSet{method2_origin},
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::origin(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OriginFrame2"),
              test::FrameProperties{
                  .callee_port = *anchor_port,
                  .callee = nullptr,
                  // No change to position.
                  .call_position = position1,
                  .origins = OriginSet{crtex_origin},
                  .local_positions = LocalPositionSet{position1},
                  .call_kind = CallKind::origin(),
              }),
      }));
}

TEST_F(TaintTest, FilterInvalidFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kind_factory->get("TestSourceOne");
  auto* test_kind_two = context.kind_factory->get("TestSourceTwo");

  // Filter by callee. In practice, this scenario where the frames each contain
  // a different callee will not happen. These frames will be never show up in
  // the same `CalleePortFrames` object.
  //
  // TODO(T91357916): Move callee, call_position and callee_port out of `Frame`
  // and re-visit these tests. Signature of `filter_invalid_frames` will likely
  // change.
  auto taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE callee,
          const AccessPath& /* callee_port */,
          const Kind* /* kind */) { return callee == nullptr; });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})}));

  // Filter by callee port (drops nothing)
  taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port == AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          })}));
  // Filter by kind
  taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& /* callee_port */,
          const Kind* kind) { return kind != test_kind_two; });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})}));
}

TEST_F(TaintTest, ContainsKind) {
  auto context = test::make_empty_context();

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })};

  EXPECT_TRUE(taint.contains_kind(context.kind_factory->local_return()));
  EXPECT_TRUE(taint.contains_kind(context.kind_factory->get("TestSource")));
  EXPECT_FALSE(taint.contains_kind(context.kind_factory->get("TestSink")));
}

TEST_F(TaintTest, PartitionByKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = method_two2,
              .call_kind = CallKind::callsite(),
          })};

  auto taint_by_kind = taint.partition_by_kind();
  EXPECT_TRUE(taint_by_kind.size() == 3);
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource1")],
      Taint{test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource2")],
      Taint{test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource3")],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = method_one,
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = method_two2,
                  .call_kind = CallKind::callsite(),
              })}));
}

TEST_F(TaintTest, PartitionByKindGeneric) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = method_one,
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation_with_trace(CallKind::CallSite),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{
              .callee = method_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{
              .callee = method_two2,
              .call_kind = CallKind::callsite(),
          })};

  auto taint_by_kind = taint.partition_by_kind<bool>([&](const Kind* kind) {
    return kind->discard_transforms()->is<PropagationKind>();
  });
  EXPECT_TRUE(taint_by_kind.size() == 2);
  EXPECT_EQ(
      taint_by_kind[true],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .output_paths =
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  .call_kind = CallKind::propagation(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method_one,
                  .output_paths =
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  .call_kind =
                      CallKind::propagation_with_trace(CallKind::CallSite),
              }),
      }));
  EXPECT_EQ(
      taint_by_kind[false],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource1"),
              test::FrameProperties{
                  .callee = method_one,
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{
                  .callee = method_two2,
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, IntersectIntervals) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto initial_taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_one,
              .class_interval_context = CallClassIntervalContext(
                  ClassIntervals::Interval::finite(2, 6),
                  /* preserves_type_context*/ true),
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_two2,
              .class_interval_context = CallClassIntervalContext(
                  ClassIntervals::Interval::finite(4, 5),
                  /* preserves_type_context*/ false),
              .call_kind = CallKind::callsite(),
          })};

  {
    // Frames with preserves_type_context = false are always preserved even if
    // they intersect with nothing.
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint::bottom());
    EXPECT_EQ(
        taint,
        Taint{test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_two2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(4, 5),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            })});
  }

  {
    // Drop frames with preserves_type_context=true that don't intersect with
    // anything.
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method_two2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(10, 11),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_EQ(
        taint,
        Taint{test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_two2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(4, 5),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            })});
  }

  {
    // Taint is never "expanded" as a result of the intersection.
    auto taint = Taint::bottom();
    taint.intersect_intervals_with(initial_taint);
    EXPECT_EQ(taint, Taint::bottom());
  }

  {
    // Taint intersecting with itself gives itself
    auto taint = initial_taint;
    taint.intersect_intervals_with(initial_taint);
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // Produce bottom() if nothing intersects
    auto taint = Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method_two2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(4, 5),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })};
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method_two2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(6, 7),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_TRUE(taint.is_bottom());
  }

  {
    // If a frame is kept during intersection, all properties are kept the same
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method_two2, // method_one in initial_taint
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(
                    2, 3), // (2, 6) in initial_taint
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // Taint is preserved as long as it intersects with least one frame
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_one,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(2, 3),
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_one,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(10, 11),
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
    });
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // If other has at least one frame with preserves_type_context=false, all
    // frames are retained (because everything intersects with it).
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_two2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::top(),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            }),
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method_one,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(
                        10, 11), // intersects with nothing in initial_taint
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
    });
    EXPECT_EQ(taint, initial_taint);
  }
}

TEST_F(TaintTest, FeaturesJoined) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method_two2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* feature1 = context.feature_factory->get("Feature1");
  auto* feature2 = context.feature_factory->get("Feature2");
  auto* feature3 = context.feature_factory->get("Feature3");

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_one,
              .inferred_features = FeatureMayAlwaysSet{feature1},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method_two2,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature2},
                  /* always */ FeatureSet{feature3}),
              .locally_inferred_features = FeatureMayAlwaysSet{feature1},
              .call_kind = CallKind::callsite(),
          })};

  // In practice, features_joined() is called on `Taint` objects with only one
  // underlying kind. The expected behavior is to first merge locally inferred
  // features within each frame (this is an add() operation, not join()), then
  // perform a join() across all frames that have different callees/positions.

  EXPECT_EQ(
      taint.features_joined(),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{feature2, feature3},
          /* always */ FeatureSet{feature1}));
}

TEST_F(TaintTest, VisitFrames) {
  auto context = test::make_empty_context();

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{})};

  std::unordered_set<const Kind*> kinds;
  taint.visit_frames([&kinds](const CallInfo&, const Frame& frame) {
    kinds.insert(frame.kind());
  });

  EXPECT_EQ(kinds.size(), 2);
  EXPECT_NE(kinds.find(context.kind_factory->get("TestSource1")), kinds.end());
  EXPECT_NE(kinds.find(context.kind_factory->get("TestSource2")), kinds.end());
}

TEST_F(TaintTest, Transform) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method_one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind = context.kind_factory->get("TestSink");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  auto taint = Taint{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_one,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee = method_one,
              .call_position = test_position_two,
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.transform_frames([feature_one](Frame frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
    return frame;
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee = method_one,
                  .call_position = test_position_one,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee = method_one,
                  .call_position = test_position_two,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, AttachPosition) {
  auto context = test::make_empty_context();

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* test_position_three = context.positions->get(std::nullopt, 3);

  auto frames = Taint{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position_one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{feature_two}}),
      // Will be merged with the frame above after attach_position because they
      // have the same kind. Features will be joined too.
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position_two}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{.call_position = test_position_two}),
  };

  auto frames_with_new_position = frames.attach_position(test_position_three);

  EXPECT_EQ(
      frames_with_new_position,
      (Taint{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .call_position = test_position_three,
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
                  .call_kind = CallKind::origin(),
              }),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .call_position = test_position_three,
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_kind = CallKind::origin(),
              }),
      }));
}

} // namespace marianatrench
