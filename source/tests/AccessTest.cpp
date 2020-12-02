// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class AccessTest : public test::Test {};

TEST_F(AccessTest, PathExtend) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  auto path = Path{};
  path.extend(Path{});
  EXPECT_EQ(path, Path{});

  path.extend(Path{x, y});
  EXPECT_EQ(path, (Path{x, y}));

  path.extend(Path{});
  EXPECT_EQ(path, (Path{x, y}));

  path.extend(Path{z, x});
  EXPECT_EQ(path, (Path{x, y, z, x}));
}

TEST_F(AccessTest, PathTruncate) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  auto path = Path{};
  path.truncate(2);
  EXPECT_EQ(path, Path{});

  path = Path{x};
  path.truncate(2);
  EXPECT_EQ(path, Path{x});

  path = Path{x, y};
  path.truncate(2);
  EXPECT_EQ(path, (Path{x, y}));

  path = Path{x, y, z};
  path.truncate(2);
  EXPECT_EQ(path, (Path{x, y}));

  path = Path{x, y, z, x};
  path.truncate(2);
  EXPECT_EQ(path, (Path{x, y}));
}

TEST_F(AccessTest, PathIsPrefixOf) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  EXPECT_TRUE(Path{}.is_prefix_of(Path{}));
  EXPECT_TRUE(Path{}.is_prefix_of(Path{x}));
  EXPECT_TRUE(Path{}.is_prefix_of(Path{x, y}));

  EXPECT_FALSE(Path{x}.is_prefix_of(Path{}));
  EXPECT_TRUE(Path{x}.is_prefix_of(Path{x}));
  EXPECT_TRUE(Path{x}.is_prefix_of(Path{x, y}));
  EXPECT_TRUE(Path{x}.is_prefix_of(Path{x, y, z}));
  EXPECT_FALSE(Path{x}.is_prefix_of(Path{y}));

  EXPECT_FALSE((Path{x, y}).is_prefix_of(Path{}));
  EXPECT_FALSE((Path{x, y}).is_prefix_of(Path{x}));
  EXPECT_TRUE((Path{x, y}).is_prefix_of(Path{x, y}));
  EXPECT_TRUE((Path{x, y}).is_prefix_of(Path{x, y, z}));
  EXPECT_TRUE((Path{x, y}).is_prefix_of(Path{x, y, x}));
  EXPECT_FALSE((Path{x, y}).is_prefix_of(Path{y}));
}

TEST_F(AccessTest, PathCommonPrefix) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  auto path = Path{};
  path.reduce_to_common_prefix(Path{});
  EXPECT_EQ(path, Path{});
  path.reduce_to_common_prefix(Path{x});
  EXPECT_EQ(path, Path{});
  path.reduce_to_common_prefix(Path{x, y});
  EXPECT_EQ(path, Path{});

  path = Path{x};
  path.reduce_to_common_prefix(Path{});
  EXPECT_EQ(path, Path{});

  path = Path{x};
  path.reduce_to_common_prefix(Path{x});
  EXPECT_EQ(path, Path{x});

  path = Path{x};
  path.reduce_to_common_prefix(Path{x, y});
  EXPECT_EQ(path, Path{x});

  path = Path{x};
  path.reduce_to_common_prefix(Path{x, y, z});
  EXPECT_EQ(path, Path{x});

  path = Path{x};
  path.reduce_to_common_prefix(Path{y});
  EXPECT_EQ(path, Path{});

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{});
  EXPECT_EQ(path, Path{});

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{x});
  EXPECT_EQ(path, Path{x});

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{x, y});
  EXPECT_EQ(path, (Path{x, y}));

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{x, y, z});
  EXPECT_EQ(path, (Path{x, y}));

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{x, y, x});
  EXPECT_EQ(path, (Path{x, y}));

  path = Path{x, y};
  path.reduce_to_common_prefix(Path{y});
  EXPECT_EQ(path, (Path{}));
}

TEST_F(AccessTest, AccessPathLessOrEqual) {
  auto root = Root(Root::Kind::Return);
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  EXPECT_TRUE(AccessPath(root).leq(AccessPath(root)));
  EXPECT_FALSE(AccessPath(root).leq(AccessPath(root, Path{x})));
  EXPECT_FALSE(AccessPath(root).leq(AccessPath(root, Path{x, y})));

  EXPECT_TRUE(AccessPath(root, Path{x}).leq(AccessPath(root)));
  EXPECT_TRUE(AccessPath(root, Path{x}).leq(AccessPath(root, Path{x})));
  EXPECT_FALSE(AccessPath(root, Path{x}).leq(AccessPath(root, Path{x, y})));
  EXPECT_FALSE(AccessPath(root, Path{x}).leq(AccessPath(root, Path{y})));

  EXPECT_TRUE(AccessPath(root, Path{x, y}).leq(AccessPath(root)));
  EXPECT_TRUE(AccessPath(root, Path{x, y}).leq(AccessPath(root, Path{x})));
  EXPECT_TRUE(AccessPath(root, Path{x, y}).leq(AccessPath(root, Path{x, y})));
  EXPECT_FALSE(
      AccessPath(root, Path{x, y}).leq(AccessPath(root, Path{x, y, z})));
  EXPECT_FALSE(AccessPath(root, Path{x, y}).leq(AccessPath(root, Path{y})));
}

TEST_F(AccessTest, AccessPathJoin) {
  auto root = Root(Root::Kind::Return);
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  auto access_path = AccessPath(root);
  access_path.join_with(AccessPath(root));
  EXPECT_EQ(access_path, AccessPath(root));
  access_path.join_with(AccessPath(root, Path{x}));
  EXPECT_EQ(access_path, AccessPath(root));
  access_path.join_with(AccessPath(root, Path{x, y}));
  EXPECT_EQ(access_path, AccessPath(root));

  access_path = AccessPath(root, Path{x});
  access_path.join_with(AccessPath(root));
  EXPECT_EQ(access_path, AccessPath(root));

  access_path = AccessPath(root, Path{x});
  access_path.join_with(AccessPath(root, Path{x}));
  EXPECT_EQ(access_path, AccessPath(root, Path{x}));

  access_path = AccessPath(root, Path{x});
  access_path.join_with(AccessPath(root, Path{x, y}));
  EXPECT_EQ(access_path, AccessPath(root, Path{x}));

  access_path = AccessPath(root, Path{x});
  access_path.join_with(AccessPath(root, Path{z}));
  EXPECT_EQ(access_path, AccessPath(root));

  access_path = AccessPath(root, Path{x, y});
  access_path.join_with(AccessPath(root));
  EXPECT_EQ(access_path, AccessPath(root));

  access_path = AccessPath(root, Path{x, y});
  access_path.join_with(AccessPath(root, Path{x}));
  EXPECT_EQ(access_path, AccessPath(root, Path{x}));

  access_path = AccessPath(root, Path{x, y});
  access_path.join_with(AccessPath(root, Path{x, z}));
  EXPECT_EQ(access_path, AccessPath(root, Path{x}));

  access_path = AccessPath(root, Path{x, y});
  access_path.join_with(AccessPath(root, Path{x, y, z}));
  EXPECT_EQ(access_path, AccessPath(root, Path{x, y}));
}

} // namespace marianatrench
