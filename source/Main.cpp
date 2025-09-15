/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <signal.h>

#include <exception>
#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>

#include <AggregateException.h>
#include <DebugUtils.h>
#include <RedexContext.h>

#include <mariana-trench/ExitCode.h>
#include <mariana-trench/GlobalRedexContext.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/SanitizersOptions.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>

int main(int argc, char* argv[]) {
  signal(SIGSEGV, crash_backtrace_handler);
  signal(SIGABRT, crash_backtrace_handler);
#ifndef _MSC_VER
  signal(SIGBUS, crash_backtrace_handler);
#endif

  namespace program_options = boost::program_options;
  program_options::options_description options;
  options.add_options()("help,h", "Show help dialog.")(
      "config,c",
      program_options::value<std::string>()->required(),
      "Path to the JSON configuration file.");

  auto tool = marianatrench::MarianaTrench();
  tool.add_options(options);

  try {
    program_options::variables_map variables;
    program_options::store(
        program_options::parse_command_line(argc, argv, options), variables);
    if (variables.count("help")) {
      std::cerr << options;
      return 0;
    }
    if (!variables.count("config")) {
      std::cerr << "error: missing parameter `--config`.\n";
      std::cerr << "Usage: " << argv[0] << " --config <json_config_file>\n";
      return ExitCode::invalid_argument_error(
          "No JSON configuration file provided.");
    }

    marianatrench::GlobalRedexContext redex_context(
        /* allow_class_duplicates */ true);
    tool.run(variables);
  } catch (const aggregate_exception& aggregate_exception) {
    std::cerr << "Caught multiple exceptions:" << std::endl;
    for (const auto& exception : aggregate_exception.m_exceptions) {
      try {
        std::rethrow_exception(exception);
      } catch (const std::exception& rethrown_exception) {
        marianatrench::EventLogger::log_event(
            "redex_error", rethrown_exception.what(), /* verbosity_level */ 1);
        std::cerr << rethrown_exception.what() << std::endl;
      }
    }
    return ExitCode::redex_error(aggregate_exception.what());
  } catch (const marianatrench::ModelGeneratorError& exception) {
    return ExitCode::model_generator_error(exception.what());
  } catch (const marianatrench::LifecycleMethodsJsonError& exception) {
    return ExitCode::lifecycle_error(exception.what());
  } catch (const marianatrench::LifecycleMethodValidationError& exception) {
    return ExitCode::lifecycle_error(exception.what());
  } catch (const marianatrench::ShimGeneratorError& exception) {
    return ExitCode::shim_generator_error(exception.what());
  } catch (const std::invalid_argument& exception) {
    return ExitCode::invalid_argument_error(exception.what());
  } catch (const std::runtime_error& exception) {
    return ExitCode::mariana_trench_error(exception.what());
  } catch (const std::logic_error& exception) {
    return ExitCode::mariana_trench_error(exception.what());
  } catch (const std::exception& exception) {
    return ExitCode::error(exception.what());
  }

  return ExitCode::success();
}
