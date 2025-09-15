/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Sanitizer.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class SanitizerTest : public test::Test {};

TEST_F(SanitizerTest, Constructor) {
  // Test bottom
  EXPECT_TRUE(Sanitizer().is_bottom());
  EXPECT_TRUE(Sanitizer::bottom().is_bottom());
}

TEST_F(SanitizerTest, SanitizerLeq) {
  auto context = test::make_empty_context();

  // Comparison with bottom
  EXPECT_TRUE(Sanitizer::bottom().leq(Sanitizer::bottom()));
  EXPECT_TRUE(Sanitizer::bottom().leq(Sanitizer(
      SanitizerKind::Sources,
      /* kinds */ KindSetAbstractDomain::top())));
  EXPECT_FALSE(Sanitizer(
                   SanitizerKind::Sources,
                   /* kinds */ KindSetAbstractDomain::top())
                   .leq(Sanitizer::bottom()));

  const auto* kind1 = context.kind_factory->get("Kind1");
  const auto* kind2 = context.kind_factory->get("Kind2");
  // Comparison within same kind
  EXPECT_TRUE(Sanitizer(
                  SanitizerKind::Sources,
                  /* kinds */
                  KindSetAbstractDomain(SourceSinkKind::source(kind1)))
                  .leq(Sanitizer(
                      SanitizerKind::Sources,
                      /* kinds */ KindSetAbstractDomain::top())));
  EXPECT_FALSE(
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */
          KindSetAbstractDomain(
              {SourceSinkKind::source(kind1), SourceSinkKind::source(kind2)}))
          .leq(Sanitizer(
              SanitizerKind::Sources,
              /* kinds */
              KindSetAbstractDomain(SourceSinkKind::source(kind1)))));
  EXPECT_TRUE(Sanitizer(
                  SanitizerKind::Sinks,
                  /* kinds */ KindSetAbstractDomain::top())
                  .leq(Sanitizer(
                      SanitizerKind::Sinks,
                      /* kinds */ KindSetAbstractDomain::top())));

  // Comparison with different kinds
  EXPECT_FALSE(
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */
          KindSetAbstractDomain(
              {SourceSinkKind::sink(kind1), SourceSinkKind::sink(kind2)}))
          .leq(Sanitizer(
              SanitizerKind::Sources,
              /* kinds */
              KindSetAbstractDomain(
                  {SourceSinkKind::source(kind1),
                   SourceSinkKind::source(kind2)}))));
}

TEST_F(SanitizerTest, SanitizerEquals) {
  EXPECT_TRUE(Sanitizer::bottom().equals(Sanitizer::bottom()));
  EXPECT_FALSE(Sanitizer::bottom().equals(Sanitizer(
      SanitizerKind::Sources,
      /* kinds */ KindSetAbstractDomain::top())));

  // Test that all possible bottom sanitizers are equal
  auto sources_bottom =
      Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain::bottom());
  auto sinks_bottom =
      Sanitizer(SanitizerKind::Sinks, KindSetAbstractDomain::bottom());
  auto propagations_bottom =
      Sanitizer(SanitizerKind::Propagations, KindSetAbstractDomain::bottom());
  EXPECT_TRUE(sources_bottom.equals(sinks_bottom));
  EXPECT_TRUE(propagations_bottom.equals(sinks_bottom));
  EXPECT_TRUE(sources_bottom.equals(propagations_bottom));
  EXPECT_TRUE(Sanitizer::bottom().equals(propagations_bottom));
}

TEST_F(SanitizerTest, SanitizerJoin) {
  // Join with bottom
  EXPECT_EQ(Sanitizer::bottom().join(Sanitizer::bottom()), Sanitizer::bottom());
  EXPECT_EQ(
      Sanitizer::bottom().join(Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top())),
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top()));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top())
          .join(Sanitizer::bottom()),
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top()));

  auto context = test::make_empty_context();
  const auto* kind1 = context.kind_factory->get("Kind1");
  const auto* kind2 = context.kind_factory->get("Kind2");
  const auto* kind3 = context.kind_factory->get("Kind3");
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain(SourceSinkKind::source(kind1)))
          .join(Sanitizer(
              SanitizerKind::Sources,
              /* kinds */ KindSetAbstractDomain::top())),
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top()));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */
          KindSetAbstractDomain(
              {SourceSinkKind::sink(kind1), SourceSinkKind::sink(kind2)}))
          .join(Sanitizer(
              SanitizerKind::Sinks,
              /* kinds */
              KindSetAbstractDomain(
                  {SourceSinkKind::sink(kind2), SourceSinkKind::sink(kind3)}))),
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */
          KindSetAbstractDomain(
              {SourceSinkKind::sink(kind1),
               SourceSinkKind::sink(kind2),
               SourceSinkKind::sink(kind3)})));
  EXPECT_EQ(
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */ KindSetAbstractDomain::top())
          .join(Sanitizer(
              SanitizerKind::Sinks,
              /* kinds */ KindSetAbstractDomain(SourceSinkKind::sink(kind1)))),
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */ KindSetAbstractDomain::top()));

  // Incompatible Joins
  EXPECT_THROW(
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top())
          .join(Sanitizer(
              SanitizerKind::Propagations,
              /* kinds */ KindSetAbstractDomain::top())),
      std::exception);
}

} // namespace marianatrench
