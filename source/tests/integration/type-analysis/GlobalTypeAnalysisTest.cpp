/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexClass.h>

#include <mariana-trench/tests/integration/type-analysis/TypeAnalysisTestBase.h>
#include <mariana-trench/type-analysis/GlobalTypeAnalyzer.h>

namespace marianatrench {

using namespace type_analyzer;
using namespace type_analyzer::global;

class GlobalTypeAnalysisTest : public TypeAnalysisTestBase {};

TEST_F(GlobalTypeAnalysisTest, ReturnTypeTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestA;.foo:()I");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto meth_get_subone = get_method("TestA;.getSubOne", "Base");
  EXPECT_EQ(wps.get_return_type(meth_get_subone), get_type_domain("SubOne"));
  auto meth_get_subtwo = get_method("TestA;.getSubTwo", "Base");
  EXPECT_EQ(wps.get_return_type(meth_get_subtwo), get_type_domain("SubTwo"));
  auto meth_passthrough = get_method("TestA;.passThrough",
                                     "Lcom/facebook/redextest/Base;",
                                     "Lcom/facebook/redextest/Base;");
  EXPECT_EQ(wps.get_return_type(meth_passthrough), get_type_domain("SubTwo"));

  auto meth_foo = get_method("TestA;.foo:()I");
  auto lta = gta->get_replayable_local_analysis(meth_foo);
  auto code = meth_foo->get_code();
  auto foo_exit_env = lta->get_exit_state_at(code->cfg().exit_block());
  EXPECT_EQ(foo_exit_env.get_reg_environment().get(0),
            get_type_domain("SubOne"));
  EXPECT_EQ(foo_exit_env.get_reg_environment().get(2),
            get_type_domain("SubTwo"));
}

TEST_F(GlobalTypeAnalysisTest, ConstsAndAGETTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestB;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto meth_pass_null =
      get_method("TestB;.passNull", "Ljava/lang/String;", "Ljava/lang/String;");
  EXPECT_TRUE(wps.get_return_type(meth_pass_null).is_null());

  auto meth_pass_string = get_method(
      "TestB;.passString", "Ljava/lang/String;", "Ljava/lang/String;");
  EXPECT_EQ(
      wps.get_return_type(meth_pass_string),
      get_type_domain_simple("Ljava/lang/String;", /* is_not_null */ true));

  auto meth_pass_class =
      get_method("TestB;.passClass", "Ljava/lang/Class;", "Ljava/lang/Class;");
  EXPECT_EQ(
      wps.get_return_type(meth_pass_class),
      get_type_domain_simple("Ljava/lang/Class;", /* is_not_null */ true));

  auto meth_array_comp = get_method("TestB;.getStringArrayComponent",
                                    "[Ljava/lang/String;",
                                    "Ljava/lang/String;");
  EXPECT_EQ(wps.get_return_type(meth_array_comp),
            get_type_domain_simple("Ljava/lang/String;"));

  auto meth_nested_array_comp =
      get_method("TestB;.getNestedStringArrayComponent",
                 "[[Ljava/lang/String;",
                 "[Ljava/lang/String;");
  EXPECT_EQ(wps.get_return_type(meth_nested_array_comp),
            get_type_domain_simple("[Ljava/lang/String;"));
}

TEST_F(GlobalTypeAnalysisTest, NullableFieldTypeTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestC;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  // Field holding the reference to the nullalbe anonymous class
  auto field_monitor =
      get_field("TestC;.mMonitor:Lcom/facebook/redextest/Receiver;");
  EXPECT_EQ(*wps.get_field_type(field_monitor).get_dex_type(),
            get_type("TestC$1"));
  EXPECT_TRUE(wps.get_field_type(field_monitor).is_nullable());

  // Field on the anonymous class referencing the outer class
  auto field_anony =
      get_field("TestC$1;.this$0:Lcom/facebook/redextest/TestC;");
  EXPECT_EQ(wps.get_field_type(field_anony),
            get_type_domain("TestC").join(DexTypeDomain::null()));
}

TEST_F(GlobalTypeAnalysisTest, TrueVirtualFieldTypeTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestD;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  // True virtual methods are entry-points. Redex conservatively sets fields
  // written to by such methods as top, but Mariana Trench does not.
  auto field_val =
      get_field("TestD$State;.mVal:Lcom/facebook/redextest/TestD$Base;");
  auto field_type = wps.get_field_type(field_val);
  EXPECT_FALSE(field_type.is_top());
  EXPECT_TRUE(field_type.is_nullable());
  const auto& single_domain = field_type.get_single_domain();
  EXPECT_EQ(single_domain, SingletonDexTypeDomain(get_type("TestD$Sub")));
  const auto& set_domain = field_type.get_set_domain();
  EXPECT_EQ(set_domain.get_types(), get_type_set({get_type("TestD$Sub")}));
}

TEST_F(GlobalTypeAnalysisTest, SmallSetDexTypeDomainTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestE;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto meth_ret_subs = get_method(
      "TestE;.returnSubTypes", "I", "Lcom/facebook/redextest/TestE$Base;");
  auto rtype = wps.get_return_type(meth_ret_subs);
  EXPECT_TRUE(rtype.is_nullable());
  const auto& single_domain = rtype.get_single_domain();
  EXPECT_EQ(single_domain, SingletonDexTypeDomain(get_type("TestE$Base")));
  const auto& set_domain = rtype.get_set_domain();
  EXPECT_EQ(set_domain.get_types(),
            get_type_set({get_type("TestE$SubOne"), get_type("TestE$SubTwo"),
                          get_type("TestE$SubThree")}));
}

TEST_F(GlobalTypeAnalysisTest, ConstNullnessDomainTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestF;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();
  auto meth_foo = get_method("TestF;.foo", "", "I");
  auto lta = gta->get_replayable_local_analysis(meth_foo);
  auto code = meth_foo->get_code();
  auto foo_exit_env = lta->get_exit_state_at(code->cfg().exit_block());
  EXPECT_TRUE(foo_exit_env.get_reg_environment().get(0).is_top());
}

TEST_F(GlobalTypeAnalysisTest, ArrayConstNullnessDomainTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestG;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();
  auto meth_foo =
      get_method("TestG;.foo", "", "Lcom/facebook/redextest/TestG$Base;");
  auto rtype = wps.get_return_type(meth_foo);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_nullable());

  auto meth_bar =
      get_method("TestG;.bar", "", "Lcom/facebook/redextest/TestG$Base;");
  rtype = wps.get_return_type(meth_bar);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_nullable());
}

TEST_F(GlobalTypeAnalysisTest, ClinitFieldAnalyzerTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestH;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto field_sbase =
      get_field("TestH;.BASE:Lcom/facebook/redextest/TestH$Base;");
  auto ftype = wps.get_field_type(field_sbase);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestH$Base")));
  EXPECT_EQ(ftype.get_set_domain(), get_small_set_domain({"TestH$Base"}));

  auto field_mbase =
      get_field("TestH;.mBase:Lcom/facebook/redextest/TestH$Base;");
  ftype = wps.get_field_type(field_mbase);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestH$Base")));
  EXPECT_EQ(ftype.get_set_domain(),
            get_small_set_domain({"TestH$SubOne", "TestH$SubTwo"}));

  auto meth_foo =
      get_method("TestH;.foo", "", "Lcom/facebook/redextest/TestH$Base;");
  auto rtype = wps.get_return_type(meth_foo);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestH$Base")));
  EXPECT_EQ(rtype.get_set_domain(),
            get_small_set_domain({"TestH$SubOne", "TestH$SubTwo"}));

  auto meth_bar =
      get_method("TestH;.bar", "", "Lcom/facebook/redextest/TestH$Base;");
  rtype = wps.get_return_type(meth_bar);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestH$Base")));
  EXPECT_EQ(rtype.get_set_domain(),
            get_small_set_domain({"TestH$SubOne", "TestH$SubTwo"}));

  auto meth_baz =
      get_method("TestH;.baz", "", "Lcom/facebook/redextest/TestH$Base;");
  rtype = wps.get_return_type(meth_baz);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestH$Base")));
  EXPECT_EQ(rtype.get_set_domain(), get_small_set_domain({"TestH$Base"}));
}

TEST_F(GlobalTypeAnalysisTest, IFieldsNullnessTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestI;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto one_m1 = get_field("TestI$One;.m1:Lcom/facebook/redextest/TestI$Foo;");
  auto ftype = wps.get_field_type(one_m1);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestI$Foo")));
  auto one_m2 = get_field("TestI$One;.m2:Lcom/facebook/redextest/TestI$Foo;");
  ftype = wps.get_field_type(one_m2);
  // Because only_aggregate_safely_inferrable_fields = false, type is available.
  // Otherwise, it would be top()
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestI$Foo")));

  auto two_m1 = get_field("TestI$Two;.m1:Lcom/facebook/redextest/TestI$Foo;");
  ftype = wps.get_field_type(two_m1);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestI$Foo")));

  auto two_m2 = get_field("TestI$Two;.m2:Lcom/facebook/redextest/TestI$Foo;");
  ftype = wps.get_field_type(two_m2);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestI$Foo")));
}

TEST_F(GlobalTypeAnalysisTest, PrimitiveArrayTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestJ;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto create_byte_array = get_method("TestJ;.createByteArray", "", "[B");
  auto rtype = wps.get_return_type(create_byte_array);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.is_not_null());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(get_type_simple("[B")));
}

TEST_F(GlobalTypeAnalysisTest, InstanceSensitiveCtorTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestK;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto field_f = get_field("TestK$Foo;.f:Lcom/facebook/redextest/TestK$A;");
  auto ftype = wps.get_field_type(field_f);
  // Because only_aggregate_safely_inferrable_fields = false, type is available.
  // Otherwise, it would be top()
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestK$A")));
  const auto& set_domain = ftype.get_set_domain();
  EXPECT_EQ(set_domain.get_types(),
            get_type_set({get_type("TestK$A"), get_type("TestK$B")}));
}

TEST_F(GlobalTypeAnalysisTest, InstanceSensitiveCtorNullnessTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestL;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto field_f = get_field("TestL$Foo;.f:Lcom/facebook/redextest/TestL$A;");
  auto ftype = wps.get_field_type(field_f);
  EXPECT_FALSE(ftype.is_top());
  EXPECT_TRUE(ftype.is_nullable());
  EXPECT_EQ(ftype.get_single_domain(),
            SingletonDexTypeDomain(get_type("TestL$A")));
}

TEST_F(GlobalTypeAnalysisTest, ArrayNullnessEscapeTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestM;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto call_native =
      get_method("TestM;.callNative", "", "Lcom/facebook/redextest/TestM$A;");
  auto rtype = wps.get_return_type(call_native);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_FALSE(rtype.is_not_null());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestM$A;")));
}

TEST_F(GlobalTypeAnalysisTest, ArrayNullnessEscape2Test) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestN;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();

  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto dance1 = get_method(
      "TestN;.danceWithArray1", "", "Lcom/facebook/redextest/TestN$A;");
  auto rtype = wps.get_return_type(dance1);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_FALSE(rtype.is_not_null());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestN$A;")));

  auto dance2 = get_method(
      "TestN;.danceWithArray2", "", "Lcom/facebook/redextest/TestN$A;");
  rtype = wps.get_return_type(dance2);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_FALSE(rtype.is_not_null());
  EXPECT_TRUE(rtype.is_nullable());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestN$A;")));
}

TEST_F(GlobalTypeAnalysisTest, MultipleCalleeTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestO;.main:()V");
  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();

  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto base_same =
      get_method("TestO$Base;.same", "", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(base_same != nullptr);
  auto rtype = wps.get_return_type(base_same);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestO$B;")));

  auto sub_same =
      get_method("TestO$Sub;.same", "", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(sub_same != nullptr);
  rtype = wps.get_return_type(sub_same);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestO$B;")));

  auto call_same =
      get_method("TestO;.callSame", "I", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(call_same != nullptr);
  rtype = wps.get_return_type(call_same);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestO$B;")));

  auto base_diff =
      get_method("TestO$Base;.diff", "", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(base_diff != nullptr);
  rtype = wps.get_return_type(base_diff);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestO$A;")));

  auto sub_diff =
      get_method("TestO$Sub;.diff", "", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(sub_diff != nullptr);
  rtype = wps.get_return_type(sub_diff);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_EQ(rtype.get_single_domain(),
            SingletonDexTypeDomain(
                get_type_simple("Lcom/facebook/redextest/TestO$B;")));

  auto call_diff =
      get_method("TestO;.callDiff", "I", "Lcom/facebook/redextest/TestO$I;");
  EXPECT_TRUE(call_diff != nullptr);
  rtype = wps.get_return_type(call_diff);
  EXPECT_FALSE(rtype.is_top());
  EXPECT_TRUE(rtype.get_single_domain().is_top());
}

// This makes sure we are no longer running in a bug where all code following an
// invocation within a constructor was considered unreachable.
TEST_F(GlobalTypeAnalysisTest, InvokeInInitRegressionTest) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestP;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto meth = get_method("TestP;.bar", "TestP$C");
  auto rtype = wps.get_return_type(meth);
  EXPECT_TRUE(rtype.is_nullable());
  const auto& single_domain = rtype.get_single_domain();
  EXPECT_EQ(single_domain, SingletonDexTypeDomain(get_type("TestP$C")));
  const auto& set_domain = rtype.get_set_domain();
  EXPECT_EQ(set_domain.get_types(), get_type_set({get_type("TestP$C")}));
}

TEST_F(GlobalTypeAnalysisTest, StaticFieldTypes) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestQ;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto meth =
      get_method(/* name */ "TestQ;.foo",
                 /* params */ "I",
                 /* return_type */ "Lcom/facebook/redextest/TestQ$Base;");
  auto rtype = wps.get_return_type(meth);
  EXPECT_TRUE(rtype.is_nullable());
  const auto& single_domain = rtype.get_single_domain();
  EXPECT_EQ(single_domain, SingletonDexTypeDomain(get_type("TestQ$Base")));
  const auto& set_domain = rtype.get_set_domain();
  EXPECT_EQ(
      set_domain.get_types(),
      get_type_set({get_type("TestQ$Derived1"), get_type("TestQ$Derived2")}));
}

TEST_F(GlobalTypeAnalysisTest, EntryPointArgumentTypes) {
  auto scope = build_class_scope(stores);
  set_root_method(
      "Lcom/facebook/redextest/TestR;.root:(Lcom/facebook/redextest/"
      "TestR$Derived1;)V");
  set_root_method(
      "Lcom/facebook/redextest/TestR;.staticRoot:(Lcom/facebook/redextest/"
      "TestR$Derived2;)V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  {
    auto meth =
        get_method(/* name */ "TestR;.passThrough",
                   /* params */ "Lcom/facebook/redextest/TestR$Base;",
                   /* return_type */ "Lcom/facebook/redextest/TestR$Base;");
    auto rtype = wps.get_return_type(meth);
    EXPECT_TRUE(rtype.is_nullable());
    const auto& single_domain = rtype.get_single_domain();
    EXPECT_EQ(single_domain,
              SingletonDexTypeDomain(get_type("TestR$Derived1")));

    // The SmallSetDexTypeDomain should be top for root method arguments since
    // their precise types are unknown.
    const auto& set_domain = rtype.get_set_domain();
    EXPECT_TRUE(set_domain.is_top());
  }

  {
    auto meth =
        get_method(/* name */ "TestR;.staticPassThrough",
                   /* params */ "Lcom/facebook/redextest/TestR$Base;",
                   /* return_type */ "Lcom/facebook/redextest/TestR$Base;");
    auto rtype = wps.get_return_type(meth);
    EXPECT_TRUE(rtype.is_nullable());
    const auto& single_domain = rtype.get_single_domain();
    EXPECT_EQ(single_domain,
              SingletonDexTypeDomain(get_type("TestR$Derived2")));
    const auto& set_domain = rtype.get_set_domain();
    EXPECT_TRUE(set_domain.is_top());
  }
}

TEST_F(GlobalTypeAnalysisTest, UnassignedFields) {
  auto scope = build_class_scope(stores);
  set_root_method("Lcom/facebook/redextest/TestS;.main:()V");

  auto options = test::make_default_options();
  auto analysis = GlobalTypeAnalysis::make_default();
  auto gta = analysis.analyze(scope, *options);
  auto wps = gta->get_whole_program_state();

  auto field = get_field("TestS$One;.m1:Lcom/facebook/redextest/TestS$Foo;");
  auto field_type = wps.get_field_type(field);
  EXPECT_FALSE(field_type.is_top());
  EXPECT_TRUE(field_type.is_null());

  const auto& single_domain = field_type.get_single_domain();
  EXPECT_TRUE(single_domain.is_none());
  const auto& set_domain = field_type.get_set_domain();
  EXPECT_EQ(set_domain.get_types(), get_type_set({}));
}

} // namespace marianatrench
