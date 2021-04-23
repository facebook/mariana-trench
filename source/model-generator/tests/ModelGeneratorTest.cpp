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
    const auto& left_class_name = left->get_class()->get_name()->str();
    const auto& right_class_name = right->get_class()->get_name()->str();
    if (left_class_name == right_class_name) {
      return left->parameter_type_overrides() <
          right->parameter_type_overrides();
    }
    return left_class_name < right_class_name;
  }
  return left->get_name() < right->get_name();
};

std::unordered_map<std::string, std::vector<const Method*>> sort_mapping(
    const ConcurrentMap<std::string, marianatrench::MethodSet>& mapping) {
  auto unordered_mapping =
      std::unordered_map<std::string, marianatrench::MethodSet>(
          mapping.begin(), mapping.end());
  std::unordered_map<std::string, std::vector<const Method*>> result;

  for (const auto& [key, methods] : unordered_mapping) {
    std::vector<const Method*> new_methods(methods.begin(), methods.end());
    result.insert({key, new_methods});
  }

  for (auto& [_key, methods] : result) {
    std::sort(methods.begin(), methods.end(), compare_methods);
  }
  return result;
}
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
  auto* second_overriding_method_with_overrides = context.methods->create(
      dex_second_overriding_method, {{0, DexType::get_type("LString;")}});

  {
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_name_to_methods = {
            {"allocateArray", {array_allocation_method}},
            {"onReceive",
             {base_method,
              first_overriding_method,
              second_overriding_method,
              second_overriding_method_with_overrides}},
        };
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_class_to_methods = {
            {"Lcom/mariana_trench/artificial/ArrayAllocation;",
             {array_allocation_method}},
            {"LClass;", {base_method}},
            {"LSubclass;", {first_overriding_method}},
            {"LSubSubclass;",
             {second_overriding_method,
              second_overriding_method_with_overrides}}};
    std::unordered_map<std::string, std::vector<const Method*>>
        expected_signature_to_methods = {
            {"LClass;.onReceive:(Landroid/content/Context;Landroid/content/Intent;)V",
             {base_method}},
            {"LSubclass;.onReceive:(Landroid/content/Context;Landroid/content/Intent;)V",
             {first_overriding_method}},
            {"LSubSubclass;.onReceive:(Landroid/content/Context;Landroid/content/Intent;)V",
             {second_overriding_method,
              second_overriding_method_with_overrides}},
            {"Lcom/mariana_trench/artificial/ArrayAllocation;.allocateArray:(I)V",
             {array_allocation_method}},
        };

    const auto method_mappings = MethodMappings(*context.methods);
    std::unordered_map<std::string, std::vector<const Method*>>
        name_to_methods_map = sort_mapping(method_mappings.name_to_methods);
    for (auto& pair : expected_name_to_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(name_to_methods_map, expected_name_to_methods);

    std::unordered_map<std::string, std::vector<const Method*>>
        class_to_methods_map = sort_mapping(method_mappings.class_to_methods);
    for (auto& pair : expected_class_to_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(class_to_methods_map, expected_class_to_methods);

    std::unordered_map<std::string, std::vector<const Method*>>
        signature_to_methods_map =
            sort_mapping(method_mappings.signature_to_methods);
    for (auto& pair : expected_signature_to_methods) {
      std::sort(pair.second.begin(), pair.second.end(), compare_methods);
    }
    EXPECT_EQ(signature_to_methods_map, expected_signature_to_methods);
  }
}
