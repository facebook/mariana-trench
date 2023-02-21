/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

/**
 * Include all abstract domain methods from the given `WrappedClass` class into
 * the `WrapperClass` class. All method calls are forwarded to the
 * `wrapped_member` member of `WrapperClass` type.
 */
#define INCLUDE_ABSTRACT_DOMAIN_METHODS(                      \
    WrapperClass, WrappedClass, wrapped_member)               \
  static WrapperClass bottom() {                              \
    return WrapperClass(WrappedClass::bottom());              \
  }                                                           \
                                                              \
  static WrapperClass top() {                                 \
    return WrapperClass(WrappedClass::top());                 \
  }                                                           \
                                                              \
  bool is_bottom() const override {                           \
    return this->wrapped_member.is_bottom();                  \
  }                                                           \
                                                              \
  bool is_top() const override {                              \
    return this->wrapped_member.is_top();                     \
  }                                                           \
                                                              \
  void set_to_bottom() override {                             \
    this->wrapped_member.set_to_bottom();                     \
  }                                                           \
                                                              \
  void set_to_top() override {                                \
    this->wrapped_member.set_to_top();                        \
  }                                                           \
                                                              \
  bool leq(const WrapperClass& other) const override {        \
    return this->wrapped_member.leq(other.wrapped_member);    \
  }                                                           \
                                                              \
  bool equals(const WrapperClass& other) const override {     \
    return this->wrapped_member.equals(other.wrapped_member); \
  }                                                           \
                                                              \
  void join_with(const WrapperClass& other) override {        \
    this->wrapped_member.join_with(other.wrapped_member);     \
  }                                                           \
                                                              \
  void widen_with(const WrapperClass& other) override {       \
    this->wrapped_member.widen_with(other.wrapped_member);    \
  }                                                           \
                                                              \
  void meet_with(const WrapperClass& other) override {        \
    this->wrapped_member.meet_with(other.wrapped_member);     \
  }                                                           \
                                                              \
  void narrow_with(const WrapperClass& other) override {      \
    this->wrapped_member.narrow_with(other.wrapped_member);   \
  }

/**
 * Include all set method from the given `WrappedClass` class into the
 * `WrapperClass` class. All method calls are forwarded to the `wrapped_member`
 * member of `WrapperClass` type.
 */
#define INCLUDE_SET_METHODS(                                               \
    WrapperClass, WrappedClass, wrapped_member, ElementType, IteratorType) \
  explicit WrapperClass(std::initializer_list<ElementType> elements)       \
      : wrapped_member(elements) {}                                        \
                                                                           \
  explicit WrapperClass(sparta::PatriciaTreeSet<ElementType> elements)     \
      : wrapped_member(std::move(elements)) {}                             \
                                                                           \
  bool empty() const {                                                     \
    return this->wrapped_member.empty();                                   \
  }                                                                        \
                                                                           \
  IteratorType begin() const {                                             \
    return this->wrapped_member.begin();                                   \
  }                                                                        \
                                                                           \
  IteratorType end() const {                                               \
    return this->wrapped_member.end();                                     \
  }                                                                        \
                                                                           \
  const sparta::PatriciaTreeSet<ElementType>& elements() const {           \
    return this->wrapped_member.elements();                                \
  }                                                                        \
                                                                           \
  size_t size() const {                                                    \
    return this->wrapped_member.size();                                    \
  }                                                                        \
                                                                           \
  void add(ElementType element) {                                          \
    this->wrapped_member.add(element);                                     \
  }                                                                        \
                                                                           \
  void remove(ElementType element) {                                       \
    this->wrapped_member.remove(element);                                  \
  }                                                                        \
                                                                           \
  bool contains(ElementType element) const {                               \
    return this->wrapped_member.contains(element);                         \
  }                                                                        \
                                                                           \
  void difference_with(const WrapperClass& other) {                        \
    this->wrapped_member.difference_with(other.wrapped_member);            \
  }

/**
 * Include all set concept member types from the given `WrappedClass` class.
 */
#define INCLUDE_SET_MEMBER_TYPES(WrappedClass, ElementType) \
  using iterator = WrappedClass::iterator;                  \
  using const_iterator = WrappedClass::iterator;            \
  using value_type = ElementType;                           \
  using difference_type = std::ptrdiff_t;                   \
  using size_type = size_t;                                 \
  using const_reference = ElementType&;                     \
  using const_pointer = ElementType*;

#define INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Class) \
  Class(const Class&) = default;                                 \
  Class(Class&&) = default;                                      \
  Class& operator=(const Class&) = default;                      \
  Class& operator=(Class&&) = default;                           \
  ~Class() = default;

#define DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Class) \
  Class(const Class&) = delete;                         \
  Class(Class&&) = delete;                              \
  Class& operator=(const Class&) = delete;              \
  Class& operator=(Class&&) = delete;                   \
  ~Class() = default;
