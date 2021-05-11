/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iterator>
#include <mutex>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <RedexResources.h>
#include <Resolver.h>
#include <Walkers.h>

#include <mariana-trench/Features.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

ModelGenerator::ModelGenerator(const std::string& name, Context& context)
    : name_(name),
      context_(context),
      options_(*context.options),
      methods_(*context.methods),
      overrides_(*context.overrides) {
  mt_assert_log(context.options != nullptr, "invalid context");
  mt_assert_log(context.methods != nullptr, "invalid context");
  mt_assert_log(context.overrides != nullptr, "invalid context");
}

std::vector<Model> ModelGenerator::run_optimized(
    const Methods& methods,
    const MethodMappings& /* method_mappings */) {
  return this->run(methods);
}

std::vector<Model> MethodVisitorModelGenerator::run(const Methods& methods) {
  return this->run_impl(methods.begin(), methods.end());
}

namespace {

void create_name_to_method(
    const Method* method,
    ConcurrentMap<std::string, MethodHashedSet>& method_mapping) {
  const auto& method_name = method->get_name();
  method_mapping.update(
      method_name,
      [&](const std::string& /* name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}

void create_class_to_method(
    const Method* method,
    ConcurrentMap<std::string, MethodHashedSet>& method_mapping) {
  std::string parent_class = method->get_class()->get_name()->str();
  method_mapping.update(
      parent_class,
      [&](const std::string& /* parent_name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}

void create_class_to_override_method(
    const Method* method,
    ConcurrentMap<std::string, MethodHashedSet>& method_mapping) {
  std::string class_name = method->get_class()->get_name()->str();
  auto* dex_class = redex::get_class(class_name);
  if (!dex_class) {
    return;
  }
  std::unordered_set<std::string> parent_classes =
      generator::get_parents_from_class(
          dex_class, /* include_interfaces */ true);
  parent_classes.insert(class_name);
  for (const auto& parent_class : parent_classes) {
    method_mapping.update(
        parent_class,
        [&](const std::string& /* parent_name */,
            MethodHashedSet& methods,
            bool /* exists */) { methods.add(method); });
  }
}

void create_signature_to_method(
    const Method* method,
    ConcurrentMap<std::string, MethodHashedSet>& method_mapping) {
  std::string signature = method->signature();
  method_mapping.update(
      signature,
      [&](const std::string& /* parent_name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}
} // namespace

MethodMappings::MethodMappings(const Methods& methods) {
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    create_name_to_method(method, name_to_methods);
    create_class_to_method(method, class_to_methods);
    create_class_to_override_method(method, class_to_override_methods);
    create_signature_to_method(method, signature_to_methods);
  });
  for (const auto* method : methods) {
    all_methods.add(method);
    queue.add_item(method);
  }
  queue.run_all();
}

const std::string& generator::get_class_name(const Method* method) {
  return method->get_class()->get_name()->str();
}

const std::string& generator::get_method_name(const Method* method) {
  return method->get_name();
}

std::optional<std::string> generator::get_super_type(const Method* method) {
  const DexClass* current_class = type_class(method->get_class());
  const DexType* super_class = current_class->get_super_class();
  if (!super_class) {
    return std::nullopt;
  }
  return super_class->get_name()->str();
}

std::unordered_set<std::string> generator::get_interfaces_from_class(
    DexClass* dex_class) {
  std::unordered_set<std::string> interfaces;
  std::deque<DexType*> interface_types =
      dex_class->get_interfaces()->get_type_list();
  while (!interface_types.empty()) {
    DexType* interface = interface_types.back();
    interface_types.pop_back();
    interfaces.emplace(interface->get_name()->str());
    DexClass* interface_class = type_class(interface);
    if (interface_class) {
      const auto& super_interface_types =
          interface_class->get_interfaces()->get_type_list();
      interface_types.insert(
          interface_types.end(),
          super_interface_types.begin(),
          super_interface_types.end());
    }
  }
  return interfaces;
}

std::unordered_set<std::string> generator::get_parents_from_class(
    DexClass* dex_class,
    bool include_interfaces = false) {
  std::unordered_set<std::string> parent_classes;

  while (dex_class != nullptr) {
    const DexType* super_type = dex_class->get_super_class();
    if (!super_type) {
      break;
    }
    parent_classes.emplace(super_type->get_name()->str());
    DexClass* super_class = type_class(super_type);
    if (include_interfaces) {
      auto interfaces = generator::get_interfaces_from_class(dex_class);
      parent_classes.merge(interfaces);
    }
    dex_class = super_class;
  }
  return parent_classes;
}

std::unordered_set<std::string> generator::get_custom_parents_from_class(
    DexClass* dex_class) {
  std::unordered_set<std::string> parent_classes;

  while (true) {
    const DexType* super_type = dex_class->get_super_class();
    if (super_type &&
        !boost::starts_with(super_type->get_name()->str(), "Landroid") &&
        !boost::starts_with(super_type->get_name()->str(), "Ljava")) {
      parent_classes.emplace(super_type->get_name()->str());
      DexClass* super_class = type_class(super_type);
      if (super_class) {
        dex_class = super_class;
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return parent_classes;
}

std::string generator::get_outer_class(const std::string& classname) {
  auto class_start = classname.substr(0, classname.find(";", 0));
  return class_start.substr(0, class_start.find("$", 0));
}

std::vector<std::pair<ParameterPosition, const DexType*>>
generator::get_argument_types(const DexMethod* dex_method) {
  std::vector<std::pair<ParameterPosition, const DexType*>> arguments;

  const auto& method_prototype = dex_method->get_proto();
  if (!method_prototype) {
    return arguments;
  }

  const auto& dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return arguments;
  }

  // We exclude argument 0 ("this/self") for instance methods
  ParameterPosition index = (dex_method->get_access() & ACC_STATIC) ? 0 : 1;
  for (const auto dex_argument : *dex_arguments) {
    arguments.push_back(std::make_pair(index++, dex_argument));
  }
  return arguments;
}

std::vector<std::pair<ParameterPosition, const DexType*>>
generator::get_argument_types(const Method* method) {
  return get_argument_types(method->dex_method());
}

std::vector<std::pair<ParameterPosition, std::string>>
generator::get_argument_types_string(const Method* method) {
  std::vector<std::pair<ParameterPosition, std::string>> arguments;
  auto argument_types_dex = generator::get_argument_types(method);

  for (const auto& argument_type_dex : argument_types_dex) {
    arguments.push_back(std::make_pair(
        argument_type_dex.first, argument_type_dex.second->str()));
  }

  return arguments;
}

std::optional<const DexType*> generator::get_return_type(const Method* method) {
  const auto& method_prototype = method->get_proto();
  if (!method_prototype) {
    return std::nullopt;
  }

  const auto& return_type = method_prototype->get_rtype();
  if (!return_type) {
    return std::nullopt;
  }

  return return_type;
}

std::optional<std::string> generator::get_return_type_string(
    const Method* method) {
  const auto& return_type = generator::get_return_type(method);
  if (!return_type) {
    return std::nullopt;
  }
  return (*return_type)->str();
}

bool generator::is_numeric_data_type(const DataType& type) {
  std::unordered_set<DataType> numeric_types = {
      DataType::Short,
      DataType::Float,
      DataType::Int,
      DataType::Long,
      DataType::Double};
  return numeric_types.find(type) != numeric_types.end();
}

void static verify_parameter_position(
    const Method* method,
    ParameterPosition parameter_position) {
  mt_assert(method != nullptr);
  ParameterPosition argument_size =
      generator::get_argument_types(method).size();
  if (parameter_position > argument_size) {
    std::string error;
    if (!argument_size) {
      error.append("no argument size");
    } else {
      error.append(fmt::format("argument size is {}", argument_size));
    }
    error.append(fmt::format(", parameter position is {}", parameter_position));
    throw std::invalid_argument(error);
  }
}

void generator::add_propagation_to_return(
    Context& context,
    Model& model,
    ParameterPosition parameter_position,
    const std::vector<std::string>& features) {
  verify_parameter_position(model.method(), parameter_position);

  auto input = AccessPath(Root(Root::Kind::Argument, parameter_position));
  auto output = AccessPath(Root(Root::Kind::Return));
  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.features->get(feature));
  }
  auto propagation = Propagation(
      input,
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ user_features);
  model.add_propagation(propagation, output);
}

void generator::add_propagation_to_parameter(
    Context& context,
    Model& model,
    ParameterPosition from,
    ParameterPosition to,
    const std::vector<std::string>& features) {
  verify_parameter_position(model.method(), from);
  verify_parameter_position(model.method(), to);

  auto input = AccessPath(Root(Root::Kind::Argument, from));
  auto output = AccessPath(Root(Root::Kind::Argument, to));
  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.features->get(feature));
  }
  auto propagation = Propagation(
      input,
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ user_features);
  model.add_propagation(propagation, output);
}

void generator::add_propagation_to_self(
    Context& context,
    Model& model,
    ParameterPosition parameter_position,
    const std::vector<std::string>& features) {
  add_propagation_to_parameter(
      context,
      model,
      parameter_position,
      /* parameter self */ 0,
      features);
}

namespace {

/* Checks whether the given annotation set has a given annotation
 * type/value. */
bool has_annotation(
    const DexAnnotationSet* annotations_set,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values) {
  if (!annotations_set) {
    return false;
  }

  for (const auto& annotation : annotations_set->get_annotations()) {
    const DexType* annotation_type = annotation->type();
    if (!annotation_type || annotation_type->c_str() != expected_type) {
      continue;
    }

    // If we expect a certain value, check values of the current
    // annotation.
    if (expected_values && !expected_values->empty()) {
      for (const auto& element : annotation->anno_elems()) {
        if (expected_values->find(element.encoded_value->show()) !=
            expected_values->end()) {
          LOG(4,
              "Found annotation type {} value {}.",
              annotation_type->str(),
              element.encoded_value->show());
          return true;
        }
      }
    }

    // If we do not expect a certain value, return as we found the
    // annotation.
    return true;
  }

  return false;
}

} // namespace

bool generator::has_annotation(
    const DexMethod* method,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values) {
  if (!method) {
    return false;
  }
  return marianatrench::has_annotation(
      method->get_anno_set(), expected_type, expected_values);
}

bool generator::has_annotation(
    const DexClass* dex_class,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values) {
  if (!dex_class) {
    return false;
  }
  return marianatrench::has_annotation(
      dex_class->get_anno_set(), expected_type, expected_values);
}

Frame generator::source(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports) {
  // These ports must go with canonical names.
  mt_assert(
      callee_port != Root::Kind::Anchor && callee_port != Root::Kind::Producer);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.features->get(feature));
  }
  return Frame(
      /* kind */ context.kinds->get(kind),
      /* callee_port */ AccessPath(Root(callee_port)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ MethodSet{method},
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* local_positions */ {},
      /* canonical_names */ {});
}

Frame generator::sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports) {
  // These ports must go with canonical names.
  mt_assert(
      callee_port != Root::Kind::Anchor && callee_port != Root::Kind::Producer);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.features->get(feature));
  }
  return Frame(
      /* kind */ context.kinds->get(kind),
      /* callee_port */ AccessPath(Root(callee_port)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ MethodSet{method},
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* local_positions */ {},
      /* canonical_names */ {});
}

Frame generator::partial_sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::string& label,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports) {
  // These ports must go with canonical names.
  mt_assert(
      callee_port != Root::Kind::Anchor && callee_port != Root::Kind::Producer);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.features->get(feature));
  }
  return Frame(
      /* kind */ context.kinds->get_partial(kind, label),
      /* callee_port */ AccessPath(Root(callee_port)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ MethodSet{method},
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* local_positions */ {},
      /* canonical_names */ {});
}

} // namespace marianatrench
