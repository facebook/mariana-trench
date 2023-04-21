/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/TaintTree.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaintTreeTest : public test::Test {};

namespace {

Taint make_propagation(
    Context& context,
    ParameterPosition parameter_position,
    Path output_path = {},
    CollapseDepth collapse_depth = CollapseDepth(4)) {
  return Taint::propagation_taint(
      context.kind_factory->local_argument(parameter_position),
      /* output_paths */
      PathTreeDomain{{output_path, collapse_depth}},
      /* inferred_features */ {},
      /* user_features */ {});
}

struct PropagateBackwardTaint {
  Taint operator()(Taint taint, Path::Element path_element) const {
    taint.append_to_propagation_output_paths(path_element);
    return taint;
  }
};

} // namespace

TEST_F(TaintTreeTest, PropagateOnRead) {
  auto context = test::make_empty_context();
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintTree{make_propagation(context, 1, Path{}, CollapseDepth(4))};
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateBackwardTaint()),
      (TaintTree{
          make_propagation(context, 1, Path{x, y}, CollapseDepth(2)),
      }));

  tree.write(
      Path{x},
      make_propagation(context, 2, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateBackwardTaint()),
      (TaintTree{
          make_propagation(context, 1, Path{x, y}, CollapseDepth(2))
              .join(make_propagation(context, 2, Path{y}, CollapseDepth(3))),
      }));

  tree.write(
      Path{x, y},
      make_propagation(context, 3, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateBackwardTaint()),
      (TaintTree{
          make_propagation(context, 1, Path{x, y}, CollapseDepth(2))
              .join(make_propagation(context, 2, Path{y}, CollapseDepth(3)))
              .join(make_propagation(context, 3, Path{}, CollapseDepth(4))),
      }));

  tree.write(
      Path{x, y, z},
      make_propagation(context, 4, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateBackwardTaint()),
      (TaintTree{
          {
              Path{},
              make_propagation(context, 1, Path{x, y}, CollapseDepth(2)),
          },
          {
              Path{},
              make_propagation(context, 2, Path{y}, CollapseDepth(3)),
          },
          {
              Path{},
              make_propagation(context, 3, Path{}, CollapseDepth(4)),
          },
          {
              Path{z},
              make_propagation(context, 4, Path{}, CollapseDepth(4)),
          },
      }));

  tree = TaintTree{
      make_propagation(context, 0, Path{x}, CollapseDepth(4)),
  };
  EXPECT_EQ(
      tree.read(Path{y}, PropagateBackwardTaint()),
      (TaintTree{
          make_propagation(context, 0, Path{x, y}, CollapseDepth(3)),
      }));

  tree.set_to_bottom();
  tree.write(
      Path{x},
      make_propagation(context, 0, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  tree.write(
      Path{y},
      make_propagation(context, 1, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  tree.write(
      Path{z},
      make_propagation(context, 2, Path{}, CollapseDepth(4)),
      UpdateKind::Weak);
  tree.write(
      Path{y, z},
      make_propagation(context, 1, Path{z}, CollapseDepth(4)),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{y, z}, PropagateBackwardTaint()),
      (TaintTree{
          make_propagation(context, 1, Path{z}, CollapseDepth(3)),
      }));
}

TEST_F(TaintTreeTest, Collapse) {
  auto context = test::make_empty_context();
  const auto kind = context.kind_factory->get("Test");
  const Feature broadening = Feature("via-broadening");
  const FeatureMayAlwaysSet features = FeatureMayAlwaysSet({&broadening});

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = TaintTree{Taint{test::make_taint_config(
      kind,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})}};
  EXPECT_EQ(
      tree.collapse(features),
      Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})});

  tree.write(
      Path{x},
      Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 2))})},
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(features),
      (Taint{
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              }),
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .locally_inferred_features = features,
              }),
      }));

  tree.write(
      Path{x, y},
      (Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
          })}),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(features),
      (Taint{
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              }),
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .locally_inferred_features = features,
              }),
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                  .locally_inferred_features = features,
              }),
      }));

  tree.write(
      Path{},
      (Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
          })}),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(features),
      (Taint{
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              }),
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .locally_inferred_features = features,
              }),
          test::make_taint_config(
              kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
              }),
      }));

  // Update collapse depth when collapsing backward taint.
  tree = TaintTree{
      {Path{x},
       Taint{test::make_propagation_taint_config(
           context.kind_factory->local_return(),
           PathTreeDomain{
               {Path{x}, CollapseDepth(4)},
           },
           /* inferred_features */ {},
           /* locally_inferred_features */ {},
           /* user features */ {})}},
      {Path{y},
       Taint{test::make_propagation_taint_config(
           context.kind_factory->local_return(),
           PathTreeDomain{
               {Path{y}, CollapseDepth(3)},
           },
           /* inferred_features */ {},
           /* locally_inferred_features */ {},
           /* user features */ {})}},
  };
  EXPECT_EQ(
      tree.collapse(features),
      Taint{Taint{test::make_propagation_taint_config(
          context.kind_factory->local_return(),
          PathTreeDomain{
              {Path{x}, CollapseDepth(0)},
              {Path{y}, CollapseDepth(0)},
          },
          /* inferred_features */ {},
          /* locally_inferred_features */ features,
          /* user_features */ {})}});

  tree = TaintTree{
      {Path{x, x},
       Taint{test::make_propagation_taint_config(
           context.kind_factory->local_return(),
           PathTreeDomain{
               {Path{x}, CollapseDepth(4)},
           },
           /* inferred_features */ {},
           /* locally_inferred_features */ {},
           /* user features */ {})}},
      {Path{x, y},
       Taint{test::make_propagation_taint_config(
           context.kind_factory->local_return(),
           PathTreeDomain{
               {Path{y}, CollapseDepth(3)},
           },
           /* inferred_features */ {},
           /* locally_inferred_features */ {},
           /* user features */ {})}},
  };
  tree.collapse_deeper_than(1, features);
  EXPECT_EQ(
      tree,
      (TaintTree{
          {Path{x},
           Taint{test::make_propagation_taint_config(
               context.kind_factory->local_return(),
               PathTreeDomain{
                   {Path{x}, CollapseDepth(0)},
                   {Path{y}, CollapseDepth(0)},
               },
               /* inferred_features */ {},
               /* locally_inferred_features */ features,
               /* user_features */ {})}},
      }));
}

TEST_F(TaintTreeTest, LimitLeaves) {
  auto context = test::make_empty_context();
  const auto kind = context.kind_factory->get("Test");

  const Feature broadening = Feature("via-broadening");
  const FeatureMayAlwaysSet features = FeatureMayAlwaysSet({&broadening});

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree3 = TaintTree{Taint{test::make_taint_config(
      kind,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
      })}};
  tree3.write(
      Path{x},
      TaintTree{Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
          })}},
      UpdateKind::Weak);
  tree3.write(
      Path{y},
      TaintTree{Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
          })}},
      UpdateKind::Weak);
  tree3.write(
      Path{z},
      TaintTree{Taint{test::make_taint_config(
          kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 4)),
          })}},
      UpdateKind::Weak);
  tree3.limit_leaves(2, features);
  EXPECT_EQ(
      tree3,
      (TaintTree{
          {
              Path{},
              Taint{test::make_taint_config(
                  kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  })},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                      .locally_inferred_features = features,
                  })},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                      .locally_inferred_features = features,
                  })},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 4)),
                      .locally_inferred_features = features,
                  })},
          }}));
}

} // namespace marianatrench
