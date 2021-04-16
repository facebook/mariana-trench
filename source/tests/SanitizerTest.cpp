/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Sanitizer.h>
#include <mariana-trench/tests/Test.h>
#include <optional>

namespace marianatrench {

class SanitizerTest : public test::Test {};

TEST_F(SanitizerTest, Constructor) {
  // Test bottom
  EXPECT_TRUE(Sanitizer().is_bottom());
  EXPECT_TRUE(Sanitizer::bottom().is_bottom());

  // Providing sink kinds to a Sources sanitizer and vice-versa should throw
  EXPECT_THROW(
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::top()),
      std::exception);
  auto context = test::make_empty_context();
  const auto* kind = context.kinds->get("Kind");
  EXPECT_THROW(
      Sanitizer(
          SanitizerKind::Sinks,
          /* source_kinds */ KindSetAbstractDomain({kind}),
          /* sink_kinds */ KindSetAbstractDomain::top()),
      std::exception);
}

TEST_F(SanitizerTest, SanitizerLeq) {
  auto context = test::make_empty_context();

  // Comparison with bottom
  EXPECT_TRUE(Sanitizer::bottom().leq(Sanitizer::bottom()));
  EXPECT_TRUE(Sanitizer::bottom().leq(Sanitizer(
      SanitizerKind::Sources,
      /* source_kinds */ KindSetAbstractDomain::top(),
      /* sink_kinds */ KindSetAbstractDomain::bottom())));
  EXPECT_FALSE(Sanitizer(
                   SanitizerKind::Sources,
                   /* source_kinds */ KindSetAbstractDomain::top(),
                   /* sink_kinds */ KindSetAbstractDomain::bottom())
                   .leq(Sanitizer::bottom()));

  const auto* kind1 = context.kinds->get("Kind1");
  const auto* kind2 = context.kinds->get("Kind2");
  // Comparison within same kind
  EXPECT_TRUE(Sanitizer(
                  SanitizerKind::Sources,
                  /* source_kinds */
                  KindSetAbstractDomain({kind1}),
                  /* sink_kinds */ KindSetAbstractDomain::bottom())
                  .leq(Sanitizer(
                      SanitizerKind::Sources,
                      /* source_kinds */ KindSetAbstractDomain::top(),
                      /* sink_kinds */ KindSetAbstractDomain::bottom())));
  EXPECT_FALSE(Sanitizer(
                   SanitizerKind::Propagations,
                   /* source_kinds */ KindSetAbstractDomain({kind1, kind2}),
                   /* sink_kinds */ KindSetAbstractDomain::top())
                   .leq(Sanitizer(
                       SanitizerKind::Propagations,
                       /* source_kinds */ KindSetAbstractDomain({kind1}),
                       /* sink_kinds */ KindSetAbstractDomain::top())));
  EXPECT_FALSE(Sanitizer(
                   SanitizerKind::Sinks,
                   /* source_kinds */ KindSetAbstractDomain::bottom(),
                   /* sink_kinds */ KindSetAbstractDomain::top())
                   .leq(Sanitizer(
                       SanitizerKind::Sinks,
                       /* source_kinds */ KindSetAbstractDomain::bottom(),
                       /* sink_kinds */ KindSetAbstractDomain({kind1}))));

  // Comparison with different kinds
  EXPECT_FALSE(
      Sanitizer(
          SanitizerKind::Sinks,
          /* source_kinds */ KindSetAbstractDomain::bottom(),
          /* sink_kinds */ KindSetAbstractDomain({kind1, kind2}))
          .leq(Sanitizer(
              SanitizerKind::Propagations,
              /* source_kinds */ KindSetAbstractDomain::top(),
              /* sink_kinds */ KindSetAbstractDomain({kind1, kind2}))));
}

TEST_F(SanitizerTest, SanitizerEquals) {
  EXPECT_TRUE(Sanitizer::bottom().equals(Sanitizer::bottom()));
  EXPECT_FALSE(Sanitizer::bottom().equals(Sanitizer(
      SanitizerKind::Sources,
      /* source_kinds */ KindSetAbstractDomain::top(),
      /* sink_kinds */ KindSetAbstractDomain::bottom())));
}

TEST_F(SanitizerTest, SanitizerJoin) {
  // Join with bottom
  EXPECT_EQ(Sanitizer::bottom().join(Sanitizer::bottom()), Sanitizer::bottom());
  EXPECT_EQ(
      Sanitizer::bottom().join(Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom())),
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom()));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom())
          .join(Sanitizer::bottom()),
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom()));

  auto context = test::make_empty_context();
  const auto* kind1 = context.kinds->get("Kind1");
  const auto* kind2 = context.kinds->get("Kind2");
  const auto* kind3 = context.kinds->get("Kind3");
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain(kind1),
          /* sink_kinds */ KindSetAbstractDomain::bottom())
          .join(Sanitizer(
              SanitizerKind::Sources,
              /* source_kinds */ KindSetAbstractDomain::top(),
              /* sink_kinds */ KindSetAbstractDomain::bottom())),
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom()));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Propagations,
          /* source_kinds */ KindSetAbstractDomain({kind1, kind2}),
          /* sink_kinds */ KindSetAbstractDomain({kind1}))
          .join(Sanitizer(
              SanitizerKind::Propagations,
              /* source_kinds */ KindSetAbstractDomain({kind2, kind3}),
              /* sink_kinds */ KindSetAbstractDomain({kind1}))),
      Sanitizer(
          SanitizerKind::Propagations,
          /* source_kinds */ KindSetAbstractDomain({kind1, kind2, kind3}),
          /* sink_kinds */ KindSetAbstractDomain({kind1})));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sinks,
          /* source_kinds */ KindSetAbstractDomain::bottom(),
          /* sink_kinds */ KindSetAbstractDomain::top())
          .join(Sanitizer(
              SanitizerKind::Sinks,
              /* source_kinds */ KindSetAbstractDomain::bottom(),
              /* sink_kinds */ KindSetAbstractDomain({kind1}))),
      Sanitizer(
          SanitizerKind::Sinks,
          /* source_kinds */ KindSetAbstractDomain::bottom(),
          /* sink_kinds */ KindSetAbstractDomain::top()));

  // Incompatible Joins
  EXPECT_THROW(
      Sanitizer(
          SanitizerKind::Sources,
          /* source_kinds */ KindSetAbstractDomain::top(),
          /* sink_kinds */ KindSetAbstractDomain::bottom())
          .join(Sanitizer(
              SanitizerKind::Sinks,
              /* source_kinds */ KindSetAbstractDomain::bottom(),
              /* sink_kinds */ KindSetAbstractDomain::top())),
      std::exception);
}

} // namespace marianatrench
