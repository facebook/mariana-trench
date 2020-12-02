// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>
#include <optional>

namespace marianatrench {

class Heuristics {
 public:
  /**
   * When a method has a set of overrides greater than this threshold, we do not
   * join all overrides at call sites.
   */
  constexpr static std::size_t kJoinOverrideThreshold = 40;

  /**
   * When a method has a set of overrides greater than this threshold, is called
   * at least once and is not marked with `NoJoinVirtualOverrides`, we print a
   * warning.
   */
  constexpr static std::optional<std::size_t> kWarnOverrideThreshold =
      std::nullopt;

  /**
   * Maximum height of an abstract tree after widening.
   *
   * When reaching the maximum, we collapse the leaves to reduce the height.
   */
  constexpr static std::size_t kAbstractTreeWideningHeight = 4;

  /**
   * Maximum number of leaves in a model source or sink tree.
   *
   * When reaching the maximum, we collapse all the subtrees into a single node.
   */
  constexpr static std::size_t kModelTreeMaxLeaves = 20;

  /**
   * Maximum size of the port of a generation.
   */
  constexpr static std::size_t kGenerationMaxPortSize = 4;

  /**
   * Maximum size of the port of a parameter source.
   */
  constexpr static std::size_t kParameterSourceMaxPortSize = 4;

  /**
   * Maximum size of the port of a sink.
   */
  constexpr static std::size_t kSinkMaxPortSize = 4;

  /**
   * Maximum size of the input or output access path of a propagation.
   */
  constexpr static std::size_t kPropagationMaxPathSize = 2;

  /**
   * Maximum number of global iterations before we abort.
   */
  constexpr static std::size_t kMaxNumberIterations = 150;

  /**
   * Maximum number of local positions per frame.
   */
  constexpr static std::size_t kMaxNumberLocalPositions = 20;
};

} // namespace marianatrench
