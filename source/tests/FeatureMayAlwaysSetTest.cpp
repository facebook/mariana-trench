/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FeatureMayAlwaysSetTest : public test::Test {};

TEST_F(FeatureMayAlwaysSetTest, Constructor) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");

  EXPECT_TRUE(FeatureMayAlwaysSet::bottom().is_bottom());
  EXPECT_TRUE(FeatureMayAlwaysSet::top().is_top());
  EXPECT_TRUE(FeatureMayAlwaysSet().empty());

  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{two})
          .may(),
      (FeatureSet{one}));
  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{two})
          .always(),
      FeatureSet{two});

  EXPECT_EQ(FeatureMayAlwaysSet::make_may({one}).may(), FeatureSet{one});
  EXPECT_EQ(FeatureMayAlwaysSet::make_may({one}).always(), FeatureSet{});

  EXPECT_EQ(
      FeatureMayAlwaysSet::make_may(FeatureSet{one}).may(), FeatureSet{one});
  EXPECT_EQ(
      FeatureMayAlwaysSet::make_may(FeatureSet{one}).always(), FeatureSet{});

  EXPECT_EQ(FeatureMayAlwaysSet::make_always({one}).may(), FeatureSet{});
  EXPECT_EQ(FeatureMayAlwaysSet::make_always({one}).always(), FeatureSet{one});

  EXPECT_EQ(
      FeatureMayAlwaysSet::make_always(FeatureSet{one}).may(), FeatureSet{});
  EXPECT_EQ(
      FeatureMayAlwaysSet::make_always(FeatureSet{one}).always(),
      FeatureSet{one});

  EXPECT_EQ(FeatureMayAlwaysSet{one}.may(), FeatureSet{});
  EXPECT_EQ(FeatureMayAlwaysSet{one}.always(), FeatureSet{one});
}

TEST_F(FeatureMayAlwaysSetTest, Leq) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");

  EXPECT_FALSE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{one}, /* always */ FeatureSet{})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{}, /* always */ FeatureSet{})));
  EXPECT_TRUE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{one}, /* always */ FeatureSet{})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{one}, /* always */ FeatureSet{})));
  EXPECT_FALSE(FeatureMayAlwaysSet(
                   /* may */ FeatureSet{one, two}, /* always */ FeatureSet{})
                   .leq(FeatureMayAlwaysSet(
                       /* may */ FeatureSet{one}, /* always */ FeatureSet{})));
  EXPECT_TRUE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{one}, /* always */ FeatureSet{})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{one, two}, /* always */ FeatureSet{})));

  EXPECT_FALSE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{}, /* always */ FeatureSet{one})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{}, /* always */ FeatureSet{})));
  EXPECT_TRUE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{}, /* always */ FeatureSet{one})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{}, /* always */ FeatureSet{one})));
  EXPECT_FALSE(
      FeatureMayAlwaysSet(/* may */ FeatureSet{one}, /* always */ FeatureSet{})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{}, /* always */ FeatureSet{one})));
  EXPECT_TRUE(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{}, /* always */ FeatureSet{one, two})
          .leq(FeatureMayAlwaysSet(
              /* may */ FeatureSet{one}, /* always */ FeatureSet{two})));
}

TEST_F(FeatureMayAlwaysSetTest, Equals) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");

  EXPECT_TRUE(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one}, /* always */ FeatureSet{two})
          .equals(FeatureMayAlwaysSet(
              /* may */ FeatureSet{one}, /* always */ FeatureSet{two})));
  EXPECT_FALSE(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one}, /* always */ FeatureSet{two})
          .equals(FeatureMayAlwaysSet(
              /* may */ FeatureSet{two}, /* always */ FeatureSet{one})));
}

TEST_F(FeatureMayAlwaysSetTest, Join) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");
  const auto* three = context.feature_factory->get("FeatureThree");

  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one}, /* always */ FeatureSet{two})
          .join(FeatureMayAlwaysSet(
              /* may */ FeatureSet{two}, /* always */ FeatureSet{one})),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{}));
  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{}, /* always */ FeatureSet{one, two})
          .join(FeatureMayAlwaysSet(
              /* may */ FeatureSet{}, /* always */ FeatureSet{one, three})),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two, three}, /* always */ FeatureSet{one}));
}

TEST_F(FeatureMayAlwaysSetTest, Meet) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");
  const auto* three = context.feature_factory->get("FeatureThree");

  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{two})
          .meet(FeatureMayAlwaysSet(
              /* may */ FeatureSet{two, three},
              /* always */ FeatureSet{two})),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{two}, /* always */ FeatureSet{two}));
  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one}, /* always */ FeatureSet{two})
          .meet(FeatureMayAlwaysSet(
              /* may */ FeatureSet{two}, /* always */ FeatureSet{one})),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{one, two}));
  EXPECT_EQ(
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two}, /* always */ FeatureSet{one})
          .meet(FeatureMayAlwaysSet(
              /* may */ FeatureSet{two, three}, /* always */ FeatureSet{})),
      FeatureMayAlwaysSet::bottom());
}

TEST_F(FeatureMayAlwaysSetTest, Add) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");
  const auto* three = context.feature_factory->get("FeatureThree");

  auto set = FeatureMayAlwaysSet(
      /* may */ FeatureSet{one}, /* always */ FeatureSet{two});
  set.add(FeatureMayAlwaysSet(
      /* may */ FeatureSet{three},
      /* always */ FeatureSet{}));
  EXPECT_EQ(
      set,
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two, three}, /* always */ FeatureSet{two}));

  set = FeatureMayAlwaysSet(
      /* may */ FeatureSet{one}, /* always */ FeatureSet{two});
  set.add(FeatureMayAlwaysSet(
      /* may */ FeatureSet{},
      /* always */ FeatureSet{three}));
  EXPECT_EQ(
      set,
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two, three},
          /* always */ FeatureSet{two, three}));
}

TEST_F(FeatureMayAlwaysSetTest, AddMay) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");
  const auto* three = context.feature_factory->get("FeatureThree");

  auto set = FeatureMayAlwaysSet(
      /* may */ FeatureSet{one}, /* always */ FeatureSet{two});
  set.add_may(three);
  EXPECT_EQ(
      set,
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two, three}, /* always */ FeatureSet{two}));
}

TEST_F(FeatureMayAlwaysSetTest, AddAlways) {
  auto context = test::make_empty_context();
  const auto* one = context.feature_factory->get("FeatureOne");
  const auto* two = context.feature_factory->get("FeatureTwo");
  const auto* three = context.feature_factory->get("FeatureThree");

  auto set = FeatureMayAlwaysSet(
      /* may */ FeatureSet{one}, /* always */ FeatureSet{two});
  set.add_always(three);
  EXPECT_EQ(
      set,
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{one, two, three},
          /* always */ FeatureSet{two, three}));
}

} // namespace marianatrench
