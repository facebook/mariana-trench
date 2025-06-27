/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ListingCommands.h>

#include <iostream>
#include <filesystem>
#include <boost/algorithm/string.hpp>
#include <fmt/format.h>

#include <mariana-trench/Rules.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/TransformsFactory.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <set>

namespace marianatrench {

std::vector<std::string> ListingCommands::parse_paths_list(const std::string& input) {
  std::vector<std::string> paths;
  std::string paths_str = input;
  boost::algorithm::split(paths, paths_str, boost::is_any_of(","), boost::token_compress_on);
  
  // Remove empty entries and trim whitespace
  paths.erase(
    std::remove_if(paths.begin(), paths.end(), [](std::string& path) {
      boost::algorithm::trim(path);
      return path.empty();
    }),
    paths.end());
  
  return paths;
}

void ListingCommands::list_all_rules(Context& context) {
  std::cout << "=== All Rules ===" << std::endl;
  
  auto rules_paths = context.options->rules_paths();
  
  for (const auto& rules_path : rules_paths) {
    std::cout << "\nRules from: " << rules_path << std::endl;
    
    try {
      // Parse JSON file directly since Rules::from_json_file doesn't exist
      Json::Value rules_json = JsonReader::parse_json_file(rules_path);
      
      for (const auto& rule_json : rules_json) {
        auto rule = Rule::from_json(rule_json, context);
        std::cout << fmt::format("  Code: {}", rule->code()) << std::endl;
        std::cout << fmt::format("  Name: {}", rule->name()) << std::endl;
        std::cout << fmt::format("  Description: {}", rule->description()) << std::endl;
        
        // Check if it's a SourceSinkRule to access sources and sinks
        if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
          std::cout << "  Sources: ";
          for (const auto* source : source_sink_rule->source_kinds()) {
            std::cout << source->to_trace_string() << " ";
          }
          std::cout << std::endl;
          
          std::cout << "  Sinks: ";
          for (const auto* sink : source_sink_rule->sink_kinds()) {
            std::cout << sink->to_trace_string() << " ";
          }
          std::cout << std::endl;
        }
        std::cout << std::endl;
      }
    } catch (const std::exception& e) {
      std::cout << "  Error loading rules: " << e.what() << std::endl;
    }
  }
}

void ListingCommands::list_all_model_generators(Context& context) {
  std::cout << "=== All Model Generators ===" << std::endl;
  
  // List from configuration
  auto config = context.options->model_generators_configuration();
  std::cout << "\nFrom configuration:" << std::endl;
  for (const auto& gen_config : config) {
    std::cout << "  " << gen_config.name() << std::endl;
  }
  
  // List from search paths
  auto search_paths = context.options->model_generator_search_paths();
  std::cout << "\nFrom search paths:" << std::endl;
  for (const auto& search_path : search_paths) {
    std::cout << "\nIn " << search_path << ":" << std::endl;
    
    if (std::filesystem::exists(search_path) && std::filesystem::is_directory(search_path)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(search_path)) {
        if (entry.is_regular_file() && 
            (entry.path().extension() == ".models" || entry.path().extension() == ".json")) {
          std::cout << "  " << entry.path().filename().string() << std::endl;
        }
      }
    }
  }
}

void ListingCommands::list_all_model_generators_in_rules(Context& context) {
  std::cout << "=== Model Generators Used in Rules ===" << std::endl;
  
  auto rules_paths = context.options->rules_paths();
  std::set<std::string> used_generators;
  
  for (const auto& rules_path : rules_paths) {
    std::cout << "\nAnalyzing rules from: " << rules_path << std::endl;
    
    try {
      Json::Value rules_json = JsonReader::parse_json_file(rules_path);
      
      for (const auto& rule_json : rules_json) {
        auto rule = Rule::from_json(rule_json, context);
        
        // Extract any model generator references from rule definitions
        if (rule_json.isMember("model_generators")) {
          for (const auto& generator : rule_json["model_generators"]) {
            if (generator.isString()) {
              used_generators.insert(generator.asString());
            }
          }
        }
      }
    } catch (const std::exception& e) {
      std::cout << "  Error analyzing rules: " << e.what() << std::endl;
    }
  }
  
  std::cout << "\nModel generators referenced in rules:" << std::endl;
  for (const auto& generator : used_generators) {
    std::cout << "  " << generator << std::endl;
  }
  
  if (used_generators.empty()) {
    std::cout << "  No model generators directly referenced in rules." << std::endl;
  }
}

void ListingCommands::list_all_kinds(Context& context) {
  std::cout << "=== All Kinds ===" << std::endl;
  
  std::set<std::string> all_kinds;
  
  // Get kinds from rules
  auto rules_paths = context.options->rules_paths();
  for (const auto& rules_path : rules_paths) {
    try {
      Json::Value rules_json = JsonReader::parse_json_file(rules_path);
      
      for (const auto& rule_json : rules_json) {
        auto rule = Rule::from_json(rule_json, context);
        
        if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
          for (const auto* source : source_sink_rule->source_kinds()) {
            all_kinds.insert(source->to_trace_string());
          }
          for (const auto* sink : source_sink_rule->sink_kinds()) {
            all_kinds.insert(sink->to_trace_string());
          }
        }
      }
    } catch (const std::exception& e) {
      std::cout << "Error processing rules from " << rules_path << ": " << e.what() << std::endl;
    }
  }
  
  // Get kinds from model generators
  auto search_paths = context.options->model_generator_search_paths();
  for (const auto& search_path : search_paths) {
    if (std::filesystem::exists(search_path) && std::filesystem::is_directory(search_path)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(search_path)) {
        if (entry.is_regular_file() && 
            (entry.path().extension() == ".models" || entry.path().extension() == ".json")) {
          try {
            Json::Value model_json = JsonReader::parse_json_file(entry.path());
            // Extract kinds from model generators (simplified analysis)
            if (model_json.isMember("model_generators")) {
              for (const auto& generator : model_json["model_generators"]) {
                if (generator.isMember("model")) {
                  const auto& model = generator["model"];
                  if (model.isMember("sources")) {
                    for (const auto& source : model["sources"]) {
                      if (source.isMember("kind")) {
                        all_kinds.insert(source["kind"].asString());
                      }
                    }
                  }
                  if (model.isMember("sinks")) {
                    for (const auto& sink : model["sinks"]) {
                      if (sink.isMember("kind")) {
                        all_kinds.insert(sink["kind"].asString());
                      }
                    }
                  }
                }
              }
            }
          } catch (const std::exception& e) {
            // Skip files that can't be parsed
          }
        }
      }
    }
  }
  
  std::cout << "\nAll kinds found:" << std::endl;
  for (const auto& kind : all_kinds) {
    std::cout << "  " << kind << std::endl;
  }
}

void ListingCommands::list_all_kinds_in_rules(Context& context) {
  std::cout << "=== Kinds Used in Rules ===" << std::endl;
  
  auto rules_paths = context.options->rules_paths();
  std::map<std::string, std::set<std::string>> kind_usage; // kind -> rules using it
  
  for (const auto& rules_path : rules_paths) {
    std::cout << "\nAnalyzing rules from: " << rules_path << std::endl;
    
    try {
      Json::Value rules_json = JsonReader::parse_json_file(rules_path);
      
      for (const auto& rule_json : rules_json) {
        auto rule = Rule::from_json(rule_json, context);
        
        if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
          for (const auto* source : source_sink_rule->source_kinds()) {
            kind_usage[source->to_trace_string()].insert(rule->name());
          }
          for (const auto* sink : source_sink_rule->sink_kinds()) {
            kind_usage[sink->to_trace_string()].insert(rule->name());
          }
        }
      }
    } catch (const std::exception& e) {
      std::cout << "  Error analyzing rules: " << e.what() << std::endl;
    }
  }
  
  std::cout << "\nKinds and their usage in rules:" << std::endl;
  for (const auto& [kind, rules] : kind_usage) {
    std::cout << fmt::format("  {}: used in {} rule(s)", kind, rules.size()) << std::endl;
    for (const auto& rule : rules) {
      std::cout << fmt::format("    - {}", rule) << std::endl;
    }
  }
}

void ListingCommands::list_all_filters(Context& /* context */) {
  std::cout << "=== All Filters ===" << std::endl;
  
  std::cout << "\nBuilt-in filters:" << std::endl;
  std::cout << "  • source-filter: Filters sources based on criteria" << std::endl;
  std::cout << "  • sink-filter: Filters sinks based on criteria" << std::endl;
  std::cout << "  • transform-filter: Filters transform operations" << std::endl;
  std::cout << "  • method-filter: Filters methods from analysis" << std::endl;
  std::cout << "  • class-filter: Filters classes from analysis" << std::endl;
  
  std::cout << "\nNote: Custom filters can be defined in configuration files." << std::endl;
}

void ListingCommands::list_all_features(Context& /* context */) {
  std::cout << "=== All Features ===" << std::endl;
  
  std::cout << "\nBuilt-in features:" << std::endl;
  std::cout << "  • always-type: Always includes type information in traces" << std::endl;
  std::cout << "  • via-type-of: Adds type information of specific arguments" << std::endl;
  std::cout << "  • via-value-of: Adds value information of specific arguments" << std::endl;
  std::cout << "  • via-cast: Indicates data type casting operations" << std::endl;
  std::cout << "  • first-field: Marks first field access in a chain" << std::endl;
  std::cout << "  • first-index: Marks first array/list index access" << std::endl;
  std::cout << "  • broadening: Indicates where path broadening occurred" << std::endl;
  std::cout << "  • return-via-parameter: Data returned through parameter modification" << std::endl;
  std::cout << "  • intent-routing: Android Intent routing patterns" << std::endl;
  std::cout << "  • copy-arguments: Argument copying behavior" << std::endl;
  std::cout << "  • origin-field: Original field where taint was introduced" << std::endl;
  std::cout << "  • array-allocation: Array allocation tracking" << std::endl;
  std::cout << "  • breadcrumbs: Execution path tracking" << std::endl;
  
  std::cout << "\nCustom features can be defined in model generators and configuration files." << std::endl;
}

void ListingCommands::list_all_lifecycles(Context& context) {
  std::cout << "=== All Lifecycles ===" << std::endl;
  
  auto lifecycles_paths = context.options->lifecycles_paths();
  
  for (const auto& lifecycle_path : lifecycles_paths) {
    std::cout << "\nLifecycles from: " << lifecycle_path << std::endl;
    
    try {
      Json::Value lifecycle_json = JsonReader::parse_json_file(lifecycle_path);
      
      for (const auto& lifecycle : lifecycle_json) {
        if (lifecycle.isMember("name")) {
          std::cout << fmt::format("  Name: {}", lifecycle["name"].asString()) << std::endl;
        }
        if (lifecycle.isMember("methods")) {
          std::cout << "  Methods:" << std::endl;
          for (const auto& method : lifecycle["methods"]) {
            if (method.isString()) {
              std::cout << fmt::format("    - {}", method.asString()) << std::endl;
            }
          }
        }
        std::cout << std::endl;
      }
    } catch (const std::exception& e) {
      std::cout << "  Error loading lifecycles: " << e.what() << std::endl;
    }
  }
  
  if (lifecycles_paths.empty()) {
    std::cout << "\nNo lifecycle paths configured. Default Android lifecycles may be used." << std::endl;
    std::cout << "\nCommon Android lifecycles:" << std::endl;
    std::cout << "  • Activity: onCreate, onStart, onResume, onPause, onStop, onDestroy" << std::endl;
    std::cout << "  • Fragment: onAttach, onCreate, onCreateView, onStart, onResume, onPause, onStop, onDestroyView, onDestroy, onDetach" << std::endl;
    std::cout << "  • Service: onCreate, onStartCommand, onBind, onDestroy" << std::endl;
    std::cout << "  • BroadcastReceiver: onReceive" << std::endl;
  }
}

} // namespace marianatrench 