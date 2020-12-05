/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

/**
 * Context for the analysis of a single method.
 */
class MethodContext final {
 public:
  MethodContext(Context& context, const Registry& registry, Model& model);
  MethodContext(const MethodContext&) = delete;
  MethodContext(MethodContext&&) = delete;
  MethodContext& operator=(const MethodContext&) = delete;
  MethodContext& operator=(MethodContext&&) = delete;
  ~MethodContext() = default;

  const Method* method() const {
    return model.method();
  }

  bool dump() const {
    return dump_;
  }

  Model model_at_callsite(
      const CallTarget& call_target,
      const Position* position) const;

  const Options& options;
  const ArtificialMethods& artificial_methods;
  const Methods& methods;
  const Positions& positions;
  const Types& types;
  const ClassProperties& class_properties;
  const Overrides& overrides;
  const CallGraph& call_graph;
  const Rules& rules;
  const Dependencies& dependencies;
  const Scheduler& scheduler;
  const Kinds& kinds;
  const Features& features;
  const Registry& registry;
  MemoryFactory memory_factory;
  Model& model;

 private:
  struct CacheKey {
    CallTarget call_target;
    const Position* position;

    bool operator==(const CacheKey& other) const {
      return call_target == other.call_target && position == other.position;
    }
  };

  struct CacheKeyHash {
    std::size_t operator()(const CacheKey& key) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, std::hash<CallTarget>()(key.call_target));
      boost::hash_combine(seed, key.position);
      return seed;
    }
  };

 private:
  Context& context_;
  bool dump_;
  mutable std::unordered_map<CacheKey, Model, CacheKeyHash>
      callsite_model_cache_;
};

} // namespace marianatrench
