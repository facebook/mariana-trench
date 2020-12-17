/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/filesystem/path.hpp>
#include <json/json.h>

#include <ConcurrentContainers.h>
#include <DexClass.h>
#include <DexStore.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>

namespace {

constexpr std::size_t k_default_shard_limit = 10000;

} // namespace

namespace marianatrench {

class Registry final {
 public:
  /* Create a registry with default models for all methods. */
  explicit Registry(Context& context, const DexStoresVector& stores);

  /* Create a registry with the given models. */
  explicit Registry(Context& context, const std::vector<Model>& models);

  /* Create a registry with the given json models. */
  explicit Registry(Context& context, const Json::Value& models_value);

  Registry(const Registry&) = delete;
  Registry(Registry&&) = default;
  Registry& operator=(const Registry&) = delete;
  Registry& operator=(Registry&&) = delete;
  ~Registry() = default;

  /**
   * Load the global registry
   *
   * This joins all generated models and json models.
   * Afterwards, it creates a default model for methods that don't have one.
   */
  static Registry load(
      Context& context,
      const Options& options,
      const DexStoresVector& stores,
      const std::vector<Model>& generated_models);

  void add_default_models();

  /* This is thread-safe. */
  Model get(const Method* method) const;

  /* This is thread-safe. */
  void set(const Model& model);

  std::size_t models_size() const;
  std::size_t issues_size() const;

  void join_with(const Model& model);
  void join_with(const Registry& other);

  /*
   * After an analysis, the registry might contain invalid traces because of
   * collapses in the abstract tree domain.
   *
   * # Example
   * At global iteration 1, method `f` as a source on port `Return.foo`.
   * Method `g` is analyzed and finds an issue, referring to the source from
   * `f`. The issue has the following sources:
   * ```
   * Taint{Frame(callee=`f`, callee_port=`Return.foo`, ...)}
   * ```
   *
   * At global iteration 2, the source in `f` gets collapse into port `Return`.
   * This can happen for many reasons, for instance if the source tree gets too
   * wide. Method `g` now infers an issue with the sources:
   * ```
   * Taint{
   *    Frame(callee=`f`, callee_port=`Return`, ...),
   *    Frame(callee=`f`, callee_port=`Return.foo`, ...),
   * }
   * ```
   * If we export this in our results, this would result in invalid traces
   * because in `f`, there is no more source for `Return.foo`.
   *
   * To prevent that, we remove the frame with callee port `Return.foo` here.
   */
  void postprocess_remove_collapsed_traces();

  /*
   * Add a start and end column to the positions involved in issues so that they
   * can be highlighted in the Zoncolan UI
   */
  void augment_positions();

  void dump_metadata(const boost::filesystem::path& path) const;
  void dump_models(
      const boost::filesystem::path& path,
      const std::size_t shard_limit = k_default_shard_limit) const;
  std::string dump_models() const;
  Json::Value models_to_json() const;

 private:
  Context& context_;

  mutable ConcurrentMap<const Method*, Model> models_;
};

} // namespace marianatrench
