/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/TaggedRootSet.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaggedRootSetTest : public test::Test {};

TEST_F(TaggedRootSetTest, SerializationDeserialization) {
  {
    auto tagged_root =
        TaggedRoot(Root(Root::Kind::Argument, 1), /* tag */ nullptr);
    EXPECT_EQ(TaggedRoot::from_json(tagged_root.to_json()), tagged_root);
  }

  {
    auto tagged_root = TaggedRoot(
        Root(Root::Kind::Argument, 1), DexString::make_string("tag"));
    EXPECT_EQ(TaggedRoot::from_json(tagged_root.to_json()), tagged_root);
  }
}

} // namespace marianatrench
