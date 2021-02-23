/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Creators.h>
#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class ModelGeneratorTest : public test::Test {};

bool compare_methods(const Method* left, const Method* right) {
  if (left->get_name() == right->get_name()) {
    return left->get_class()->get_name()->str() <
        right->get_class()->get_name()->str();
  }
  return left->get_name() < right->get_name();
};

} // namespace

TEST_F(ModelGeneratorTest, MappingGenerator) {
  Scope scope;
  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V");
  auto* dex_first_overriding_method = redex::create_void_method(
      scope,
      /* class_name */ "LSubclass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ dex_base_method->get_class());
  auto* dex_second_overriding_method = redex::create_void_method(
      scope,
      /* class_name */ "LSubSubclass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ dex_first_overriding_method->get_class());
  DexStore store("test-stores");

  auto interface = DexType::make_type("LInterface;");
  ClassCreator creator(interface);
  creator.set_access(DexAccessFlags::ACC_INTERFACE);
  creator.set_super(type::java_lang_Object());
  creator.create();
  auto super_interface = DexType::make_type("LSuperInterface;");
  ClassCreator super_creator(super_interface);
  super_creator.set_access(DexAccessFlags::ACC_INTERFACE);
  super_creator.set_super(type::java_lang_Object());
  super_creator.create();

  type_class(dex_base_method->get_class())
      ->set_interfaces(DexTypeList::make_type_list({interface}));
  type_class(interface)->set_interfaces(
      DexTypeList::make_type_list({super_interface}));

  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* array_allocation_method = context.methods->get(
      context.artificial_methods->array_allocation_method());
  auto* base_method = context.methods->get(dex_base_method);
  auto* first_overriding_method =
      context.methods->get(dex_first_overriding_method);
  auto* second_overriding_method =
      context.methods->get(dex_second_overriding_method);

  {
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_name_to_methods = {
            {"onReceive",
             {base_method, first_overriding_method, second_overriding_method}},
            {"allocateArray", {array_allocation_method}},
        };
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_class_to_methods = {
            {"Lcom/mariana_trench/artificial/ArrayAllocation;",
             {array_allocation_method}},
            {"LClass;", {base_method}},
            {"LSubclass;", {first_overriding_method}},
            {"LSubSubclass;", {second_overriding_method}}};
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_class_to_override_methods = {
            {"Lcom/mariana_trench/artificial/ArrayAllocation;",
             {array_allocation_method}},
            {"LSuperInterface;",
             {base_method, first_overriding_method, second_overriding_method}},
            {"LInterface;",
             {base_method, first_overriding_method, second_overriding_method}},
            {"LClass;",
             {base_method, first_overriding_method, second_overriding_method}},
            {"LSubclass;", {first_overriding_method, second_overriding_method}},
            {"LSubSubclass;", {second_overriding_method}},
            {"Ljava/lang/Object;",
             {array_allocation_method,
              base_method,
              first_overriding_method,
              second_overriding_method}},
        };

    const auto method_mappings = MethodMappings(*context.methods);

    auto name_to_methods_result =
        std::unordered_map<std::string, marianatrench::MethodSet>(
            method_mappings.name_to_methods.begin(),
            method_mappings.name_to_methods.end());
    std::unordered_map<std::string, std::vector<const Method*>>
        name_to_methods_map;
    for (const auto& pair : name_to_methods_result) {
      std::vector<const Method*> methods = {
          pair.second.begin(), pair.second.end()};
      name_to_methods_map.insert({pair.first, methods});
    }
    for (auto& pair : name_to_methods_map) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    for (auto& pair : expected_name_to_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(name_to_methods_map, expected_name_to_methods);

    auto class_to_methods_result =
        std::unordered_map<std::string, marianatrench::MethodSet>(
            method_mappings.class_to_methods.begin(),
            method_mappings.class_to_methods.end());
    std::unordered_map<std::string, std::vector<const Method*>>
        class_to_methods_map;
    for (const auto& pair : class_to_methods_result) {
      std::vector<const Method*> methods = {
          pair.second.begin(), pair.second.end()};
      class_to_methods_map.insert({pair.first, methods});
    }
    for (auto& pair : class_to_methods_map) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    for (auto& pair : expected_class_to_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(class_to_methods_map, expected_class_to_methods);

    auto class_to_override_methods_result =
        std::unordered_map<std::string, marianatrench::MethodSet>(
            method_mappings.class_to_override_methods.begin(),
            method_mappings.class_to_override_methods.end());
    std::unordered_map<std::string, std::vector<const Method*>>
        class_to_override_methods_map;
    for (const auto& pair : class_to_override_methods_result) {
      std::vector<const Method*> methods = {
          pair.second.begin(), pair.second.end()};
      class_to_override_methods_map.insert({pair.first, methods});
    }
    for (auto& pair : class_to_override_methods_map) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    for (auto& pair : expected_class_to_override_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(
        class_to_override_methods_map, expected_class_to_override_methods);
  }
}
