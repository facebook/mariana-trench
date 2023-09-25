/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <re2/re2.h>

#include <mariana-trench/AnnotationFeatureSet.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

class AnnotationFeatureSetTest : public test::Test {};

testing::AssertionResult has_features(const std::string& output, const std::string_view first, const std::string_view second) {
  const re2::RE2 pattern{"\\{`.*`, `.*`\\}"};
  if (!re2::RE2::FullMatch(output, pattern)) {
    return testing::AssertionFailure() << output << " is ill-formatted";
  }

  if (std::string::npos == output.find(first)) {
    return testing::AssertionFailure() << output << " is missing: " << first;
  }
  if (std::string::npos == output.find(second)) {
    return testing::AssertionFailure() << output << " is missing: " << second;
  }
  return testing::AssertionSuccess();
}

} // namespace

TEST_F(AnnotationFeatureSetTest, show) {
    AnnotationFeatureSet annotation_features;
    const DexType* method_annotation = DexType::make_type("Lfoo/MethodAnnotation;");
    const DexType* param_annotation = DexType::make_type("Lbar/ParamAnnotation;");
    AnnotationFeature first{Root{Root::Kind::Return}, method_annotation, "MyLabel"};
    AnnotationFeature second{Root{Root::Kind::Argument, 1}, param_annotation, std::nullopt};
    annotation_features.add(&first);
    annotation_features.add(&second);

    std::ostringstream oss;
    oss << annotation_features;
    EXPECT_TRUE(has_features(
      oss.str(),
      "`AnnotationFeature(port=`Return`, dex_type=`Lfoo/MethodAnnotation;`, label=`MyLabel`)`",
      "`AnnotationFeature(port=`Argument(1)`, dex_type=`Lbar/ParamAnnotation;`)`"));
}

} // namespace marianatrench
