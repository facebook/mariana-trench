/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <json/json.h>

#include <sparta/utils/EnumBitSet.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Issue.h>
#include <mariana-trench/IssueSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/PropagationConfig.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>
#include <mariana-trench/Sanitizer.h>
#include <mariana-trench/SetterAccessPathConstantDomain.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintTree.h>
#include <mariana-trench/model-generator/ModelGeneratorNameSet.h>

namespace marianatrench {

using SanitizerSet = GroupHashedSetAbstractDomain<
    Sanitizer,
    Sanitizer::GroupHash,
    Sanitizer::GroupEqual>;

/**
 * A `Model` is a summary of what we know about a method. A `Model` should
 * contain the properties we are interested in, such as *generations*,
 * *propagations* and *sinks*.
 *
 * A *mode* describes a specific behavior of a model. See `Model::Mode`.
 *
 * A *generation* describes the property that the method either
 * returns a tainted value or mutates (and hence taints) a reference type
 * argument, regardless of whether parameters are tainted.
 *
 * A *parameter source* of a method describes the property that the method
 * receives a tainted value on a given parameter.
 *
 * A *propagation* describes how taint may flow through a method. More
 * specifically, how taint may flow from a parameter to the method's return
 * value or another parameters. A *propagation* will only propagate the taint
 * if the parameter is tainted.
 *
 * A *global sanitizer* sanitizes all sources, sinks or propagations flowing
 * through the method that have a kind dictated by its kinds field
 *
 * *Attach to sources* automatically adds features to all sources flowing out of
 * the method.
 *
 * *Attach to sinks* automatically adds features to all sources flowing in
 * the method.
 *
 * *Attach to propagations* automatically adds features to all propagations from
 * or to a given argument or return value.
 *
 * *Add features to arguments* automatically adds features to all taint that
 * might flow in or out of a given argument. This is equivalent to *attach to
 * sources/sinks/propagations*, but also adds features even when there is no
 * inferred propagation. E.g,
 * ```
 * List<String> x;
 * f(x);
 * // Here x has the feature, regardless of the propagations of f.
 * ```
 *
 * *inline as getter* is either top, bottom or an argument access path that will
 * be used to inline a getter method at call sites.
 *
 * *inline as setter* is either top, bottom or a target and value access path
 * that will be used to inline a setter method at call sites.
 *
 * *model generators* is a set of model generator names that originated a part
 * of that model.
 */
class Model final {
 public:
  /**
   * A `Mode` describes a specific behavior of a model.
   */
  enum class Mode : unsigned {
    // Skip the analysis of this method.
    SkipAnalysis,

    // Add the 'via-obscure' feature to sources flowing through this method.
    AddViaObscureFeature,

    // Taint-in-taint-out (taint on arguments flow into the return value).
    TaintInTaintOut,

    // Taint-in-taint-this (taint on arguments flow into `this`).
    TaintInTaintThis,

    // Do not join all overrides at virtual call sites.
    NoJoinVirtualOverrides,

    // Do not collapse input paths when applying propagations.
    NoCollapseOnPropagation,

    // Alias existing memory location on method invokes.
    AliasMemoryLocationOnInvoke,

    // Perform a strong write on propagation
    StrongWriteOnPropagation,

    _Count,
  };

  using Modes = sparta::EnumBitSet<Mode>;

  enum class FreezeKind : unsigned {
    Generations,
    ParameterSources,
    Sinks,
    Propagations,
    _Count,
  };

  using Frozen = sparta::EnumBitSet<FreezeKind>;

 public:
  /* Create an empty model. */
  explicit Model();

  /**
   * Create a model for the given method.
   */
  explicit Model(
      const Method* MT_NULLABLE method,
      Context& context,
      Modes modes = {},
      Frozen frozen = {},
      const std::vector<std::pair<AccessPath, TaintConfig>>& generations = {},
      const std::vector<std::pair<AccessPath, TaintConfig>>& parameter_sources =
          {},
      const std::vector<std::pair<AccessPath, TaintConfig>>& sinks = {},
      const std::vector<PropagationConfig>& propagations = {},
      const std::vector<Sanitizer>& global_sanitizers = {},
      const std::vector<std::pair<Root, SanitizerSet>>& port_sanitizers = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_sources = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_sinks = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_propagations =
          {},
      const std::vector<std::pair<Root, FeatureSet>>&
          add_features_to_arguments = {},
      const AccessPathConstantDomain& inline_as_getter =
          AccessPathConstantDomain::bottom(),
      const SetterAccessPathConstantDomain& inline_as_setter =
          SetterAccessPathConstantDomain::bottom(),
      const ModelGeneratorNameSet& model_generators = {},
      const IssueSet& issues = {});

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Model)

  bool operator==(const Model& other) const;
  bool operator!=(const Model& other) const;

  const Method* MT_NULLABLE method() const {
    return method_;
  }

  /* Copy the model and attach it to the given method. */
  Model instantiate(const Method* method, Context& context) const;

  /* Return the callee model for the given callsite. */
  Model at_callsite(
      const Method* caller,
      const Position* position,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      const CallClassIntervalContext& class_interval_context) const;

  /* Create a new fresh model without sources/sinks/propagations based on the
   * structure of the current model. */
  Model initial_model_for_iteration() const;

  void collapse_invalid_paths(Context& context);

  void approximate(const FeatureMayAlwaysSet& widening_features);

  bool empty() const;

  void add_mode(Model::Mode mode, Context& context);
  void add_taint_in_taint_out(Context& context);

  // Intended to be used for methods whose concrete definition is unknown,
  // i.e. `dex_method_reference->is_def() == false`.
  void add_taint_in_taint_out(
      Context& context,
      const IRInstruction* instruction);

  void add_taint_in_taint_this(Context& context);

  void add_generation(const AccessPath& port, TaintConfig generation);

  /* Add generations after applying sanitizers */
  void add_inferred_generations(
      AccessPath port,
      Taint generations,
      const FeatureMayAlwaysSet& widening_features);
  const TaintAccessPathTree& generations() const {
    return generations_;
  }
  void set_generations(TaintAccessPathTree generations) {
    generations_ = std::move(generations);
  }

  void add_parameter_source(const AccessPath& port, TaintConfig source);
  const TaintAccessPathTree& parameter_sources() const {
    return parameter_sources_;
  }
  void set_parameter_sources(TaintAccessPathTree parameter_sources) {
    parameter_sources_ = std::move(parameter_sources);
  }

  void add_sink(const AccessPath& port, TaintConfig sink);

  /* Add sinks after applying sanitizers */
  void add_inferred_sinks(
      AccessPath port,
      Taint sinks,
      const FeatureMayAlwaysSet& widening_features);
  const TaintAccessPathTree& sinks() const {
    return sinks_;
  }
  void set_sinks(TaintAccessPathTree sinks) {
    sinks_ = std::move(sinks);
  }

  const TaintAccessPathTree& call_effect_sources() const {
    return call_effect_sources_;
  }
  void add_call_effect_source(AccessPath port, TaintConfig source);

  const TaintAccessPathTree& call_effect_sinks() const {
    return call_effect_sinks_;
  }
  void add_call_effect_sink(AccessPath port, TaintConfig sink);
  void add_inferred_call_effect_sinks(AccessPath port, Taint sink);
  void add_inferred_call_effect_sinks(
      AccessPath port,
      Taint sink,
      const FeatureMayAlwaysSet& widening_features);

  void add_propagation(PropagationConfig propagation);
  /* Add a propagation after applying sanitizers */
  void add_inferred_propagations(
      AccessPath input_path,
      Taint local_taint,
      const FeatureMayAlwaysSet& widening_features);
  const TaintAccessPathTree& propagations() const {
    return propagations_;
  }

  void add_global_sanitizer(Sanitizer sanitizer);
  const SanitizerSet& global_sanitizers() const {
    return global_sanitizers_;
  }
  Taint
  apply_source_sink_sanitizers(SanitizerKind kind, Taint taint, Root root);
  bool has_global_propagation_sanitizer() const;

  void add_port_sanitizers(SanitizerSet sanitizers, Root root);

  void add_attach_to_sources(Root root, FeatureSet features);
  FeatureSet attach_to_sources(Root root) const;

  void add_attach_to_sinks(Root root, FeatureSet features);
  FeatureSet attach_to_sinks(Root root) const;

  void add_attach_to_propagations(Root root, FeatureSet features);
  FeatureSet attach_to_propagations(Root root) const;

  void add_add_features_to_arguments(Root root, FeatureSet features);
  void add_add_via_value_of_features_to_arguments(
      Root root,
      TaggedRootSet via_value_of_ports);
  bool has_add_features_to_arguments() const;
  FeatureSet add_features_to_arguments(Root root) const;

  const AccessPathConstantDomain& inline_as_getter() const;
  void set_inline_as_getter(AccessPathConstantDomain inline_as_getter);

  const SetterAccessPathConstantDomain& inline_as_setter() const;
  void set_inline_as_setter(SetterAccessPathConstantDomain inline_as_setter);

  void add_model_generator(const ModelGeneratorName* model_generator);
  void add_model_generator_if_empty(const ModelGeneratorName* model_generator);

  // Converts all existing model generator (names) into their "sharded"
  // equivalent. Used when loading from `Options::sharded_model_generators` to
  // indicate that the original model generators are indirectly responsible for
  // this model. The `identifier` is the directory name containing the models.
  void make_sharded_model_generators(const std::string& identifier);

  void add_issue(Issue issue);
  const IssueSet& issues() const {
    return issues_;
  }
  void set_issues(IssueSet issues) {
    issues_ = std::move(issues);
  }

  void remove_kinds(const std::unordered_set<const Kind*>& to_remove);

  bool skip_analysis() const;
  bool add_via_obscure_feature() const;
  bool is_taint_in_taint_out() const;
  bool is_taint_in_taint_this() const;
  bool no_join_virtual_overrides() const;
  bool no_collapse_on_propagation() const;
  bool alias_memory_location_on_invoke() const;
  bool strong_write_on_propagation() const;
  Modes modes() const {
    return modes_;
  }

  bool is_frozen(FreezeKind freeze_kind) const;
  void freeze(FreezeKind freeze_kind);

  bool leq(const Model& other) const;
  void join_with(const Model& other);

  std::unordered_set<const Kind*> source_kinds() const;
  std::unordered_set<const Kind*> sink_kinds() const;

  /**
   * This method is used to detect the presence of specific taint transforms.
   * Only local transforms are considered and returned because if something
   * exists as a global transform, it would have came from a local transform
   * somewhere else (in a different Model).
   */
  std::unordered_set<const Transform*> local_transform_kinds() const;

  /**
   * Parses the JSON from a user's configuration. The input JSON schema is not
   * symmetrical to `to_json`, even if it looks somewhat similar. Use
   * `from_json()` if symmetry is expected.
   */
  static Model from_config_json(
      const Method* MT_NULLABLE method,
      const Json::Value& value,
      Context& context,
      bool check_unexpected_members = true);

  /**
   * Helper to read all generations, parameter_sources and sinks type taint
   * configs from the given JSON \p value. Only the configs which match
   * \p predicate are extracted and inserted into \p generations,
   * \p parameter_sources, and \p sinks respectively.
   *
   * @param value Model configuration JSON value.
   * @param generations All extracted generations JSON taint configs.
   * @param parameter_sources All extracted parameter_sources JSON taint
   * configs.
   * @param sinks All extracted sinks JSON taint configs.
   * @param json_sources \see Model::from_json
   * @param json_sinks \see Model::from_json
   * @param predicate indicates which JSON config objects to extract. At the
   * moment this is either TaintConfigTemplate::is_concrete to extract all JSON
   * configs which can be directly added to the model, or
   * TaintConfigTemplate::is_template to extract the taint configs which need
   * to be stored in ModelTemplate for later instantiation.
   */
  static void read_taint_configs_from_json(
      const Json::Value& model,
      std::vector<std::pair<AccessPath, Json::Value>>& generations,
      std::vector<std::pair<AccessPath, Json::Value>>& parameter_sources,
      std::vector<std::pair<AccessPath, Json::Value>>& sinks,
      bool (*predicate)(const Json::Value& value));

  static Model from_json(const Json::Value& value, Context& context);
  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  /* Export the model to json and include the method position. */
  Json::Value to_json(Context& context) const;

  friend std::ostream& operator<<(std::ostream& out, const Model& model);

 private:
  void update_taint_tree(
      TaintAccessPathTree& tree,
      AccessPath port,
      std::size_t truncation_amount,
      Taint new_taint,
      const FeatureMayAlwaysSet& widening_features);

  bool check_root_consistency(Root root) const;
  bool check_port_consistency(const AccessPath& access_path) const;
  bool check_parameter_source_port_consistency(
      const AccessPath& access_path) const;
  bool check_taint_config_consistency(
      const TaintConfig& frame,
      std::string_view kind) const;
  bool check_taint_consistency(const Taint& frame) const;
  bool check_inline_as_getter_consistency(
      const AccessPathConstantDomain& inline_as) const;
  bool check_inline_as_setter_consistency(
      const SetterAccessPathConstantDomain& inline_as) const;
  bool check_call_effect_port_consistency(
      const AccessPath& port,
      std::string_view kind) const;

  // In the following methods, the `Taint` object should originate from a
  // `Model` object. This guarantees that it was constructed using a
  // `TaintConfig` in `Model`'s constructor, and has passed other verification
  // checks in the `add_*(AccessPath, TaintConfig)` methods.
  void add_generation(AccessPath port, Taint generation);
  void add_parameter_source(AccessPath port, Taint source);
  void add_sink(AccessPath port, Taint sink);
  void add_call_effect_source(AccessPath port, Taint source);
  void add_call_effect_sink(AccessPath port, Taint sink);
  void add_propagation(AccessPath input_path, Taint output);

 private:
  const Method* MT_NULLABLE method_;
  Modes modes_;
  Frozen frozen_;
  TaintAccessPathTree generations_;
  TaintAccessPathTree parameter_sources_;
  TaintAccessPathTree sinks_;
  TaintAccessPathTree call_effect_sources_;
  TaintAccessPathTree call_effect_sinks_;
  TaintAccessPathTree propagations_;
  SanitizerSet global_sanitizers_;
  RootPatriciaTreeAbstractPartition<SanitizerSet> port_sanitizers_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_sources_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_sinks_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_propagations_;
  RootPatriciaTreeAbstractPartition<FeatureSet> add_features_to_arguments_;
  RootPatriciaTreeAbstractPartition<TaggedRootSet>
      add_via_value_of_features_to_arguments_;
  AccessPathConstantDomain inline_as_getter_;
  SetterAccessPathConstantDomain inline_as_setter_;
  ModelGeneratorNameSet model_generators_;
  IssueSet issues_;
};

inline Model::Modes operator|(Model::Mode left, Model::Mode right) {
  return Model::Modes(left) | right;
}

inline Model::Frozen operator|(
    Model::FreezeKind left,
    Model::FreezeKind right) {
  return Model::Frozen(left) | right;
}

std::string model_mode_to_string(Model::Mode mode);
std::optional<Model::Mode> string_to_model_mode(const std::string& mode);

constexpr std::array<Model::Mode, 8> k_all_modes = {
    Model::Mode::SkipAnalysis,
    Model::Mode::AddViaObscureFeature,
    Model::Mode::TaintInTaintOut,
    Model::Mode::TaintInTaintThis,
    Model::Mode::NoJoinVirtualOverrides,
    Model::Mode::NoCollapseOnPropagation,
    Model::Mode::AliasMemoryLocationOnInvoke,
    Model::Mode::StrongWriteOnPropagation,
};

constexpr std::array<Model::FreezeKind, 4> k_all_freeze_kinds = {
    Model::FreezeKind::Generations,
    Model::FreezeKind::ParameterSources,
    Model::FreezeKind::Sinks,
    Model::FreezeKind::Propagations};

} // namespace marianatrench
