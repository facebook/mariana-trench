// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <mariana-trench/Feature.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

class Features final {
 public:
  Features() = default;
  Features(const Features&) = delete;
  Features(Features&&) = delete;
  Features& operator=(const Features&) = delete;
  Features& operator=(Features&&) = delete;
  ~Features() = default;

  const Feature* get(const std::string& data) const;

 private:
  UniquePointerFactory<std::string, Feature> factory_;
};

} // namespace marianatrench
