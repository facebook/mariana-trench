# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19.3...4.0 FATAL_ERROR)
project(MarianaTrench VERSION 0.1 LANGUAGES CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")
endif()
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
set(LINK_TYPE "Shared" CACHE STRING "Choose the type of linkage")
set_property(CACHE LINK_TYPE PROPERTY STRINGS "Shared" "Static")
if (LINK_TYPE STREQUAL "Static")
  # Force cmake to only find static libraries.
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(BUILD_SHARED_LIBS OFF)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_MULTITHREADED ON)
  if (NOT APPLE)
    # On macOS, we need to link dynamically to libc, so we cannot use `-static`.
    set(CMAKE_EXE_LINKER_FLAGS "-static")
    set(Boost_USE_STATIC_RUNTIME ON)
  endif()
  if (UNIX)
    # Use `-pthread` instead of `-lpthread` in addition to `-static` on Linux, otherwise linking fails.
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  endif()
elseif (LINK_TYPE STREQUAL "Shared")
  if (UNIX)
    # On Linux with Linuxbrew, preserve the runtime path.
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  endif()
endif()

option(SANITIZER "Enable address and undefined sanitizers" OFF)
if (SANITIZER)
    add_compile_options(
        "$<$<CONFIG:DEBUG>:-fsanitize=address>"
        "$<$<CONFIG:DEBUG>:-fsanitize=undefined>"
        "$<$<CONFIG:DEBUG>:-fno-omit-frame-pointer>"
    )
    add_link_options(
        "$<$<CONFIG:DEBUG>:-fsanitize=address>"
        "$<$<CONFIG:DEBUG>:-fsanitize=undefined>"
    )
endif()

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Link type: ${LINK_TYPE}")
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

if (POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif ()

# Dependencies
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Boost 1.75.0 REQUIRED
             COMPONENTS filesystem regex program_options iostreams thread
             CONFIG)
# CMake's provided `FindGTest.cmake` does not define GMock.
# We use `CONFIG` to find the `GTestConfig.cmake` provided by gtest, which properly defines GMock.
find_package(GTest 1.10.0 REQUIRED CONFIG)
find_package(JsonCpp 1.9.6 REQUIRED)
find_package(fmt 7.1.2...<10.0.0 REQUIRED)
find_package(re2 REQUIRED)
find_package(Redex REQUIRED)

# Create a header directory header-tree/mariana-trench/ pointing to source/
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/header-tree")
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/source" "${CMAKE_CURRENT_BINARY_DIR}/header-tree/mariana-trench" SYMBOLIC)

# Enable/disable warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-Wall" "-Wextra" "-Wno-nullability-completeness")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options("-Wall" "-Wextra" "-Wno-nonnull-compare")
else()
  message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} is not currently supported.")
endif()

option(FMT_DEPRECATED_OSTREAM "Enable automatic std::ostream insertion operator (operator<<) discovery" OFF)
if (FMT_DEPRECATED_OSTREAM OR ("${fmt_VERSION}" VERSION_GREATER "8"))
  add_compile_options("-DFMT_DEPRECATED_OSTREAM=ON")
endif()

# Targets
file(GLOB library_sources
     "source/*.def"
     "source/*.cpp"
     "source/constraints/*.cpp"
     "source/model-generator/*.cpp"
     "source/shim-generator/*.cpp"
     "source/type-analysis/*.cpp")
list(FILTER library_sources EXCLUDE REGEX ".*/source/Main.cpp")
add_library(mariana-trench-library STATIC ${library_sources})
target_link_libraries(mariana-trench-library PUBLIC
                      Redex::LibTool
                      Threads::Threads
                      ZLIB::ZLIB
                      Boost::regex
                      Boost::program_options
                      Boost::iostreams
                      Boost::thread
                      GTest::gtest
                      JsonCpp::JsonCpp
                      fmt::fmt
                      re2::re2
                      Boost::filesystem)
target_include_directories(mariana-trench-library PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/header-tree")

add_executable(mariana-trench-binary "source/Main.cpp")
target_link_libraries(mariana-trench-binary PUBLIC mariana-trench-library)
install(TARGETS mariana-trench-binary DESTINATION "bin")

function(generate_shim_wrapper)
  set(REPOSITORY_ROOT "${CMAKE_SOURCE_DIR}")
  set(BUILD_ROOT "${CMAKE_BINARY_DIR}")
  configure_file("scripts/cmake_shim.py" "${CMAKE_CURRENT_BINARY_DIR}/mariana-trench")
endfunction()
generate_shim_wrapper()

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
gtest_discover_tests(mariana-trench-unit-tests PROPERTIES DISCOVERY_TIMEOUT 600)

file(GLOB model_generator_test_sources "source/model-generator/tests/*.cpp")
add_executable(mariana-trench-model-generator-tests EXCLUDE_FROM_ALL ${model_generator_test_sources})
target_link_libraries(mariana-trench-model-generator-tests PUBLIC
                      mariana-trench-test-library
                      GTest::gmock
                      GTest::gtest_main)
add_dependencies(build-tests mariana-trench-model-generator-tests)
gtest_discover_tests(mariana-trench-model-generator-tests PROPERTIES DISCOVERY_TIMEOUT 600)

add_executable(mariana-trench-integration-test-models EXCLUDE_FROM_ALL
               "source/tests/integration/models/IntegrationTest.cpp")
target_link_libraries(mariana-trench-integration-test-models PUBLIC
                      mariana-trench-test-library
                      GTest::gmock
                      GTest::gtest_main)
add_dependencies(build-tests mariana-trench-integration-test-models)
gtest_discover_tests(mariana-trench-integration-test-models PROPERTIES DISCOVERY_TIMEOUT 600)

find_package(Java)
find_package(AndroidSDK)

if (NOT Java_FOUND)
  message(STATUS "Integration tests are disabled because Java could not be found.")
elseif (NOT AndroidSDK_FOUND)
  message(STATUS "Integration tests are disable because Android SDK could not be found.")
else()
  include(UseJava)

  # Some test cases fail if switching to D8 as D8 seems to do more local optimization, etc.
  # Add tests that require the use of LEGACY DX too. When adding tests, please document the
  # differences between the two dexers.
  set(LEGACY_DX_TESTS "end-to-end-field_tito_fp;")

  function(generate_integration_test_code root_directory test_directory name)
    # Target that compiles the source to a .jar
    set(CMAKE_JAVA_COMPILE_FLAGS -source 7 -target 7 -nowarn)
    file(GLOB test_sources "${test_directory}/*.java")
    file(GLOB android_sources "source/tests/integration/${root_directory}/android_classes/*.java")
    file(GLOB library_sources "source/tests/integration/${root_directory}/library_classes/*.java")
    add_jar(java-class-${name}
            SOURCES ${test_sources} ${android_sources} ${library_sources})
    set_property(TARGET java-class-${name} PROPERTY EXCLUDE_FROM_ALL TRUE)

    # Target that compiles the .jar to a .dex
    if (name IN_LIST LEGACY_DX_TESTS)
      message(STATUS "USING DX ${name}")
      add_custom_command(OUTPUT java-dex-${name}.dex
                        COMMAND
                          ${ANDROID_DX}
                          --core-library
                          --dex
                          --output=java-dex-${name}.dex
                          $<TARGET_PROPERTY:java-class-${name},JAR_FILE>
                        DEPENDS java-class-${name})
    else()
      message(STATUS "USING D8 ${name}")
      add_custom_command(OUTPUT java-dex-${name}.dex
                        COMMAND
                          mkdir java-dex-${name}-tmp
                        COMMAND
                          ${ANDROID_D8}
                          $<TARGET_PROPERTY:java-class-${name},JAR_FILE>
                          --output java-dex-${name}-tmp/
                        COMMAND
                          mv java-dex-${name}-tmp/classes.dex java-dex-${name}.dex
                        COMMAND
                          rm -r java-dex-${name}-tmp
                        DEPENDS java-class-${name})
    endif()
    add_custom_target(java-dex-${name} DEPENDS java-dex-${name}.dex)
    set_property(TARGET java-dex-${name} PROPERTY EXCLUDE_FROM_ALL TRUE)
  endfunction()

  function(generate_integration_test directory)
    add_executable(mariana-trench-integration-test-${directory} EXCLUDE_FROM_ALL
                   "source/tests/integration/${directory}/IntegrationTest.cpp")
    target_link_libraries(mariana-trench-integration-test-${directory} PUBLIC
                          mariana-trench-test-library
                          GTest::gmock
                          GTest::gtest_main)
    add_dependencies(build-tests mariana-trench-integration-test-${directory})
    set(test_properties "")
    file(GLOB codes "source/tests/integration/${directory}/code/*")
    foreach(path ${codes})
      get_filename_component(name "${path}" NAME)
      generate_integration_test_code("${directory}" "${path}" "${directory}-${name}")
      add_dependencies(mariana-trench-integration-test-${directory} "java-dex-${directory}-${name}")
      list(APPEND test_properties ENVIRONMENT "${name}=java-dex-${directory}-${name}.dex")
    endforeach()
    gtest_discover_tests(mariana-trench-integration-test-${directory} PROPERTIES "${test_properties}" DISCOVERY_TIMEOUT 600)
  endfunction()

  generate_integration_test(end-to-end)
  generate_integration_test(json-model-generator)
endif()

if(INCLUDE_INTEGRATION_TESTS)
  list(JOIN INCLUDE_INTEGRATION_TESTS " |" include_tests)
  message(STATUS "Only integration tests matching the following regex will be included: \"${include_tests}\"")
else()
  set(include_tests ".*")
endif()

if(EXCLUDE_INTEGRATION_TESTS)
  list(JOIN EXCLUDE_INTEGRATION_TESTS " |" exclude_tests)
  message(STATUS "Integration tests matching the following regex will be excluded: \"${exclude_tests}\"")
else()
  set(exclude_tests "")
endif()

# CMake's `test` target does not build the tests, so we define our own `check` target.
add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} -R "\"${include_tests}\"" -E "\"${exclude_tests}\""
        DEPENDS build-tests)
