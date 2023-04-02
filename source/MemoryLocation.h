/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <DexClass.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

class FieldMemoryLocation;

class MemoryLocation {
 protected:
  MemoryLocation() = default;

 public:
  MemoryLocation(const MemoryLocation&) = delete;
  MemoryLocation(MemoryLocation&&) = delete;
  MemoryLocation& operator=(const MemoryLocation&) = delete;
  MemoryLocation& operator=(MemoryLocation&&) = delete;
  virtual ~MemoryLocation() = default;

  /* Check wether the memory location has the given type. */
  template <typename T>
  bool is() const {
    static_assert(std::is_base_of<MemoryLocation, T>::value, "invalid check");
    return dynamic_cast<const T*>(this) != nullptr;
  }

  /* Return the memory location as the given type, or nullptr. */
  template <typename T>
  T* MT_NULLABLE as() {
    static_assert(std::is_base_of<MemoryLocation, T>::value, "invalid cast");
    return dynamic_cast<T*>(this);
  }

  /* Return the memory location for the given field of this memory location. */
  FieldMemoryLocation* make_field(const DexString* field);

  /**
   * Return the root memory location for this memory location.
   *
   * This is either a parameter or an instruction result.
   */
  virtual MemoryLocation* root();

  /**
   * Return the path (i.e, list of fields) from the root to this memory
   * location.
   */
  virtual const Path& path();

  /**
   * Return the access path that this memory location describes.
   *
   * This returns std::nullopt if the root is not a parameter.
   */
  std::optional<AccessPath> access_path();

  virtual std::string str() const = 0;

 private:
  friend std::ostream& operator<<(
      std::ostream& out,
      const MemoryLocation& memory_location);

 private:
  std::unordered_map<const DexString*, std::unique_ptr<FieldMemoryLocation>>
      fields_;
};

class ParameterMemoryLocation : public MemoryLocation {
 public:
  explicit ParameterMemoryLocation(ParameterPosition position);

  std::string str() const override;

  ParameterPosition position() const {
    return position_;
  }

 private:
  ParameterPosition position_;
};

class ThisParameterMemoryLocation final : public ParameterMemoryLocation {
 public:
  explicit ThisParameterMemoryLocation();

  std::string str() const override;
};

/* Memory location representing a field of an object. */
class FieldMemoryLocation final : public MemoryLocation {
 public:
  explicit FieldMemoryLocation(MemoryLocation* parent, const DexString* field);

  std::string str() const override;

  MemoryLocation* parent() const {
    return parent_;
  }

  const DexString* field() const {
    return field_;
  }

  MemoryLocation* root() override;

  const Path& path() override;

 private:
  MemoryLocation* parent_;
  const DexString* field_;

  MemoryLocation* root_;
  Path path_;
};

/* Memory location representing the result of an instruction. */
class InstructionMemoryLocation final : public MemoryLocation {
 public:
  explicit InstructionMemoryLocation(const IRInstruction* instruction);

  std::string str() const override;

  const IRInstruction* instruction() const {
    return instruction_;
  }

  std::optional<std::string> get_constant() const;

 private:
  const IRInstruction* instruction_;
};

/**
 * A memory factory to create unique memory location pointers.
 *
 * Note that this is NOT thread-safe.
 */
class MemoryFactory final {
 public:
  explicit MemoryFactory(const Method* method);

  MemoryFactory(const MemoryFactory&) = delete;
  MemoryFactory(MemoryFactory&&) = delete;
  MemoryFactory& operator=(const MemoryFactory&) = delete;
  MemoryFactory& operator=(MemoryFactory&&) = delete;
  ~MemoryFactory() = default;

  /* Return the memory location representing the given parameter. */
  ParameterMemoryLocation* make_parameter(ParameterPosition parameter_position);

  /* Return a memory location representing the result of the given instruction.
   */
  InstructionMemoryLocation* make_location(const IRInstruction* instruction);

 private:
  std::vector<std::unique_ptr<ParameterMemoryLocation>> parameters_;
  std::unordered_map<
      const IRInstruction*,
      std::unique_ptr<InstructionMemoryLocation>>
      instructions_;
};

} // namespace marianatrench
