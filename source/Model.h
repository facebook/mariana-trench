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

#include <mariana-trench/Access.h>
#include <mariana-trench/CallEffects.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Flags.h>
#include <mariana-trench/Issue.h>
#include <mariana-trench/IssueSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/Propagation.h>
#include <mariana-trench/PropagationSet.h>
#include <mariana-trench/PropagationTree.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>
#include <mariana-trench/Sanitizer.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintTree.h>

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
 * if the parameter is tainted. See `Propagation`.
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
 * *inline as* is either top, bottom or an argument access path that will be
 * used to inline the method at call sites.
 */
class Model final {
 public:
  /**
   * A `Mode` describes a specific behavior of a model.
   *
   * Note that enumeration values must be a power of 2, see `Flags`.
   */
  enum class Mode : unsigned {
    // Do not infer modes using heuristics.
    OverrideDefault = 0x1,

    // Skip the analysis of this method.
    SkipAnalysis = 0x2,

    // Add the 'via-obscure' feature to sources flowing through this method.
    AddViaObscureFeature = 0x4,

    // Taint-in-taint-out (taint on arguments flow into the return value).
    TaintInTaintOut = 0x8,

    // Taint-in-taint-this (taint on arguments flow into `this`).
    TaintInTaintThis = 0x10,

    // Do not join all overrides at virtual call sites.
    NoJoinVirtualOverrides = 0x20,

    // Do not collapse input paths when applying propagations.
    NoCollapseOnPropagation = 0x40,

    // Alias existing memory location on method invokes.
    AliasMemoryLocationOnInvoke = 0x80,

    Normal = 0,
  };

  using Modes = Flags<Mode>;

 public:
  /* Create an empty model. */
  explicit Model();

  /**
   * Create a model for the given method.
   *
   * By default, it uses a set of heuristics to infer the modes. Use
   * `Mode::OverrideDefault` to disable it.
   */
  explicit Model(
      const Method* MT_NULLABLE method,
      Context& context,
      Modes modes = Mode::Normal,
      const std::vector<std::pair<AccessPath, TaintConfig>>& generations = {},
      const std::vector<std::pair<AccessPath, TaintConfig>>& parameter_sources =
          {},
      const std::vector<std::pair<AccessPath, TaintConfig>>& sinks = {},
      const std::vector<std::pair<Propagation, AccessPath>>& propagations = {},
      const std::vector<Sanitizer>& global_sanitizers = {},
      const std::vector<std::pair<Root, SanitizerSet>>& port_sanitizers = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_sources = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_sinks = {},
      const std::vector<std::pair<Root, FeatureSet>>& attach_to_propagations =
          {},
      const std::vector<std::pair<Root, FeatureSet>>&
          add_features_to_arguments = {},
      const AccessPathConstantDomain& inline_as =
          AccessPathConstantDomain::bottom(),
      const IssueSet& issues = {});

  Model(const Model& other) = default;
  Model(Model&&) = default;
  Model& operator=(const Model& other) = default;
  Model& operator=(Model&&) = default;
  ~Model() = default;

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
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  void collapse_invalid_paths(Context& context);

  void approximate(const FeatureMayAlwaysSet& widening_features);

  bool empty() const;

  void add_mode(Model::Mode mode, Context& context);
  void add_taint_in_taint_out(Context& context);
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

  const CallEffectsAbstractDomain& call_effect_sources() const {
    return call_effect_sources_;
  }
  void add_call_effect_source(CallEffect effect, TaintConfig source);

  const CallEffectsAbstractDomain& call_effect_sinks() const {
    return call_effect_sinks_;
  }
  void add_call_effect_sink(CallEffect effect, TaintConfig sink);
  void add_inferred_call_effect_sinks(CallEffect effect, Taint sink);

  void add_propagation(Propagation propagation, AccessPath output);
  /* Add a propagation after applying sanitizers */
  void add_inferred_propagation(Propagation propagation, AccessPath output);
  const PropagationAccessPathTree& propagations() const {
    return propagations_;
  }

  void add_global_sanitizer(Sanitizer sanitizer);
  const SanitizerSet& global_sanitizers() const {
    return global_sanitizers_;
  }
  Taint
  apply_source_sink_sanitizers(SanitizerKind kind, Taint taint, Root root);
  bool has_global_propagation_sanitizer();

  void add_port_sanitizers(SanitizerSet sanitizers, Root root);

  void add_attach_to_sources(Root root, FeatureSet features);
  FeatureSet attach_to_sources(Root root) const;

  void add_attach_to_sinks(Root root, FeatureSet features);
  FeatureSet attach_to_sinks(Root root) const;

  void add_attach_to_propagations(Root root, FeatureSet features);
  FeatureSet attach_to_propagations(Root root) const;

  void add_add_features_to_arguments(Root root, FeatureSet features);
  bool has_add_features_to_arguments() const;
  FeatureSet add_features_to_arguments(Root root) const;

  const AccessPathConstantDomain& inline_as() const;
  void set_inline_as(AccessPathConstantDomain inline_as);

  void add_issue(Issue issue);
  const IssueSet& issues() const {
    return issues_;
  }
  void set_issues(IssueSet issues) {
    issues_ = std::move(issues);
  }

  void remove_kinds(const std::unordered_set<const Kind*>& to_remove);

  bool override_default() const;
  bool skip_analysis() const;
  bool add_via_obscure_feature() const;
  bool is_taint_in_taint_out() const;
  bool is_taint_in_taint_this() const;
  bool no_join_virtual_overrides() const;
  bool no_collapse_on_propagation() const;
  bool alias_memory_location_on_invoke() const;
  Modes modes() const {
    return modes_;
  }

  bool leq(const Model& other) const;
  void join_with(const Model& other);

  static Model from_json(
      const Method* MT_NULLABLE method,
      const Json::Value& value,
      Context& context);
  Json::Value to_json() const;

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
  bool check_taint_consistency(const Taint& frame, std::string_view kind) const;
  bool check_propagation_consistency(const Propagation& propagation) const;
  bool check_inline_as_consistency(
      const AccessPathConstantDomain& inline_as) const;

  // In the following methods, the `Taint` object should originate from a
  // `Model` object. This guarantees that it was constructed using a
  // `TaintBuilder` in `Model`'s constructor, and has passed other verification
  // checks in the `add_*(AccessPath, TaintBuilder)` methods.
  void add_generation(AccessPath port, Taint generation);
  void add_parameter_source(AccessPath port, Taint source);
  void add_sink(AccessPath port, Taint sink);
  void add_call_effect_source(CallEffect effect, Taint source);
  void add_call_effect_sink(CallEffect effect, Taint sink);

 private:
  const Method* MT_NULLABLE method_;
  Modes modes_;
  TaintAccessPathTree generations_;
  TaintAccessPathTree parameter_sources_;
  TaintAccessPathTree sinks_;
  CallEffectsAbstractDomain call_effect_sources_;
  CallEffectsAbstractDomain call_effect_sinks_;
  PropagationAccessPathTree propagations_;
  SanitizerSet global_sanitizers_;
  RootPatriciaTreeAbstractPartition<SanitizerSet> port_sanitizers_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_sources_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_sinks_;
  RootPatriciaTreeAbstractPartition<FeatureSet> attach_to_propagations_;
  RootPatriciaTreeAbstractPartition<FeatureSet> add_features_to_arguments_;
  AccessPathConstantDomain inline_as_;
  IssueSet issues_;
};

inline Model::Modes operator|(Model::Mode left, Model::Mode right) {
  return Model::Modes(left) | right;
}

std::string model_mode_to_string(Model::Mode mode);
std::optional<Model::Mode> string_to_model_mode(const std::string& mode);

constexpr std::array<Model::Mode, 8> k_all_modes = {
    Model::Mode::OverrideDefault,
    Model::Mode::SkipAnalysis,
    Model::Mode::AddViaObscureFeature,
    Model::Mode::TaintInTaintOut,
    Model::Mode::TaintInTaintThis,
    Model::Mode::NoJoinVirtualOverrides,
    Model::Mode::NoCollapseOnPropagation,
    Model::Mode::AliasMemoryLocationOnInvoke};

} // namespace marianatrench
