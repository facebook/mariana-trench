/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/OriginFactory.h>
#include <mariana-trench/OriginSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

ModelGenerator::ModelGenerator(const ModelGeneratorName* name, Context& context)
    : name_(name),
      context_(context),
      options_(*context.options),
      methods_(*context.methods),
      overrides_(*context.overrides) {
  mt_assert_log(context.options != nullptr, "invalid options in context");
  mt_assert_log(context.methods != nullptr, "invalid methods in context");
  mt_assert_log(context.overrides != nullptr, "invalid overrides in context");
}

ModelGenerator::ModelGenerator(const std::string& name, Context& context)
    : name_(context.model_generator_name_factory->create(name)),
      context_(context),
      options_(*context.options),
      methods_(*context.methods),
      overrides_(*context.overrides) {
  mt_assert_log(context.options != nullptr, "invalid options in context");
  mt_assert_log(context.methods != nullptr, "invalid methods in context");
  mt_assert_log(context.overrides != nullptr, "invalid overrides in context");
}

ModelGeneratorResult ModelGenerator::run(
    const Methods& methods,
    const Fields& fields) {
  return {
      /* method_models */ emit_method_models(methods),
      /* field_models */ emit_field_models(fields)};
}

ModelGeneratorResult ModelGenerator::run_optimized(
    const Methods& methods,
    const MethodMappings& method_mappings,
    const Fields& fields) {
  return {
      /* method_models */ emit_method_models_optimized(
          methods, method_mappings),
      /* field_models */ emit_field_models(fields)};
}

std::vector<Model> ModelGenerator::emit_method_models_optimized(
    const Methods& methods,
    const MethodMappings& /* method_mappings */) {
  return this->emit_method_models(methods);
}

std::vector<Model> MethodVisitorModelGenerator::emit_method_models(
    const Methods& methods) {
  return this->run_impl(methods.begin(), methods.end());
}

std::vector<FieldModel> FieldVisitorModelGenerator::emit_field_models(
    const Fields& fields) {
  return this->run_impl(fields.begin(), fields.end());
}

std::string_view generator::get_class_name(const Method* method) {
  return method->get_class()->get_name()->str();
}

std::string_view generator::get_method_name(const Method* method) {
  return method->get_name();
}

std::optional<std::string_view> generator::get_super_type(
    const Method* method) {
  const DexClass* current_class = type_class(method->get_class());
  const DexType* super_class = current_class->get_super_class();
  if (!super_class) {
    return std::nullopt;
  }
  return super_class->get_name()->str();
}

std::unordered_set<std::string_view> generator::get_interfaces_from_class(
    DexClass* dex_class) {
  std::unordered_set<std::string_view> interfaces;
  std::vector<DexType*> interface_types = std::vector<DexType*>(
      dex_class->get_interfaces()->begin(), dex_class->get_interfaces()->end());
  while (!interface_types.empty()) {
    DexType* interface = interface_types.back();
    interface_types.pop_back();
    interfaces.emplace(interface->get_name()->str());
    DexClass* interface_class = type_class(interface);
    if (interface_class) {
      const auto& super_interface_types = *interface_class->get_interfaces();
      interface_types.insert(
          interface_types.end(),
          super_interface_types.begin(),
          super_interface_types.end());
    }
  }
  return interfaces;
}

std::unordered_set<std::string_view> generator::get_parents_from_class(
    DexClass* dex_class,
    bool include_interfaces = false) {
  std::unordered_set<std::string_view> parent_classes;

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

std::unordered_set<std::string_view> generator::get_custom_parents_from_class(
    DexClass* dex_class) {
  std::unordered_set<std::string_view> parent_classes;

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

std::string generator::get_outer_class(std::string_view classname) {
  auto class_start = classname.substr(0, classname.find(";", 0));
  return str_copy(class_start.substr(0, class_start.find("$", 0)));
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

std::vector<std::pair<ParameterPosition, std::string_view>>
generator::get_argument_types_string(const Method* method) {
  std::vector<std::pair<ParameterPosition, std::string_view>> arguments;
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

std::optional<std::string_view> generator::get_return_type_string(
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
    CollapseDepth collapse_depth,
    const std::vector<std::string>& features) {
  verify_parameter_position(model.method(), parameter_position);

  auto output = AccessPath(Root(Root::Kind::Return));
  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.feature_factory->get(feature));
  }
  model.add_propagation(PropagationConfig(
      /* input_path */ AccessPath(
          Root(Root::Kind::Argument, parameter_position)),
      /* kind */ context.kind_factory->local_return(),
      /* output_paths */ PathTreeDomain{{Path{}, collapse_depth}},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ user_features));
}

void generator::add_propagation_to_parameter(
    Context& context,
    Model& model,
    ParameterPosition from,
    ParameterPosition to,
    CollapseDepth collapse_depth,
    const std::vector<std::string>& features) {
  verify_parameter_position(model.method(), from);
  verify_parameter_position(model.method(), to);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.feature_factory->get(feature));
  }
  model.add_propagation(PropagationConfig(
      /* input_path */ AccessPath(Root(Root::Kind::Argument, from)),
      /* kind */ context.kind_factory->local_argument(to),
      /* output_paths */ PathTreeDomain{{Path{}, collapse_depth}},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ user_features));
}

void generator::add_propagation_to_self(
    Context& context,
    Model& model,
    ParameterPosition parameter_position,
    CollapseDepth collapse_depth,
    const std::vector<std::string>& features) {
  add_propagation_to_parameter(
      context,
      model,
      parameter_position,
      /* parameter self */ 0,
      collapse_depth,
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
    const DexField* field,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values) {
  if (!field) {
    return false;
  }
  return marianatrench::has_annotation(
      field->get_anno_set(), expected_type, expected_values);
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

TaintConfig generator::source(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports,
    RootSetAbstractDomain via_value_of_ports) {
  // These ports must go with canonical names.
  mt_assert(
      callee_port != Root::Kind::Anchor && callee_port != Root::Kind::Producer);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.feature_factory->get(feature));
  }

  const auto* port =
      context.access_path_factory->get(AccessPath(Root(callee_port)));

  return TaintConfig(
      /* kind */ context.kind_factory->get(kind),
      /* callee_port */ *port,
      /* callee */ nullptr,
      /* call_kind */ CallKind::declaration(),
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* via_value_of_ports */ via_value_of_ports,
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

TaintConfig generator::sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports,
    RootSetAbstractDomain via_value_of_ports,
    OriginSet origins) {
  CallKind call_kind = CallKind::declaration();
  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.feature_factory->get(feature));
  }

  const auto* port =
      context.access_path_factory->get(AccessPath(Root(callee_port)));

  return TaintConfig(
      /* kind */ context.kind_factory->get(kind),
      /* callee_port */ *port,
      /* callee */ nullptr,
      /* call_kind */ call_kind,
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ std::move(origins),
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* via_type_of_ports */ via_value_of_ports,
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

TaintConfig generator::partial_sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::string& label,
    const std::vector<std::string>& features,
    Root::Kind callee_port,
    RootSetAbstractDomain via_type_of_ports,
    RootSetAbstractDomain via_value_of_ports) {
  // These ports must go with canonical names.
  mt_assert(
      callee_port != Root::Kind::Anchor && callee_port != Root::Kind::Producer);

  FeatureSet user_features;
  for (const auto& feature : features) {
    user_features.add(context.feature_factory->get(feature));
  }

  const auto* port =
      context.access_path_factory->get(AccessPath(Root(callee_port)));

  return TaintConfig(
      /* kind */ context.kind_factory->get_partial(kind, label),
      /* callee_port */ *port,
      /* callee */ nullptr,
      /* call_kind */ CallKind::declaration(),
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* inferred features */ FeatureMayAlwaysSet::bottom(),
      /* user features */ user_features,
      /* via_type_of_ports */ via_type_of_ports,
      /* via_value_of_ports */ via_value_of_ports,
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

} // namespace marianatrench
