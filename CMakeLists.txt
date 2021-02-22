# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19.3 FATAL_ERROR)
project(MarianaTrench VERSION 0.1 LANGUAGES CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")
endif()
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")

enable_testing()

# Add path for custom cmake modules.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Generate compile_commands.json, useful for editors and static analyzers.
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost 1.75.0 REQUIRED
             COMPONENTS system regex filesystem program_options iostreams thread)
# CMake's provided `FindGTest.cmake` does not define GMock.
# We use `CONFIG` to find the `GTestConfig.cmake` provided by gtest, which properly defines GMock.
find_package(GTest 1.10.0 REQUIRED CONFIG)
find_package(JsonCpp 1.9.4 REQUIRED)
find_package(fmt 7.1.2 REQUIRED)
find_package(re2 REQUIRED)
find_package(Redex REQUIRED)

# Create a header directory header-tree/mariana-trench/ pointing to source/
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/header-tree")
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_CURRENT_BINARY_DIR}/header-tree/mariana-trench" SYMBOLIC)

# Enable/disable warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-Wall" "-Wextra" "-Wno-nullability-completeness")
else()
  message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} is not currently supported.")
endif()

# Targets
file(GLOB library_sources
     "source/*.cpp"
     "source/model-generator/*.cpp")
list(FILTER library_sources EXCLUDE REGEX ".*/source/Main.cpp")
add_library(mariana-trench-library STATIC ${library_sources})
target_link_libraries(mariana-trench-library PUBLIC
                      Threads::Threads
                      ZLIB::ZLIB
                      Boost::system
                      Boost::regex
                      Boost::filesystem
                      Boost::program_options
                      Boost::iostreams
                      Boost::thread
                      GTest::gtest
                      JsonCpp::JsonCpp
                      fmt::fmt
                      re2::re2
                      Redex::LibTool)
target_include_directories(mariana-trench-library PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/header-tree")

add_executable(mariana-trench-binary "source/Main.cpp")
target_link_libraries(mariana-trench-binary PUBLIC mariana-trench-library)
install(TARGETS mariana-trench-binary DESTINATION "bin")

# Tests
include(GoogleTest)
add_custom_target(build-tests)

add_library(mariana-trench-test-library STATIC EXCLUDE_FROM_ALL "source/tests/Test.cpp")
target_link_libraries(mariana-trench-test-library PUBLIC mariana-trench-library)

file(GLOB unit_test_sources "source/tests/*.cpp")
add_executable(mariana-trench-unit-tests EXCLUDE_FROM_ALL ${unit_test_sources})
target_link_libraries(mariana-trench-unit-tests PUBLIC
                      mariana-trench-test-library
                      GTest::gmock
                      GTest::gtest_main)
add_dependencies(build-tests mariana-trench-unit-tests)
gtest_discover_tests(mariana-trench-unit-tests)

file(GLOB model_generator_test_sources "source/model-generator/tests/*.cpp")
add_executable(mariana-trench-model-generator-tests EXCLUDE_FROM_ALL ${model_generator_test_sources})
target_link_libraries(mariana-trench-model-generator-tests PUBLIC
                      mariana-trench-test-library
                      GTest::gmock
                      GTest::gtest_main)
add_dependencies(build-tests mariana-trench-model-generator-tests)
gtest_discover_tests(mariana-trench-model-generator-tests)

# CMake's `test` target does not build the tests, so we define our own `check` target.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS build-tests)
