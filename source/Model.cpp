/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/range/adaptor/filtered.hpp>
#include <fmt/format.h>

#include <Show.h>
#include <TypeUtil.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>
#include <mariana-trench/model-generator/TaintConfigTemplate.h>

namespace marianatrench {

namespace {

class ModelConsistencyError {
 public:
  static void raise(const std::string& what) {
    ERROR(1, "Model Consistency Error: {}", what);
    EventLogger::log_event("model_consistency_error", what);
  }
};

template <typename DomainType>
bool leq_frozen(
    const DomainType& left,
    const DomainType& right,
    bool left_is_frozen,
    bool right_is_frozen) {
  if (left_is_frozen == right_is_frozen) {
    return left.leq(right);
  }

  return right_is_frozen;
}

template <typename DomainType>
void join_with_frozen(
    DomainType& left,
    const DomainType& right,
    bool left_is_frozen,
    bool right_is_frozen) {
  if (left_is_frozen == right_is_frozen) {
    left.join_with(right);
  } else if (right_is_frozen) {
    left = right;
  }
}

} // namespace

std::string model_mode_to_string(Model::Mode mode) {
  switch (mode) {
    case Model::Mode::SkipAnalysis:
      return "skip-analysis";
    case Model::Mode::AddViaObscureFeature:
      return "add-via-obscure-feature";
    case Model::Mode::TaintInTaintOut:
      return "taint-in-taint-out";
    case Model::Mode::TaintInTaintThis:
      return "taint-in-taint-this";
    case Model::Mode::NoJoinVirtualOverrides:
      return "no-join-virtual-overrides";
    case Model::Mode::NoCollapseOnPropagation:
      return "no-collapse-on-propagation";
    case Model::Mode::AliasMemoryLocationOnInvoke:
      return "alias-memory-location-on-invoke";
    case Model::Mode::StrongWriteOnPropagation:
      return "strong-write-on-propagation";
    case Model::Mode::_Count:
      mt_unreachable();
  }
  mt_unreachable();
}

std::optional<Model::Mode> string_to_model_mode(const std::string& mode) {
  if (mode == "skip-analysis") {
    return Model::Mode::SkipAnalysis;
  } else if (mode == "add-via-obscure-feature") {
    return Model::Mode::AddViaObscureFeature;
  } else if (mode == "taint-in-taint-out") {
    return Model::Mode::TaintInTaintOut;
  } else if (mode == "taint-in-taint-this") {
    return Model::Mode::TaintInTaintThis;
  } else if (mode == "no-join-virtual-overrides") {
    return Model::Mode::NoJoinVirtualOverrides;
  } else if (mode == "no-collapse-on-propagation") {
    return Model::Mode::NoCollapseOnPropagation;
  } else if (mode == "alias-memory-location-on-invoke") {
    return Model::Mode::AliasMemoryLocationOnInvoke;
  } else if (mode == "strong-write-on-propagation") {
    return Model::Mode::StrongWriteOnPropagation;
  } else {
    return std::nullopt;
  }
}

std::string model_freeze_kind_to_string(Model::FreezeKind freeze_kind) {
  switch (freeze_kind) {
    case Model::FreezeKind::Generations:
      return "generations";
    case Model::FreezeKind::ParameterSources:
      return "parameter_sources";
    case Model::FreezeKind::Sinks:
      return "sinks";
    case Model::FreezeKind::Propagations:
      return "propagation";
    case Model::FreezeKind::_Count:
      mt_unreachable();
  }
  mt_unreachable();
}

std::optional<Model::FreezeKind> string_to_freeze_kind(
    const std::string& freeze_kind) {
  if (freeze_kind == "generations") {
    return Model::FreezeKind::Generations;
  } else if (freeze_kind == "parameter_sources") {
    return Model::FreezeKind::ParameterSources;
  } else if (freeze_kind == "sinks") {
    return Model::FreezeKind::Sinks;
  } else if (freeze_kind == "propagation") {
    return Model::FreezeKind::Propagations;
  } else {
    return std::nullopt;
  }
}

Model::Model()
    : method_(nullptr),
      inline_as_getter_(AccessPathConstantDomain::bottom()),
      inline_as_setter_(SetterAccessPathConstantDomain::bottom()) {}

Model::Model(
    const Method* method,
    Context& context,
    Modes modes,
    Frozen frozen,
    const std::vector<std::pair<AccessPath, TaintConfig>>& generations,
    const std::vector<std::pair<AccessPath, TaintConfig>>& parameter_sources,
    const std::vector<std::pair<AccessPath, TaintConfig>>& sinks,
    const std::vector<PropagationConfig>& propagations,
    const std::vector<Sanitizer>& global_sanitizers,
    const std::vector<std::pair<Root, SanitizerSet>>& port_sanitizers,
    const std::vector<std::pair<Root, FeatureSet>>& attach_to_sources,
    const std::vector<std::pair<Root, FeatureSet>>& attach_to_sinks,
    const std::vector<std::pair<Root, FeatureSet>>& attach_to_propagations,
    const std::vector<std::pair<Root, FeatureSet>>& add_features_to_arguments,
    const AccessPathConstantDomain& inline_as_getter,
    const SetterAccessPathConstantDomain& inline_as_setter,
    const ModelGeneratorNameSet& model_generators,
    const IssueSet& issues)
    : method_(method), modes_(modes), frozen_(frozen) {
  if (method_) {
    // Use a set of heuristics to infer the modes of this method.

    auto* code = method_->get_code();
    if (!code) {
      modes_ |= Model::Mode::SkipAnalysis;
      modes_ |= Model::Mode::AddViaObscureFeature;
    }
  }

  if (modes_.test(Model::Mode::TaintInTaintOut)) {
    add_taint_in_taint_out(context);
  }
  if (modes_.test(Model::Mode::TaintInTaintThis)) {
    add_taint_in_taint_this(context);
  }

  for (const auto& [port, source] : generations) {
    add_generation(port, source, context.options->heuristics());
  }

  for (const auto& [port, source] : parameter_sources) {
    add_parameter_source(port, source, context.options->heuristics());
  }

  for (const auto& [port, sink] : sinks) {
    add_sink(port, sink, context.options->heuristics());
  }

  for (const auto& propagation : propagations) {
    add_propagation(propagation, context.options->heuristics());
  }

  for (const auto& sanitizer : global_sanitizers) {
    add_global_sanitizer(sanitizer);
  }

  for (const auto& [root, sanitizers] : port_sanitizers) {
    add_port_sanitizers(sanitizers, root);
  }

  for (const auto& [root, features] : attach_to_sources) {
    add_attach_to_sources(root, features);
  }

  for (const auto& [root, features] : attach_to_sinks) {
    add_attach_to_sinks(root, features);
  }

  for (const auto& [root, features] : attach_to_propagations) {
    add_attach_to_propagations(root, features);
  }

  for (const auto& [root, features] : add_features_to_arguments) {
    add_add_features_to_arguments(root, features);
  }

  set_inline_as_getter(inline_as_getter);
  set_inline_as_setter(inline_as_setter);

  for (const auto* model_generator : model_generators) {
    add_model_generator(model_generator);
  }

  for (const auto& issue : issues) {
    add_issue(issue);
  }
}

bool Model::operator==(const Model& other) const {
  return modes_ == other.modes_ && frozen_ == other.frozen_ &&
      generations_ == other.generations_ &&
      parameter_sources_ == other.parameter_sources_ &&
      call_effect_sources_ == other.call_effect_sources_ &&
      call_effect_sinks_ == other.call_effect_sinks_ &&
      sinks_ == other.sinks_ && propagations_ == other.propagations_ &&
      global_sanitizers_ == other.global_sanitizers_ &&
      port_sanitizers_ == other.port_sanitizers_ &&
      attach_to_sources_ == other.attach_to_sources_ &&
      attach_to_sinks_ == other.attach_to_sinks_ &&
      attach_to_propagations_ == other.attach_to_propagations_ &&
      add_features_to_arguments_ == other.add_features_to_arguments_ &&
      add_via_value_of_features_to_arguments_ ==
      other.add_via_value_of_features_to_arguments_ &&
      inline_as_getter_ == other.inline_as_getter_ &&
      inline_as_setter_ == other.inline_as_setter_ &&
      model_generators_ == other.model_generators_ && issues_ == other.issues_;
}

bool Model::operator!=(const Model& other) const {
  return !(*this == other);
}

Model Model::instantiate(const Method* method, Context& context) const {
  Model model(method, context, modes_, frozen_);

  for (const auto& [port, generation_taint] : generations_.elements()) {
    model.add_generation(port, generation_taint, context.options->heuristics());
  }

  for (const auto& [port, parameter_source_taint] :
       parameter_sources_.elements()) {
    model.add_parameter_source(
        port, parameter_source_taint, context.options->heuristics());
  }

  for (const auto& [port, sink_taint] : sinks_.elements()) {
    model.add_sink(port, sink_taint, context.options->heuristics());
  }

  for (const auto& [port, source_taint] : call_effect_sources_.elements()) {
    model.add_call_effect_source(
        port, source_taint, context.options->heuristics());
  }

  for (const auto& [port, sink_taint] : call_effect_sinks_.elements()) {
    model.add_call_effect_sink(port, sink_taint, context.options->heuristics());
  }

  for (const auto& [input_path, output] : propagations_.elements()) {
    model.add_propagation(input_path, output, context.options->heuristics());
  }

  for (const auto& sanitizer : global_sanitizers_) {
    model.add_global_sanitizer(sanitizer);
  }

  for (const auto& [root, sanitizers] : port_sanitizers_) {
    model.add_port_sanitizers(sanitizers, root);
  }

  for (const auto& [root, features] : attach_to_sources_) {
    model.add_attach_to_sources(root, features);
  }

  for (const auto& [root, features] : attach_to_sinks_) {
    model.add_attach_to_sinks(root, features);
  }

  for (const auto& [root, features] : attach_to_propagations_) {
    model.add_attach_to_propagations(root, features);
  }

  for (const auto& [root, features] : add_features_to_arguments_) {
    model.add_add_features_to_arguments(root, features);
  }

  for (const auto& [root, via_value_of_ports] :
       add_via_value_of_features_to_arguments_) {
    model.add_add_via_value_of_features_to_arguments(root, via_value_of_ports);
  }

  model.set_inline_as_getter(inline_as_getter_);
  model.set_inline_as_setter(inline_as_setter_);

  for (const auto* model_generator : model_generators_) {
    model.add_model_generator(model_generator);
  }

  return model;
}

Model Model::at_callsite(
    const Method* caller,
    const Position* call_position,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context) const {
  const auto* callee = method_;
  // at_callsite() does not make sense if there is no callee
  mt_assert(callee != nullptr);

  Model model;
  model.modes_ = modes_;
  model.frozen_ = frozen_;

  auto maximum_source_sink_distance =
      context.options->maximum_source_sink_distance();

  // To add special features that cannot be done in model generators.
  mt_assert(context.feature_factory != nullptr);

  model.add_features_to_arguments_ = add_features_to_arguments_;
  // Materialize via_value_of_ports into features and add them to
  // add_features_to_arguments
  for (const auto& [root, via_value_of_ports] :
       add_via_value_of_features_to_arguments_) {
    FeatureSet root_features{};
    for (const auto& tagged_root : via_value_of_ports.elements()) {
      if (!tagged_root.root().is_argument() ||
          tagged_root.root().parameter_position() >=
              source_constant_arguments.size()) {
        ERROR(
            1,
            "Invalid port {} provided for add_via_value_of_features_to_arguments_ ports of method {}",
            tagged_root,
            callee->show());
        continue;
      }
      root_features.add(context.feature_factory->get_via_value_of_feature(
          source_constant_arguments[tagged_root.root().parameter_position()],
          tagged_root.tag()));
    }
    model.add_add_features_to_arguments(root, std::move(root_features));
  }

  auto narrowed_class_interval_context = class_interval_context;
  if (class_interval_context.preserves_type_context()) {
    // When it is a "this.*" call, `callee`'s class is within the class
    // hierarchy of `caller`'s class (could be child or parent). The class
    // interval should be the more precise/derived interval.
    auto narrowed_interval = class_interval_context.callee_interval().meet(
        context.class_intervals->get_interval(callee->get_class()));
    // Intervals should intersect for this.* calls.
    mt_assert_log(
        !narrowed_interval.is_bottom(),
        "Caller and callee intervals do not intersect. Caller: %s %s, Callee: %s %s",
        show(caller).c_str(),
        show(class_interval_context.callee_interval()).c_str(),
        show(callee).c_str(),
        show(context.class_intervals->get_interval(callee->get_class()))
            .c_str());

    narrowed_class_interval_context = CallClassIntervalContext(
        narrowed_interval, class_interval_context.preserves_type_context());
  }

  auto caller_class_interval =
      context.class_intervals->get_interval(caller->get_class());

  generations_.visit(
      [&model,
       callee,
       call_position,
       maximum_source_sink_distance,
       &context,
       &source_register_types,
       &source_constant_arguments,
       &narrowed_class_interval_context,
       &caller_class_interval](
          const AccessPath& callee_port, const Taint& generations) {
        model.generations_.write(
            callee_port,
            generations.propagate(
                callee,
                context.access_path_factory->get(callee_port),
                call_position,
                maximum_source_sink_distance,
                context,
                source_register_types,
                source_constant_arguments,
                narrowed_class_interval_context,
                caller_class_interval,
                model.add_features_to_arguments_),
            UpdateKind::Weak);
      });

  sinks_.visit([&model,
                callee,
                call_position,
                maximum_source_sink_distance,
                &context,
                &source_register_types,
                &source_constant_arguments,
                &narrowed_class_interval_context,
                &caller_class_interval](
                   const AccessPath& callee_port, const Taint& sinks) {
    model.sinks_.write(
        callee_port,
        sinks.propagate(
            callee,
            context.access_path_factory->get(callee_port),
            call_position,
            maximum_source_sink_distance,
            context,
            source_register_types,
            source_constant_arguments,
            narrowed_class_interval_context,
            caller_class_interval,
            model.add_features_to_arguments_),
        UpdateKind::Weak);
  });

  std::size_t max_call_chain_source_sink_distance =
      context.options->heuristics().max_call_chain_source_sink_distance();
  call_effect_sinks_.visit(
      [&model,
       callee,
       call_position,
       &context,
       &narrowed_class_interval_context,
       &caller_class_interval,
       max_call_chain_source_sink_distance](
          const AccessPath& callee_port, const Taint& call_effect) {
        switch (callee_port.root().kind()) {
          case Root::Kind::CallEffectCallChain:
          case Root::Kind::CallEffectIntent: {
            model.call_effect_sinks_.write(
                callee_port,
                call_effect.propagate(
                    callee,
                    context.access_path_factory->get(callee_port),
                    call_position,
                    max_call_chain_source_sink_distance,
                    context,
                    /* source register types */ {},
                    /* source constant arguments */ {},
                    narrowed_class_interval_context,
                    caller_class_interval,
                    model.add_features_to_arguments_),
                UpdateKind::Weak);
          } break;
          default:
            mt_unreachable();
        }
      });

  propagations_.visit(
      [&model,
       callee,
       call_position,
       maximum_source_sink_distance,
       &context,
       &source_register_types,
       &source_constant_arguments,
       &narrowed_class_interval_context,
       &caller_class_interval](
          const AccessPath& callee_port, const Taint& propagations) {
        model.propagations_.write(
            callee_port,
            propagations.propagate(
                callee,
                context.access_path_factory->get(callee_port),
                call_position,
                maximum_source_sink_distance,
                context,
                source_register_types,
                source_constant_arguments,
                narrowed_class_interval_context,
                caller_class_interval,
                model.add_features_to_arguments_),
            UpdateKind::Weak);
      });

  model.inline_as_getter_ = inline_as_getter_;
  model.inline_as_setter_ = inline_as_setter_;

  // This is bottom when the method was never analyzed.
  // Set it to top to be sound when joining models.
  if (inline_as_getter_.is_bottom()) {
    model.inline_as_getter_.set_to_top();
  }
  if (inline_as_setter_.is_bottom()) {
    model.inline_as_setter_.set_to_top();
  }

  return model;
}

Model Model::initial_model_for_iteration() const {
  Model model;
  model.method_ = method_;
  model.modes_ = modes_;
  model.global_sanitizers_ = global_sanitizers_;
  model.port_sanitizers_ = port_sanitizers_;
  model.model_generators_ = model_generators_;
  return model;
}

void Model::collapse_invalid_paths(Context& context) {
  if (!method_) {
    return;
  }

  using FieldTypesAccumulator = std::unordered_set<const DexType*>;

  auto is_valid = [this, &context](
                      const FieldTypesAccumulator& previous_field_types,
                      Path::Element path_element) {
    FieldTypesAccumulator current_field_types;

    if (path_element.is_field()) {
      for (const auto* previous_field_type : previous_field_types) {
        if (previous_field_type == type::java_lang_Object()) {
          // Object is too generic to determine the set of possible field names.
          continue;
        }

        const auto& cached_types = context.field_cache->field_types(
            previous_field_type, path_element.name());
        current_field_types.insert(cached_types.begin(), cached_types.end());
      }

      if (current_field_types.empty()) {
        LOG(5,
            "Model for method `{}` has invalid path element `{}`",
            show(method_),
            show(path_element));
        return std::make_pair(false, current_field_types);
      }
    }

    return std::make_pair(true, current_field_types);
  };

  auto initial_accumulator = [this](const Root& root) {
    // Leaf ports appear in callee ports. This only applies to caller ports.
    mt_assert(!root.is_leaf_port());

    DexType* MT_NULLABLE root_type = nullptr;
    if (root.is_argument()) {
      root_type = method_->parameter_type(root.parameter_position());
    } else if (root.is_return()) {
      root_type = method_->return_type();
    }

    if (root_type == nullptr) {
      // This can happen when there is an invalid model, e.g. model defined
      // on an argument that does not exist, usually from a model-generator.
      // Returning an empty list of types will collapse all paths.
      ERROR(
          1,
          "Could not find root type for method `{}`, root: `{}`",
          show(method_),
          show(root));
      return FieldTypesAccumulator{};
    }

    return FieldTypesAccumulator{root_type};
  };

  auto features = FeatureMayAlwaysSet{
      context.feature_factory->get_invalid_path_broadening()};

  generations_.collapse_invalid_paths<FieldTypesAccumulator>(
      is_valid, initial_accumulator, features);
  parameter_sources_.collapse_invalid_paths<FieldTypesAccumulator>(
      is_valid, initial_accumulator, features);
  sinks_.collapse_invalid_paths<FieldTypesAccumulator>(
      is_valid, initial_accumulator, features);
  propagations_.collapse_invalid_paths<FieldTypesAccumulator>(
      is_valid, initial_accumulator, features);
}

void Model::approximate(
    const FeatureMayAlwaysSet& widening_features,
    const Heuristics& heuristics) {
  const auto make_mold = [](Taint taint) { return taint.essential(); };

  generations_.shape_with(make_mold, widening_features);
  generations_.limit_leaves(
      heuristics.generation_max_output_path_leaves(), widening_features);

  parameter_sources_.shape_with(make_mold, widening_features);
  parameter_sources_.limit_leaves(
      heuristics.parameter_source_max_output_path_leaves(), widening_features);

  sinks_.shape_with(make_mold, widening_features);
  sinks_.limit_leaves(
      heuristics.sink_max_input_path_leaves(), widening_features);

  call_effect_sources_.shape_with(make_mold, widening_features);
  call_effect_sources_.limit_leaves(
      heuristics.call_effect_source_max_output_path_leaves(),
      widening_features);

  call_effect_sinks_.shape_with(make_mold, widening_features);
  call_effect_sinks_.limit_leaves(
      heuristics.call_effect_sink_max_input_path_leaves(), widening_features);

  propagations_.shape_with(make_mold, widening_features);
  propagations_.limit_leaves(
      heuristics.propagation_max_input_path_leaves(), widening_features);
}

bool Model::empty() const {
  return modes_.empty() && frozen_.empty() && generations_.is_bottom() &&
      parameter_sources_.is_bottom() && sinks_.is_bottom() &&
      call_effect_sources_.is_bottom() && call_effect_sinks_.is_bottom() &&
      propagations_.is_bottom() && global_sanitizers_.is_bottom() &&
      port_sanitizers_.is_bottom() && attach_to_sources_.is_bottom() &&
      attach_to_sinks_.is_bottom() && attach_to_propagations_.is_bottom() &&
      add_features_to_arguments_.is_bottom() &&
      add_via_value_of_features_to_arguments_.is_bottom() &&
      inline_as_getter_.is_bottom() && inline_as_setter_.is_bottom() &&
      model_generators_.is_bottom() && issues_.is_bottom();
}

void Model::add_mode(Model::Mode mode, Context& context) {
  modes_ |= mode;

  if (mode == Model::Mode::TaintInTaintOut ||
      (mode == Model::Mode::AddViaObscureFeature &&
       modes_.test(Model::Mode::TaintInTaintOut))) {
    add_taint_in_taint_out(context);
  }
  if (mode == Model::Mode::TaintInTaintThis ||
      (mode == Model::Mode::AddViaObscureFeature &&
       modes_.test(Model::Mode::TaintInTaintThis))) {
    add_taint_in_taint_this(context);
  }
}

namespace {

void add_taint_in_taint_out_propagation(
    Model& model,
    Context& context,
    const FeatureSet& user_features,
    ParameterPosition number_of_parameters) {
  for (ParameterPosition parameter_position = 0;
       parameter_position < number_of_parameters;
       parameter_position++) {
    model.add_propagation(
        PropagationConfig(
            /* input_path */ AccessPath(
                Root(Root::Kind::Argument, parameter_position)),
            /* kind */ context.kind_factory->local_return(),
            /* output_paths */
            PathTreeDomain{{Path{}, CollapseDepth::zero()}},
            /* inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
            user_features),
        context.options->heuristics());
  }
}

} // namespace

void Model::add_taint_in_taint_out(Context& context) {
  modes_ |= Model::Mode::TaintInTaintOut;

  if (!method_ || method_->returns_void()) {
    return;
  }

  auto user_features = FeatureSet::bottom();
  if (modes_.test(Model::Mode::AddViaObscureFeature)) {
    user_features.add(context.feature_factory->get("via-obscure"));
    user_features.add(
        context.feature_factory->get("via-obscure-taint-in-taint-out"));
  }

  add_taint_in_taint_out_propagation(
      *this, context, user_features, method_->number_of_parameters());
}

void Model::add_taint_in_taint_out(
    Context& context,
    const IRInstruction* instruction) {
  modes_ |= Model::Mode::TaintInTaintOut;

  // Not intended to be used for methods that have a concrete DexMethod*.
  mt_assert(method_ == nullptr);
  const auto* dex_method_reference = instruction->get_method();
  if (dex_method_reference->is_def()) {
    // If this happens, it means methods were created after call graph
    // construction. They would not have been defined in the APK, so it should
    // not impact analysis results, but this is non-ideal behavior. Methods
    // should be created by the time call-graph construction takes place.
    WARNING(
        3,
        "Model::add_taint_in_taint_out called with def DexMethodReference: `{}`. Current model: `{}`",
        show(dex_method_reference),
        *this);
    method_ = context.methods->get(dex_method_reference->as_def());
    mt_assert_log(method_ != nullptr, "Expected method to have been created.");
    add_taint_in_taint_out(context);
    return;
  }

  if (dex_method_reference->get_proto()->get_rtype() == type::_void()) {
    return;
  }

  auto user_features =
      FeatureSet{context.feature_factory->get_missing_method()};
  if (modes_.test(Model::Mode::AddViaObscureFeature)) {
    user_features.add(context.feature_factory->get("via-obscure"));
  }

  auto number_of_parameters =
      dex_method_reference->get_proto()->get_args()->size();
  if (!opcode::is_invoke_static(instruction->opcode())) {
    ++number_of_parameters; // add 1 for "this" argument
  }
  // The number of registers given to the invoke instruction is the number of
  // parameters. If assertion fails, the computation for number of parameters
  // above is wrong which means we might be seeing invoke static on non-static
  // methods or vice versa.
  mt_assert(number_of_parameters == instruction->srcs_size());
  add_taint_in_taint_out_propagation(
      *this, context, user_features, number_of_parameters);
}

void Model::add_taint_in_taint_this(Context& context) {
  modes_ |= Model::Mode::TaintInTaintThis;

  if (!method_ || method_->is_static()) {
    return;
  }

  auto user_features = FeatureSet::bottom();
  if (modes_.test(Model::Mode::AddViaObscureFeature)) {
    user_features.add(context.feature_factory->get("via-obscure"));
  }

  for (ParameterPosition parameter_position = 1;
       parameter_position < method_->number_of_parameters();
       parameter_position++) {
    add_propagation(
        PropagationConfig(
            /* input_path */ AccessPath(
                Root(Root::Kind::Argument, parameter_position)),
            /* kind */ context.kind_factory->local_receiver(),
            /* output_paths */
            PathTreeDomain{{Path{}, CollapseDepth::zero()}},
            /* inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
            user_features),
        context.options->heuristics());
  }
}

void Model::add_generation(
    const AccessPath& port,
    TaintConfig source,
    const Heuristics& heuristics) {
  if (!check_taint_config_consistency(source, "source")) {
    return;
  }
  add_generation(port, Taint{std::move(source)}, heuristics);
}

void Model::add_inferred_generations(
    AccessPath port,
    Taint generations,
    const FeatureMayAlwaysSet& widening_features,
    const Heuristics& heuristics) {
  auto sanitized_generations = apply_source_sink_sanitizers(
      SanitizerKind::Sources, std::move(generations), port.root());
  if (!sanitized_generations.is_bottom()) {
    update_taint_tree(
        generations_,
        std::move(port),
        heuristics.generation_max_port_size(),
        std::move(sanitized_generations),
        widening_features);
  }
}

void Model::add_parameter_source(
    const AccessPath& port,
    TaintConfig source,
    const Heuristics& heuristics) {
  if (!check_taint_config_consistency(source, "source")) {
    return;
  }

  add_parameter_source(port, Taint{std::move(source)}, heuristics);
}

void Model::add_sink(
    const AccessPath& port,
    TaintConfig sink,
    const Heuristics& heuristics) {
  if (!check_taint_config_consistency(sink, "sink")) {
    return;
  }

  add_sink(port, Taint{std::move(sink)}, heuristics);
}

void Model::add_inferred_sinks(
    AccessPath port,
    Taint sinks,
    const FeatureMayAlwaysSet& widening_features,
    const Heuristics& heuristics) {
  auto sanitized_sinks = apply_source_sink_sanitizers(
      SanitizerKind::Sinks, std::move(sinks), port.root());
  if (!sanitized_sinks.is_bottom()) {
    update_taint_tree(
        sinks_,
        std::move(port),
        heuristics.call_effect_sink_max_port_size(),
        std::move(sanitized_sinks),
        widening_features);
  }
}

void Model::add_call_effect_source(
    AccessPath port,
    TaintConfig source,
    const Heuristics& heuristics) {
  if (!check_taint_config_consistency(source, "effect source")) {
    return;
  }

  add_call_effect_source(port, Taint{std::move(source)}, heuristics);
}

void Model::add_call_effect_sink(
    AccessPath port,
    TaintConfig sink,
    const Heuristics& heuristics) {
  if (!check_taint_config_consistency(sink, "effect sink")) {
    return;
  }

  add_call_effect_sink(port, Taint{std::move(sink)}, heuristics);
}

void Model::add_inferred_call_effect_sinks(
    AccessPath port,
    Taint sinks,
    const Heuristics& heuristics) {
  add_call_effect_sink(port, sinks, heuristics);
}

void Model::add_inferred_call_effect_sinks(
    AccessPath port,
    Taint sinks,
    const FeatureMayAlwaysSet& widening_features,
    const Heuristics& heuristics) {
  auto sanitized_sinks = apply_source_sink_sanitizers(
      SanitizerKind::Sinks, std::move(sinks), port.root());
  if (!sanitized_sinks.is_bottom()) {
    update_taint_tree(
        call_effect_sinks_,
        std::move(port),
        heuristics.call_effect_sink_max_port_size(),
        std::move(sanitized_sinks),
        widening_features);
  }
}

void Model::add_propagation(
    PropagationConfig propagation,
    const Heuristics& heuristics) {
  if (!check_port_consistency(propagation.input_path()) ||
      !check_root_consistency(propagation.propagation_kind()->root())) {
    return;
  }

  add_propagation(
      propagation.input_path(), Taint::propagation(propagation), heuristics);
}

void Model::add_inferred_propagations(
    AccessPath input_path,
    Taint local_taint,
    const FeatureMayAlwaysSet& widening_features,
    const Heuristics& heuristics) {
  if (has_global_propagation_sanitizer() ||
      !port_sanitizers_.get(input_path.root()).is_bottom()) {
    return;
  }

  update_taint_tree(
      propagations_,
      std::move(input_path),
      heuristics.propagation_max_input_path_size(),
      std::move(local_taint),
      widening_features);
}

void Model::add_global_sanitizer(Sanitizer sanitizer) {
  global_sanitizers_.add(sanitizer);
}

void Model::add_port_sanitizers(SanitizerSet sanitizers, Root root) {
  if (!check_root_consistency(root)) {
    return;
  }

  port_sanitizers_.update(root, [&sanitizers](const SanitizerSet& set) {
    return set.join(sanitizers);
  });
}

Taint Model::apply_source_sink_sanitizers(
    SanitizerKind kind,
    Taint taint,
    Root root) {
  mt_assert(kind != SanitizerKind::Propagations);

  std::unordered_set<const Kind*> sanitized_kinds;
  for (const auto& sanitizer_set :
       {global_sanitizers_, port_sanitizers_.get(root)}) {
    for (const auto& sanitizer : sanitizer_set) {
      if (sanitizer.sanitizer_kind() == kind) {
        const auto& kinds = sanitizer.kinds();
        if (kinds.is_top()) {
          return Taint::bottom();
        }
        sanitized_kinds.insert(
            kinds.elements().begin(), kinds.elements().end());
      }
    }
  }

  if (!sanitized_kinds.empty()) {
    taint.filter_frames([&sanitized_kinds](const Frame& frame) {
      return sanitized_kinds.find(frame.kind()) == sanitized_kinds.end();
    });
  }

  return taint;
}

bool Model::has_global_propagation_sanitizer() const {
  return global_sanitizers_.contains(
      Sanitizer(SanitizerKind::Propagations, KindSetAbstractDomain::top()));
}

void Model::add_attach_to_sources(Root root, FeatureSet features) {
  if (!check_root_consistency(root)) {
    return;
  }

  attach_to_sources_.update(
      root, [&features](const FeatureSet& set) { return set.join(features); });
}

FeatureSet Model::attach_to_sources(Root root) const {
  return attach_to_sources_.get(root);
}

void Model::add_attach_to_sinks(Root root, FeatureSet features) {
  if (!check_root_consistency(root)) {
    return;
  }

  attach_to_sinks_.update(
      root, [&features](const FeatureSet& set) { return set.join(features); });
}

FeatureSet Model::attach_to_sinks(Root root) const {
  return attach_to_sinks_.get(root);
}

void Model::add_attach_to_propagations(Root root, FeatureSet features) {
  if (!check_root_consistency(root)) {
    return;
  }

  attach_to_propagations_.update(
      root, [&features](const FeatureSet& set) { return set.join(features); });
}

FeatureSet Model::attach_to_propagations(Root root) const {
  return attach_to_propagations_.get(root);
}

void Model::add_add_features_to_arguments(Root root, FeatureSet features) {
  if (!check_root_consistency(root)) {
    return;
  }

  add_features_to_arguments_.update(
      root, [&features](const FeatureSet& set) { return set.join(features); });
}

void Model::add_add_via_value_of_features_to_arguments(
    Root root,
    TaggedRootSet via_value_of_ports) {
  if (!check_root_consistency(root)) {
    return;
  }

  add_via_value_of_features_to_arguments_.update(
      root, [&via_value_of_ports](const TaggedRootSet& set) {
        return set.join(via_value_of_ports);
      });
}

bool Model::has_add_features_to_arguments() const {
  return !add_features_to_arguments_.is_bottom();
}

FeatureSet Model::add_features_to_arguments(Root root) const {
  return add_features_to_arguments_.get(root);
}

const AccessPathConstantDomain& Model::inline_as_getter() const {
  return inline_as_getter_;
}

void Model::set_inline_as_getter(AccessPathConstantDomain inline_as_getter) {
  if (!check_inline_as_getter_consistency(inline_as_getter)) {
    return;
  }

  inline_as_getter_ = std::move(inline_as_getter);
}

void Model::add_model_generator(const ModelGeneratorName* model_generator) {
  model_generators_.add(model_generator);
}

void Model::add_model_generator_if_empty(
    const ModelGeneratorName* model_generator) {
  if (!model_generators_.is_bottom()) {
    return;
  }

  model_generators_.add(model_generator);
}

void Model::make_sharded_model_generators(const std::string& identifier) {
  ModelGeneratorNameSet new_model_generator_names;
  for (const auto* model_generator : model_generators_) {
    new_model_generator_names.add(
        ModelGeneratorNameFactory::singleton().create_sharded(
            identifier, model_generator));
  }

  if (new_model_generator_names.empty()) {
    // If there are no existing model generators, add an empty "sharded" one
    // to indicate that the entire model came from a "sharded" source.
    new_model_generator_names.add(
        ModelGeneratorNameFactory::singleton().create_sharded(
            identifier,
            /* original_generator */ nullptr));
  }

  model_generators_ = std::move(new_model_generator_names);
}

const SetterAccessPathConstantDomain& Model::inline_as_setter() const {
  return inline_as_setter_;
}

void Model::set_inline_as_setter(
    SetterAccessPathConstantDomain inline_as_setter) {
  if (!check_inline_as_setter_consistency(inline_as_setter)) {
    return;
  }

  inline_as_setter_ = std::move(inline_as_setter);
}

void Model::add_issue(Issue trace) {
  issues_.add(std::move(trace));
}

bool Model::skip_analysis() const {
  return modes_.test(Model::Mode::SkipAnalysis);
}

bool Model::add_via_obscure_feature() const {
  return modes_.test(Model::Mode::AddViaObscureFeature);
}

bool Model::is_taint_in_taint_out() const {
  return modes_.test(Model::Mode::TaintInTaintOut);
}

bool Model::is_taint_in_taint_this() const {
  return modes_.test(Model::Mode::TaintInTaintThis);
}

bool Model::no_join_virtual_overrides() const {
  return modes_.test(Model::Mode::NoJoinVirtualOverrides);
}

bool Model::no_collapse_on_propagation() const {
  return modes_.test(Model::Mode::NoCollapseOnPropagation);
}

bool Model::alias_memory_location_on_invoke() const {
  return modes_.test(Model::Mode::AliasMemoryLocationOnInvoke);
}

bool Model::strong_write_on_propagation() const {
  return modes_.test(Model::Mode::StrongWriteOnPropagation);
}

bool Model::is_frozen(Model::FreezeKind freeze_kind) const {
  return frozen_.test(freeze_kind);
}

void Model::freeze(Model::FreezeKind freeze_kind) {
  frozen_.set(freeze_kind);
}

bool Model::leq(const Model& other) const {
  return modes_.is_subset_of(other.modes_) &&
      frozen_.is_subset_of(other.frozen_) &&
      leq_frozen(
             generations_,
             other.generations_,
             is_frozen(Model::FreezeKind::Generations),
             other.is_frozen(Model::FreezeKind::Generations)) &&
      leq_frozen(
             parameter_sources_,
             other.parameter_sources_,
             is_frozen(Model::FreezeKind::ParameterSources),
             other.is_frozen(Model::FreezeKind::ParameterSources)) &&
      leq_frozen(
             sinks_,
             other.sinks_,
             is_frozen(Model::FreezeKind::Sinks),
             other.is_frozen(Model::FreezeKind::Sinks)) &&
      leq_frozen(
             propagations_,
             other.propagations_,
             is_frozen(Model::FreezeKind::Propagations),
             other.is_frozen(Model::FreezeKind::Propagations)) &&
      call_effect_sources_.leq(other.call_effect_sources_) &&
      call_effect_sinks_.leq(other.call_effect_sinks_) &&
      global_sanitizers_.leq(other.global_sanitizers_) &&
      port_sanitizers_.leq(other.port_sanitizers_) &&
      attach_to_sources_.leq(other.attach_to_sources_) &&
      attach_to_sinks_.leq(other.attach_to_sinks_) &&
      attach_to_propagations_.leq(other.attach_to_propagations_) &&
      add_features_to_arguments_.leq(other.add_features_to_arguments_) &&
      add_via_value_of_features_to_arguments_.leq(
          other.add_via_value_of_features_to_arguments_) &&
      inline_as_getter_.leq(other.inline_as_getter_) &&
      inline_as_setter_.leq(other.inline_as_setter_) &&
      model_generators_.leq(other.model_generators_) &&
      issues_.leq(other.issues_);
}

void Model::join_with(const Model& other) {
  if (this == &other) {
    return;
  }

  mt_if_expensive_assert(auto previous = *this);

  join_with_frozen(
      generations_,
      other.generations_,
      is_frozen(Model::FreezeKind::Generations),
      other.is_frozen(Model::FreezeKind::Generations));
  join_with_frozen(
      parameter_sources_,
      other.parameter_sources_,
      is_frozen(Model::FreezeKind::ParameterSources),
      other.is_frozen(Model::FreezeKind::ParameterSources));
  join_with_frozen(
      sinks_,
      other.sinks_,
      is_frozen(Model::FreezeKind::Sinks),
      other.is_frozen(Model::FreezeKind::Sinks));
  join_with_frozen(
      propagations_,
      other.propagations_,
      is_frozen(Model::FreezeKind::Propagations),
      other.is_frozen(Model::FreezeKind::Propagations));

  modes_ |= other.modes_;
  frozen_ |= other.frozen_;
  call_effect_sources_.join_with(other.call_effect_sources_);
  call_effect_sinks_.join_with(other.call_effect_sinks_);
  global_sanitizers_.join_with(other.global_sanitizers_);
  port_sanitizers_.join_with(other.port_sanitizers_);
  attach_to_sources_.join_with(other.attach_to_sources_);
  attach_to_sinks_.join_with(other.attach_to_sinks_);
  attach_to_propagations_.join_with(other.attach_to_propagations_);
  add_features_to_arguments_.join_with(other.add_features_to_arguments_);
  add_via_value_of_features_to_arguments_.join_with(
      other.add_via_value_of_features_to_arguments_);
  inline_as_getter_.join_with(other.inline_as_getter_);
  inline_as_setter_.join_with(other.inline_as_setter_);
  model_generators_.join_with(other.model_generators_);
  issues_.join_with(other.issues_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

namespace {

void kinds_from_taint_tree(
    const TaintAccessPathTree& taint_tree,
    std::unordered_set<const Kind*>& result) {
  for (const auto& [_port, taint] : taint_tree.elements()) {
    auto taint_kinds = taint.kinds();
    result.insert(taint_kinds.begin(), taint_kinds.end());
  }
}

} // namespace

std::unordered_set<const Kind*> Model::source_kinds() const {
  std::unordered_set<const Kind*> result;

  kinds_from_taint_tree(parameter_sources_, result);
  kinds_from_taint_tree(generations_, result);
  kinds_from_taint_tree(call_effect_sources_, result);

  return result;
}

std::unordered_set<const Kind*> Model::sink_kinds() const {
  std::unordered_set<const Kind*> result;

  kinds_from_taint_tree(sinks_, result);
  kinds_from_taint_tree(call_effect_sinks_, result);

  return result;
}

std::unordered_set<const Transform*> Model::local_transform_kinds() const {
  std::unordered_set<const Transform*> result;

  std::unordered_set<const Kind*> propagation_kinds;
  kinds_from_taint_tree(propagations_, propagation_kinds);
  for (const Kind* propagation_kind : propagation_kinds) {
    const auto* transform_kind = propagation_kind->as<TransformKind>();
    if (transform_kind == nullptr) {
      continue;
    }

    const auto* transforms = transform_kind->local_transforms();
    if (transforms == nullptr) {
      continue;
    }

    for (const auto* transform : *transforms) {
      result.insert(transform);
    }
  }

  return result;
}

Model Model::from_config_json(
    const Method* method,
    const Json::Value& value,
    Context& context,
    bool check_unexpected_members) {
  JsonValidation::validate_object(value);
  if (check_unexpected_members) {
    JsonValidation::check_unexpected_members(
        value,
        {"method", // Only when called from `Registry`.
         "model_generators", // Only when parsing previously emitted models.
         "position", // Only when parsing previously emitted models.
         "modes",
         "freeze",
         "generations",
         "parameter_sources",
         "sources",
         "sinks",
         "effect_sources",
         "effect_sinks",
         "propagation",
         "sanitizers",
         "attach_to_sources",
         "attach_to_sinks",
         "attach_to_propagations",
         "add_features_to_arguments",
         "inline_as_getter",
         "inline_as_setter",
         "issues"});
  }

  Modes modes;
  for (auto mode_value :
       JsonValidation::null_or_array(value, /* field */ "modes")) {
    auto mode = string_to_model_mode(JsonValidation::string(mode_value));
    if (!mode) {
      throw JsonValidationError(value, /* field */ "modes", "valid mode");
    }
    modes.set(*mode, true);
  }

  Frozen frozen;
  for (auto freeze :
       JsonValidation::null_or_array(value, /* field */ "freeze")) {
    auto freeze_kind = string_to_freeze_kind(JsonValidation::string(freeze));
    if (!freeze_kind) {
      throw JsonValidationError(
          value, /* field */ "freeze", "valid freeze kind");
    }
    frozen.set(*freeze_kind, true);
  }

  Model model(method, context, modes, frozen);

  std::vector<std::pair<AccessPath, Json::Value>> generation_values;
  std::vector<std::pair<AccessPath, Json::Value>> parameter_source_values;
  std::vector<std::pair<AccessPath, Json::Value>> sink_values;
  read_taint_configs_from_json(
      value,
      generation_values,
      parameter_source_values,
      sink_values,
      TaintConfigTemplate::is_concrete);

  for (const auto& [port, generation_value] : generation_values) {
    model.add_generation(
        port,
        TaintConfig::from_json(generation_value, context),
        context.options->heuristics());
  }

  for (const auto& [port, parameter_source_value] : parameter_source_values) {
    model.add_parameter_source(
        port,
        TaintConfig::from_json(parameter_source_value, context),
        context.options->heuristics());
  }

  for (const auto& [port, sink_value] : sink_values) {
    model.add_sink(
        port,
        TaintConfig::from_json(sink_value, context),
        context.options->heuristics());
  }

  for (auto effect_source_value :
       JsonValidation::null_or_array(value, /* field */ "effect_sources")) {
    std::string effect_type_port =
        effect_source_value.isMember("port") ? "port" : "type";
    JsonValidation::string(effect_source_value, /* field */ effect_type_port);
    auto port = AccessPath::from_json(effect_source_value[effect_type_port]);
    model.add_call_effect_source(
        port,
        TaintConfig::from_json(effect_source_value, context),
        context.options->heuristics());
  }

  for (auto effect_sink_value :
       JsonValidation::null_or_array(value, /* field */ "effect_sinks")) {
    std::string effect_type_port =
        effect_sink_value.isMember("port") ? "port" : "type";
    JsonValidation::string(effect_sink_value, /* field */ effect_type_port);
    auto port = AccessPath::from_json(effect_sink_value[effect_type_port]);
    model.add_call_effect_sink(
        port,
        TaintConfig::from_json(effect_sink_value, context),
        context.options->heuristics());
  }

  for (auto propagation_value :
       JsonValidation::null_or_array(value, /* field */ "propagation")) {
    model.add_propagation(
        PropagationConfig::from_json(propagation_value, context),
        context.options->heuristics());
  }

  for (auto sanitizer_value :
       JsonValidation::null_or_array(value, /* field */ "sanitizers")) {
    auto sanitizer = Sanitizer::from_json(sanitizer_value, context);
    if (!sanitizer_value.isMember("port")) {
      model.add_global_sanitizer(sanitizer);
    } else {
      auto root = Root::from_json(sanitizer_value["port"]);
      model.add_port_sanitizers(SanitizerSet(sanitizer), root);
    }
  }

  for (auto attach_to_sources_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sources")) {
    JsonValidation::check_unexpected_members(
        attach_to_sources_value, {"port", "features"});
    JsonValidation::string(attach_to_sources_value, /* field */ "port");
    auto root = Root::from_json(attach_to_sources_value["port"]);
    JsonValidation::null_or_array(
        attach_to_sources_value, /* field */ "features");
    auto features =
        FeatureSet::from_json(attach_to_sources_value["features"], context);
    model.add_attach_to_sources(root, features);
  }

  for (auto attach_to_sinks_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sinks")) {
    JsonValidation::check_unexpected_members(
        attach_to_sinks_value, {"port", "features"});
    JsonValidation::string(attach_to_sinks_value, /* field */ "port");
    auto root = Root::from_json(attach_to_sinks_value["port"]);
    JsonValidation::null_or_array(
        attach_to_sinks_value, /* field */ "features");
    auto features =
        FeatureSet::from_json(attach_to_sinks_value["features"], context);
    model.add_attach_to_sinks(root, features);
  }

  for (auto attach_to_propagations_value : JsonValidation::null_or_array(
           value, /* field */ "attach_to_propagations")) {
    JsonValidation::check_unexpected_members(
        attach_to_propagations_value, {"port", "features"});
    JsonValidation::string(attach_to_propagations_value, /* field */ "port");
    auto root = Root::from_json(attach_to_propagations_value["port"]);
    JsonValidation::null_or_array(
        attach_to_propagations_value, /* field */ "features");
    auto features = FeatureSet::from_json(
        attach_to_propagations_value["features"], context);
    model.add_attach_to_propagations(root, features);
  }

  for (auto add_features_to_arguments_value : JsonValidation::null_or_array(
           value, /* field */ "add_features_to_arguments")) {
    JsonValidation::check_unexpected_members(
        add_features_to_arguments_value, {"port", "features", "via_value_of"});
    JsonValidation::string(add_features_to_arguments_value, /* field */ "port");
    auto root = Root::from_json(add_features_to_arguments_value["port"]);
    if (add_features_to_arguments_value.isMember("features")) {
      auto features = FeatureSet::from_json(
          JsonValidation::nonempty_array(
              add_features_to_arguments_value, /* field */ "features"),
          context);
      model.add_add_features_to_arguments(root, features);
    }

    if (add_features_to_arguments_value.isMember("via_value_of")) {
      TaggedRootSet via_value_of_ports;
      for (const auto& tagged_root : JsonValidation::nonempty_array(
               add_features_to_arguments_value, "via_value_of")) {
        via_value_of_ports.add(TaggedRoot::from_json(tagged_root));
      }
      model.add_add_via_value_of_features_to_arguments(
          root, std::move(via_value_of_ports));
    }
  }

  if (value.isMember("inline_as_getter")) {
    JsonValidation::string(value, /* field */ "inline_as_getter");
    model.set_inline_as_getter(AccessPathConstantDomain(
        AccessPath::from_json(value["inline_as_getter"])));
  }

  if (value.isMember("inline_as_setter")) {
    model.set_inline_as_setter(
        SetterAccessPathConstantDomain(SetterAccessPath::from_json(
            JsonValidation::object(value, "inline_as_setter"))));
  }

  // We cannot parse issues for now.
  if (value.isMember("issues")) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "model without issues");
  }

  return model;
}

/**
 * Helper to read a category of taint config values from a given JSON \p value.
 * This is effectively an extract of the common logic shared by all steps of
 * \p Model::read_taint_configs_from_json in order to simplify it.
 *
 * @param output Output container to which to append the extracted taint config
 * values.
 * @param value JSON config to parse.
 * @param field Field in \p value to parse.
 * @param taint_config_filter Only taint configs in \p field matching this
 * filter will be processed.
 * @param root_filter Only taint configs relating to roots matching this filter
 * will be processed.
 * @param allow_default_port if \p true, we assume \p Root::Kind::Return as
 * default port if none is configured.
 */
static void read_taint_configs(
    std::vector<std::pair<AccessPath, Json::Value>>& output,
    const Json::Value& value,
    const std::string& field,
    bool (*taint_config_filter)(const Json::Value& value),
    bool (*root_filter)(const Root&),
    const bool allow_default_port = false) {
  for (auto config_value : JsonValidation::null_or_array(value, field) |
           boost::adaptors::filtered(taint_config_filter)) {
    auto port = AccessPath(Root(Root::Kind::Return));
    if (config_value.isMember("port")) {
      port = AccessPath::from_json(config_value["port"]);
    } else if (config_value.isMember("caller_port") || !allow_default_port) {
      port = AccessPath::from_json(config_value["caller_port"]);
    }

    if (root_filter(port.root())) {
      output.emplace_back(std::move(port), std::move(config_value));
    }
  }
}

/** Filter matching every \p Root. */
static bool all_roots(const Root&) {
  return true;
}

/** Filter matching only argument roots. */
static bool argument_roots_only(const Root& root) {
  return root.is_argument();
}

/** Filter matching only non-argument roots. */
static bool no_argument_roots(const Root& root) {
  return !root.is_argument();
}

void Model::read_taint_configs_from_json(
    const Json::Value& value,
    std::vector<std::pair<AccessPath, Json::Value>>& generation_values,
    std::vector<std::pair<AccessPath, Json::Value>>& parameter_source_values,
    std::vector<std::pair<AccessPath, Json::Value>>& sink_values,
    bool (*predicate)(const Json::Value& value)) {
  read_taint_configs(
      generation_values, value, "generations", predicate, all_roots, true);
  read_taint_configs(
      parameter_source_values,
      value,
      "parameter_sources",
      predicate,
      all_roots,
      false);
  read_taint_configs(
      parameter_source_values,
      value,
      "sources",
      predicate,
      argument_roots_only,
      true);
  read_taint_configs(
      generation_values, value, "sources", predicate, no_argument_roots, true);
  read_taint_configs(sink_values, value, "sinks", predicate, all_roots, false);
}

Model Model::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);

  const Method* MT_NULLABLE method = nullptr;
  if (value.isMember("method")) {
    method = Method::from_json(value["method"], context);
  }

  Modes modes;
  for (const auto& mode_value :
       JsonValidation::null_or_array(value, /* field */ "modes")) {
    auto mode = string_to_model_mode(JsonValidation::string(mode_value));
    if (!mode) {
      throw JsonValidationError(value, /* field */ "modes", "valid mode");
    }
    modes.set(*mode, true);
  }

  Frozen frozen;
  for (const auto& freeze :
       JsonValidation::null_or_array(value, /* field */ "freeze")) {
    auto freeze_kind = string_to_freeze_kind(JsonValidation::string(freeze));
    if (!freeze_kind) {
      throw JsonValidationError(
          value, /* field */ "freeze", "valid freeze kind");
    }
    frozen.set(*freeze_kind, true);
  }

  Model model(method, context, modes, frozen);

  for (const auto& generation_value :
       JsonValidation::null_or_array(value, /* field */ "generations")) {
    auto port = AccessPath::from_json(generation_value["port"]);
    auto taint = Taint::from_json(generation_value["taint"], context);
    model.add_generation(port, taint, context.options->heuristics());
  }

  for (const auto& parameter_source_value :
       JsonValidation::null_or_array(value, /* field */ "parameter_sources")) {
    auto port = AccessPath::from_json(parameter_source_value["port"]);
    auto taint = Taint::from_json(parameter_source_value["taint"], context);
    model.add_parameter_source(port, taint, context.options->heuristics());
  }

  for (const auto& call_effect_source_value :
       JsonValidation::null_or_array(value, /* field */ "effect_sources")) {
    auto port = AccessPath::from_json(call_effect_source_value["port"]);
    auto taint = Taint::from_json(call_effect_source_value["taint"], context);
    model.add_call_effect_source(port, taint, context.options->heuristics());
  }

  for (const auto& sink_value :
       JsonValidation::null_or_array(value, /* field */ "sinks")) {
    auto port = AccessPath::from_json(sink_value["port"]);
    auto taint = Taint::from_json(sink_value["taint"], context);
    model.add_sink(port, taint, context.options->heuristics());
  }

  for (const auto& call_effect_sink_value :
       JsonValidation::null_or_array(value, /* field */ "effect_sinks")) {
    auto port = AccessPath::from_json(call_effect_sink_value["port"]);
    auto taint = Taint::from_json(call_effect_sink_value["taint"], context);
    model.add_call_effect_sink(port, taint, context.options->heuristics());
  }

  for (const auto& propagation_value :
       JsonValidation::null_or_array(value, /* field */ "propagation")) {
    auto input_path = AccessPath::from_json(propagation_value["input"]);
    auto output_taint = Taint::from_json(propagation_value["output"], context);
    model.add_propagation(
        input_path, output_taint, context.options->heuristics());
  }

  for (const auto& sanitizer_value :
       JsonValidation::null_or_array(value, /* field */ "sanitizers")) {
    JsonValidation::validate_object(sanitizer_value);
    auto sanitizer = Sanitizer::from_json(sanitizer_value, context);
    if (sanitizer_value.isMember("port")) {
      // This is a port sanitizer.
      auto port = Root::from_json(sanitizer_value["port"]);
      model.add_port_sanitizers(SanitizerSet{sanitizer}, port);
    } else {
      model.add_global_sanitizer(sanitizer);
    }
  }

  for (const auto& attach_to_source_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sources")) {
    JsonValidation::validate_object(attach_to_source_value);
    auto port = Root::from_json(attach_to_source_value["port"]);
    auto features =
        FeatureSet::from_json(attach_to_source_value["features"], context);
    model.add_attach_to_sources(port, features);
  }

  for (const auto& attach_to_sink_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sinks")) {
    JsonValidation::validate_object(attach_to_sink_value);
    auto port = Root::from_json(attach_to_sink_value["port"]);
    auto features =
        FeatureSet::from_json(attach_to_sink_value["features"], context);
    model.add_attach_to_sinks(port, features);
  }

  for (const auto& attach_to_propagation_value : JsonValidation::null_or_array(
           value, /* field */ "attach_to_propagations")) {
    JsonValidation::validate_object(attach_to_propagation_value);
    auto port = Root::from_json(attach_to_propagation_value["port"]);
    auto features =
        FeatureSet::from_json(attach_to_propagation_value["features"], context);
    model.add_attach_to_propagations(port, features);
  }

  for (const auto& add_features_to_argument_value :
       JsonValidation::null_or_array(
           value, /* field */ "add_features_to_arguments")) {
    JsonValidation::validate_object(add_features_to_argument_value);
    auto port = Root::from_json(add_features_to_argument_value["port"]);
    if (add_features_to_argument_value.isMember("via_value_of")) {
      TaggedRootSet via_value_of_ports;
      for (const auto& add_feature_value :
           JsonValidation::null_or_array(value, /* field */ "via_value_of")) {
        via_value_of_ports.add(TaggedRoot::from_json(add_feature_value));
      }
      model.add_add_via_value_of_features_to_arguments(
          port, via_value_of_ports);
    } else {
      auto features = FeatureSet::from_json(
          add_features_to_argument_value["features"], context);
      model.add_add_features_to_arguments(port, features);
    }
  }

  if (value.isMember("inline_as_getter")) {
    model.set_inline_as_getter(AccessPathConstantDomain(
        AccessPath::from_json(value["inline_as_getter"])));
  }

  if (value.isMember("inline_as_setter")) {
    model.set_inline_as_setter(SetterAccessPathConstantDomain(
        SetterAccessPath::from_json(value["inline_as_setter"])));
  }

  for (const auto& model_generator_value :
       JsonValidation::null_or_array(value, /* field */ "model_generators")) {
    model.add_model_generator(
        ModelGeneratorName::from_json(model_generator_value, context));
  }

  // Intentionally skip parsing "issues". Model::from_json is used to read
  // cached data from another (dependent) analysis. If those issues are of
  // interest, they should be processed during that analysis instead of in
  // every subsequent analysis that depends on it.

  return model;
}

Json::Value Model::to_json(ExportOriginsMode export_origins_mode) const {
  auto value = Json::Value(Json::objectValue);

  if (method_) {
    value["method"] = method_->to_json();
  }

  if (modes_) {
    auto modes = Json::Value(Json::arrayValue);
    for (auto mode : k_all_modes) {
      if (modes_.test(mode)) {
        modes.append(Json::Value(model_mode_to_string(mode)));
      }
    }
    value["modes"] = modes;
  }

  if (frozen_) {
    auto freeze = Json::Value(Json::arrayValue);
    for (auto freeze_kind : k_all_freeze_kinds) {
      if (frozen_.test(freeze_kind)) {
        freeze.append(Json::Value(model_freeze_kind_to_string(freeze_kind)));
      }
    }
    value["freeze"] = freeze;
  }

  if (!generations_.is_bottom()) {
    auto generations_value = Json::Value(Json::arrayValue);
    for (const auto& [port, generation_taint] : generations_.elements()) {
      auto generation_value = Json::Value(Json::objectValue);
      generation_value["port"] = port.to_json();
      generation_value["taint"] = generation_taint.to_json(export_origins_mode);
      generations_value.append(generation_value);
    }
    value["generations"] = generations_value;
  }

  if (!parameter_sources_.is_bottom()) {
    auto parameter_sources_value = Json::Value(Json::arrayValue);
    for (const auto& [port, parameter_source_taint] :
         parameter_sources_.elements()) {
      auto parameter_source_value = Json::Value(Json::objectValue);
      parameter_source_value["port"] = port.to_json();
      parameter_source_value["taint"] =
          parameter_source_taint.to_json(export_origins_mode);
      parameter_sources_value.append(parameter_source_value);
    }
    value["parameter_sources"] = parameter_sources_value;
  }

  if (!call_effect_sources_.is_bottom()) {
    auto effect_sources_value = Json::Value(Json::arrayValue);
    for (const auto& [port, source_taint] : call_effect_sources_.elements()) {
      auto source_value = Json::Value(Json::objectValue);
      source_value["port"] = port.to_json();
      source_value["taint"] = source_taint.to_json(export_origins_mode);
      effect_sources_value.append(source_value);
    }
    value["effect_sources"] = effect_sources_value;
  }

  if (!sinks_.is_bottom()) {
    auto sinks_value = Json::Value(Json::arrayValue);
    for (const auto& [port, sink_taint] : sinks_.elements()) {
      auto sink_value = Json::Value(Json::objectValue);
      sink_value["port"] = port.to_json();
      sink_value["taint"] = sink_taint.to_json(export_origins_mode);
      sinks_value.append(sink_value);
    }
    value["sinks"] = sinks_value;
  }

  if (!call_effect_sinks_.is_bottom()) {
    auto effect_sinks_value = Json::Value(Json::arrayValue);
    for (const auto& [port, sink_taint] : call_effect_sinks_.elements()) {
      auto sink_value = Json::Value(Json::objectValue);
      sink_value["port"] = port.to_json();
      sink_value["taint"] = sink_taint.to_json(export_origins_mode);
      effect_sinks_value.append(sink_value);
    }
    value["effect_sinks"] = effect_sinks_value;
  }

  if (!propagations_.is_bottom()) {
    auto propagations_value = Json::Value(Json::arrayValue);
    for (const auto& [input_path, propagation_taint] :
         propagations_.elements()) {
      auto propagation_value = Json::Value(Json::objectValue);
      propagation_value["input"] = input_path.to_json();
      propagation_value["output"] =
          propagation_taint.to_json(export_origins_mode);
      propagations_value.append(propagation_value);
    }
    value["propagation"] = propagations_value;
  }

  auto sanitizers_value = Json::Value(Json::arrayValue);
  for (const auto& sanitizer : global_sanitizers_) {
    if (!sanitizer.is_bottom()) {
      sanitizers_value.append(sanitizer.to_json());
    }
  }
  for (const auto& [root, sanitizers] : port_sanitizers_) {
    auto root_value = root.to_json();
    for (const auto& sanitizer : sanitizers) {
      if (!sanitizer.is_bottom()) {
        auto sanitizer_value = sanitizer.to_json();
        sanitizer_value["port"] = root_value;
        sanitizers_value.append(sanitizer_value);
      }
    }
  }
  if (!sanitizers_value.empty()) {
    value["sanitizers"] = sanitizers_value;
  }

  if (!attach_to_sources_.is_bottom()) {
    auto attach_to_sources_value = Json::Value(Json::arrayValue);
    for (const auto& [root, features] : attach_to_sources_) {
      auto attach_to_sources_root_value = Json::Value(Json::objectValue);
      attach_to_sources_root_value["port"] = root.to_json();
      attach_to_sources_root_value["features"] = features.to_json();
      attach_to_sources_value.append(attach_to_sources_root_value);
    }
    value["attach_to_sources"] = attach_to_sources_value;
  }

  if (!attach_to_sinks_.is_bottom()) {
    auto attach_to_sinks_value = Json::Value(Json::arrayValue);
    for (const auto& [root, features] : attach_to_sinks_) {
      auto attach_to_sinks_root_value = Json::Value(Json::objectValue);
      attach_to_sinks_root_value["port"] = root.to_json();
      attach_to_sinks_root_value["features"] = features.to_json();
      attach_to_sinks_value.append(attach_to_sinks_root_value);
    }
    value["attach_to_sinks"] = attach_to_sinks_value;
  }

  if (!attach_to_propagations_.is_bottom()) {
    auto attach_to_propagations_value = Json::Value(Json::arrayValue);
    for (const auto& [root, features] : attach_to_propagations_) {
      auto attach_to_propagations_root_value = Json::Value(Json::objectValue);
      attach_to_propagations_root_value["port"] = root.to_json();
      attach_to_propagations_root_value["features"] = features.to_json();
      attach_to_propagations_value.append(attach_to_propagations_root_value);
    }
    value["attach_to_propagations"] = attach_to_propagations_value;
  }

  auto add_features_to_arguments_value = Json::Value(Json::arrayValue);
  if (!add_features_to_arguments_.is_bottom()) {
    for (const auto& [root, features] : add_features_to_arguments_) {
      auto add_features_to_arguments_root_value =
          Json::Value(Json::objectValue);
      add_features_to_arguments_root_value["port"] = root.to_json();
      add_features_to_arguments_root_value["features"] = features.to_json();
      add_features_to_arguments_value.append(
          add_features_to_arguments_root_value);
    }
  }
  if (!add_via_value_of_features_to_arguments_.is_bottom()) {
    for (const auto& [root, via_value_of] :
         add_via_value_of_features_to_arguments_) {
      auto add_via_value_of_features_to_arguments_root_value =
          Json::Value(Json::objectValue);
      add_via_value_of_features_to_arguments_root_value["port"] =
          root.to_json();

      auto ports = Json::Value(Json::arrayValue);
      for (const auto& tagged_root : via_value_of.elements()) {
        ports.append(tagged_root.to_json());
      }
      add_via_value_of_features_to_arguments_root_value["via_value_of"] = ports;
      add_features_to_arguments_value.append(
          add_via_value_of_features_to_arguments_root_value);
    }
  }
  if (!add_features_to_arguments_value.empty()) {
    value["add_features_to_arguments"] = add_features_to_arguments_value;
  }

  if (auto getter_access_path = inline_as_getter_.get_constant()) {
    value["inline_as_getter"] = getter_access_path->to_json();
  }

  if (auto setter_access_path = inline_as_setter_.get_constant()) {
    value["inline_as_setter"] = setter_access_path->to_json();
  }

  if (!model_generators_.is_bottom()) {
    auto model_generators_value = Json::Value(Json::arrayValue);
    for (const auto* model_generator : model_generators_) {
      model_generators_value.append(model_generator->to_json());
    }
    value["model_generators"] = model_generators_value;
  }

  if (!issues_.is_bottom()) {
    auto issues_value = Json::Value(Json::arrayValue);
    for (const auto& issue : issues_) {
      mt_assert(!issue.is_bottom());
      issues_value.append(issue.to_json(ExportOriginsMode::Always));
    }
    value["issues"] = issues_value;
  }

  return value;
}

Json::Value Model::to_json(Context& context) const {
  auto value = to_json(context.options->export_origins_mode());

  if (method_) {
    const auto* position = context.positions->get(method_);
    value["position"] = position->to_json();
  }

  return value;
}

std::ostream& operator<<(std::ostream& out, const Model& model) {
  out << "\nModel(method=`" << show(model.method_) << "`";
  if (model.modes_) {
    out << ",\n  modes={";
    for (auto mode : k_all_modes) {
      if (model.modes_.test(mode)) {
        out << model_mode_to_string(mode) << ", ";
      }
    }
    out << "}";
  }
  if (model.frozen_) {
    out << ",\n  freeze={";
    for (auto freeze_kind : k_all_freeze_kinds) {
      if (model.frozen_.test(freeze_kind)) {
        out << model_freeze_kind_to_string(freeze_kind) << ", ";
      }
    }
    out << "}";
  }
  if (!model.generations_.is_bottom()) {
    out << ",\n  generations={\n";
    for (const auto& binding : model.generations_.elements()) {
      const AccessPath& port = binding.first;
      binding.second.visit_frames(
          [&out, port](const CallInfo& call_info, const Frame& generation) {
            out << "    " << port << ": call_info=" << call_info
                << ", generation=" << generation << ",\n";
          });
    }
    out << "  }";
  }
  if (!model.parameter_sources_.is_bottom()) {
    out << ",\n  parameter_sources={\n";
    for (const auto& binding : model.parameter_sources_.elements()) {
      const AccessPath& port = binding.first;
      binding.second.visit_frames(
          [&out, &port](
              const CallInfo& call_info, const Frame& parameter_source) {
            out << "    " << port << ": call_info=" << call_info
                << ", parameter_source=" << parameter_source << ",\n";
          });
    }
    out << "  }";
  }
  if (!model.sinks_.is_bottom()) {
    out << ",\n  sinks={\n";
    for (const auto& binding : model.sinks_.elements()) {
      const AccessPath& port = binding.first;
      binding.second.visit_frames(
          [&out, &port](const CallInfo& call_info, const Frame& sink) {
            out << "    " << port << ": call_info=" << call_info
                << ", sink=" << sink << ",\n";
          });
    }
    out << "  }";
  }
  if (!model.call_effect_sources_.is_bottom()) {
    out << ", \n call effect sources=" << model.call_effect_sources_;
  }
  if (!model.call_effect_sinks_.is_bottom()) {
    out << ", \n call effect sinks=" << model.call_effect_sinks_;
  }
  if (!model.propagations_.is_bottom()) {
    out << ",\n  propagation={\n";
    for (const auto& binding : model.propagations_.elements()) {
      const AccessPath& input_path = binding.first;
      binding.second.visit_frames(
          [&out, &input_path](const CallInfo& call_info, const Frame& output) {
            out << "    " << input_path << ": call_info=" << call_info
                << ", output=" << output << ",\n";
          });
    }
    out << "  }";
  }
  if (!model.global_sanitizers_.is_bottom()) {
    out << ",\n  global_sanitizers=" << model.global_sanitizers_;
  }
  if (!model.port_sanitizers_.is_bottom()) {
    out << ",\n  port_sanitizers={\n";
    for (const auto& [root, sanitizers] : model.port_sanitizers_) {
      out << "   " << root << " -> " << sanitizers << ",\n";
    }
    out << "  }";
  }
  if (!model.attach_to_sources_.is_bottom()) {
    out << ",\n attach_to_sources={\n";
    for (const auto& [root, features] : model.attach_to_sources_) {
      out << "    " << root << " -> " << features << ",\n";
    }
    out << "  }";
  }
  if (!model.attach_to_sinks_.is_bottom()) {
    out << ",\n attach_to_sinks={\n";
    for (const auto& [root, features] : model.attach_to_sinks_) {
      out << "    " << root << " -> " << features << ",\n";
    }
    out << "  }";
  }
  if (!model.attach_to_propagations_.is_bottom()) {
    out << ",\n attach_to_propagations={\n";
    for (const auto& [root, features] : model.attach_to_propagations_) {
      out << "    " << root << " -> " << features << ",\n";
    }
    out << "  }";
  }
  if (!model.add_features_to_arguments_.is_bottom()) {
    out << ",\n add_features_to_arguments={\n";
    for (const auto& [root, features] : model.add_features_to_arguments_) {
      out << "    " << root << " -> " << features << ",\n";
    }
    out << "  }";
  }
  if (!model.add_via_value_of_features_to_arguments_.is_bottom()) {
    out << ",\n add_via_value_of_features_to_arguments={\n";
    for (const auto& [root, via_value_of_ports] :
         model.add_via_value_of_features_to_arguments_) {
      out << "    " << root << " -> " << via_value_of_ports.elements() << ",\n";
    }
    out << "  }";
  }
  if (auto getter_access_path = model.inline_as_getter_.get_constant()) {
    out << ",\n  inline_as_getter=" << *getter_access_path;
  }
  if (auto setter_access_path = model.inline_as_setter_.get_constant()) {
    out << ",\n  inline_as_setter=" << *setter_access_path;
  }
  if (!model.model_generators_.is_bottom()) {
    out << ",\n  model_generators={";
    for (const auto* model_generator : model.model_generators_) {
      out << *model_generator << ", ";
    }
    out << "}";
  }
  if (!model.issues_.is_bottom()) {
    out << ",\n  issues={\n";
    for (const auto& issue : model.issues_) {
      out << "    " << issue << ",\n";
    }
    out << "  }";
  }
  return out << ")";
}

void Model::remove_kinds(const std::unordered_set<const Kind*>& to_remove) {
  auto drop_special_kinds =
      [&to_remove](const Kind* kind) -> std::vector<const Kind*> {
    if (to_remove.find(kind) != to_remove.end()) {
      return std::vector<const Kind*>();
    }
    return std::vector<const Kind*>{kind};
  };
  auto transformer = [&drop_special_kinds](Taint taint) {
    taint.transform_kind_with_features(
        drop_special_kinds,
        /* add_features, never called */
        [](const Kind*) -> FeatureMayAlwaysSet { mt_unreachable(); });
    return taint;
  };

  generations_.transform(transformer);
  parameter_sources_.transform(transformer);
  sinks_.transform(transformer);
  call_effect_sources_.transform(transformer);
  call_effect_sinks_.transform(transformer);
}

void Model::update_taint_tree(
    TaintAccessPathTree& tree,
    AccessPath port,
    std::size_t truncation_amount,
    Taint new_taint,
    const FeatureMayAlwaysSet& widening_features) {
  if (!check_port_consistency(port)) {
    return;
  }

  if (port.path().size() > truncation_amount) {
    new_taint.add_locally_inferred_features(widening_features);
  }
  port.truncate(truncation_amount);
  tree.write(port, std::move(new_taint), UpdateKind::Weak);
}

bool Model::check_root_consistency(Root root) const {
  switch (root.kind()) {
    case Root::Kind::Return: {
      if (method_ && method_->returns_void()) {
        ModelConsistencyError::raise(fmt::format(
            "Model for method `{}` contains a `Return` port but method returns void.",
            show(method_)));
        return false;
      }
      return true;
    }
    case Root::Kind::Argument: {
      auto position = root.parameter_position();
      if (method_ && position >= method_->number_of_parameters()) {
        ModelConsistencyError::raise(fmt::format(
            "Model for method `{}` contains a port on parameter {} but the method only has {} parameters.",
            show(method_),
            position,
            method_->number_of_parameters()));
        return false;
      }
      return true;
    }
    case Root::Kind::CallEffectIntent: {
      // Nothing to validate. This call-effect does not have any restrictions.
      return true;
    }
    default: {
      ModelConsistencyError::raise(fmt::format(
          "Model for method `{}` contains an invalid port: `{}`",
          show(method_),
          show(root)));
      return false;
    }
  }
}

bool Model::check_port_consistency(const AccessPath& access_path) const {
  return check_root_consistency(access_path.root());
}

bool Model::check_parameter_source_port_consistency(
    const AccessPath& access_path) const {
  if (access_path.root().is_return()) {
    ModelConsistencyError::raise(fmt::format(
        "Model for method `{}` contains a parameter source with a `Return` port."
        " Use a generation instead.",
        show(method_)));
    return false;
  }

  return true;
}

bool Model::check_taint_config_consistency(
    const TaintConfig& config,
    std::string_view kind) const {
  if (config.kind() == nullptr) {
    ModelConsistencyError::raise(fmt::format(
        "Model for method `{}` contains a unknown kind for {}.",
        show(method_),
        kind));
    return false;
  }
  return true;
}

bool Model::check_taint_consistency(const Taint& taint) const {
  taint.visit_frames([this](const CallInfo&, const Frame& frame) {
    if (!frame.via_type_of_ports().is_bottom()) {
      for (const auto& tagged_root : frame.via_type_of_ports().elements()) {
        // Logs invalid ports specifed for via_type_of but does not prevent the
        // model from being created.
        check_root_consistency(tagged_root.root());
      }
    }
    if (!frame.via_value_of_ports().is_bottom()) {
      for (const auto& tagged_root : frame.via_value_of_ports().elements()) {
        // Logs invalid ports specifed for via_value_of but does not prevent the
        // model from being created.
        check_root_consistency(tagged_root.root());
      }
    }
  });
  return true;
}

bool Model::check_inline_as_getter_consistency(
    const AccessPathConstantDomain& inline_as) const {
  auto access_path = inline_as.get_constant();

  if (!access_path) {
    return true;
  }

  if (!access_path->root().is_argument()) {
    ModelConsistencyError::raise(fmt::format(
        "Model for method `{}` has an inline-as-getter with a non-argument root.",
        show(method_)));
    return false;
  }

  return check_port_consistency(*access_path);
}

bool Model::check_inline_as_setter_consistency(
    const SetterAccessPathConstantDomain& inline_as) const {
  auto setter = inline_as.get_constant();

  if (!setter) {
    return true;
  }

  if (!setter->target().root().is_argument()) {
    ModelConsistencyError::raise(fmt::format(
        "Model for method `{}` has an inline-as-setter with a non-argument target.",
        show(method_)));
    return false;
  }

  if (!setter->value().root().is_argument()) {
    ModelConsistencyError::raise(fmt::format(
        "Model for method `{}` has an inline-as-setter with a non-argument value.",
        show(method_)));
    return false;
  }

  if (!check_port_consistency(setter->target())) {
    return false;
  }

  if (!check_port_consistency(setter->value())) {
    return false;
  }

  return true;
}

bool Model::check_call_effect_port_consistency(
    const AccessPath& port,
    std::string_view kind) const {
  if (!port.root().is_call_effect()) {
    ModelConsistencyError::raise(
        fmt::format("Port for {} must be a call effect port.", kind));
    return false;
  }

  return true;
}

void Model::add_generation(
    AccessPath port,
    Taint source,
    const Heuristics& heuristics) {
  if (method_) {
    source.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(port));
  }

  if (!check_port_consistency(port) || !check_taint_consistency(source)) {
    return;
  }

  if (port.path().size() > heuristics.generation_max_port_size()) {
    WARNING(
        1,
        "Truncating user-defined generation {} down to path length {} for method {}",
        port,
        heuristics.generation_max_port_size(),
        method_ ? method_->get_name() : "nullptr");
  }
  port.truncate(heuristics.generation_max_port_size());
  generations_.write(port, std::move(source), UpdateKind::Weak);
}

void Model::add_parameter_source(
    AccessPath port,
    Taint source,
    const Heuristics& heuristics) {
  if (method_) {
    source.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(port));
  }

  if (!check_port_consistency(port) ||
      !check_parameter_source_port_consistency(port) ||
      !check_taint_consistency(source)) {
    return;
  }

  if (port.path().size() > heuristics.parameter_source_max_port_size()) {
    WARNING(
        1,
        "Truncating user-defined parameter source {} down to path length {} for method {}",
        port,
        heuristics.parameter_source_max_port_size(),
        method_ ? method_->get_name() : "nullptr");
  }
  port.truncate(heuristics.parameter_source_max_port_size());
  parameter_sources_.write(port, Taint{std::move(source)}, UpdateKind::Weak);
}

void Model::add_sink(
    AccessPath port,
    Taint sink,
    const Heuristics& heuristics) {
  if (method_) {
    sink.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(port));
  }

  if (!check_port_consistency(port) || !check_taint_consistency(sink)) {
    return;
  }

  if (port.path().size() > heuristics.call_effect_sink_max_port_size()) {
    WARNING(
        1,
        "Truncating user-defined sink {} down to path length {} for method {}",
        port,
        heuristics.call_effect_sink_max_port_size(),
        method_ ? method_->get_name() : "nullptr");
  }
  port.truncate(heuristics.call_effect_sink_max_port_size());
  sinks_.write(port, Taint{std::move(sink)}, UpdateKind::Weak);
}

void Model::add_call_effect_source(
    AccessPath port,
    Taint source,
    const Heuristics& heuristics) {
  if (!check_call_effect_port_consistency(port, "effect source")) {
    return;
  }

  if (method_) {
    source.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(port));
  }

  if (!check_taint_consistency(source)) {
    return;
  }

  if (port.path().size() > heuristics.call_effect_source_max_port_size()) {
    WARNING(
        1,
        "Truncating user-defined call effect source port {} down to path length {} for method {}",
        port,
        heuristics.call_effect_source_max_port_size(),
        method_ ? method_->get_name() : "nullptr");
  }

  port.truncate(heuristics.call_effect_source_max_port_size());
  call_effect_sources_.write(port, std::move(source), UpdateKind::Weak);
}

void Model::add_call_effect_sink(
    AccessPath port,
    Taint sink,
    const Heuristics& heuristics) {
  if (!check_call_effect_port_consistency(port, "effect sink")) {
    return;
  }

  if (method_) {
    sink.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(port));
  }

  if (!check_taint_consistency(sink)) {
    return;
  }

  if (port.path().size() > heuristics.call_effect_sink_max_port_size()) {
    WARNING(
        1,
        "Truncating user-defined call effect sink port {} down to path length {} for method {}",
        port,
        heuristics.call_effect_sink_max_port_size(),
        method_ ? method_->get_name() : "nullptr");
  }

  port.truncate(heuristics.call_effect_sink_max_port_size());
  call_effect_sinks_.write(port, std::move(sink), UpdateKind::Weak);
}

void Model::add_propagation(
    AccessPath input_path,
    Taint output,
    const Heuristics& heuristics) {
  if (!check_root_consistency(input_path.root())) {
    return;
  }

  if (method_) {
    output.add_origins_if_declaration(
        method_, AccessPathFactory::singleton().get(input_path));
  }

  if (input_path.path().size() > heuristics.propagation_max_input_path_size()) {
    WARNING(
        1,
        "Truncating user-defined propagation {} down to path length {} for method {}",
        input_path.path(),
        heuristics.propagation_max_input_path_size(),
        method_ ? method_->get_name() : "nullptr");
  }
  input_path.truncate(heuristics.propagation_max_input_path_size());
  propagations_.write(input_path, output, UpdateKind::Weak);
}

} // namespace marianatrench
