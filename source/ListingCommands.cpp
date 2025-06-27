/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iostream>
#include <set>
#include <filesystem>

#include <fmt/format.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/ListingCommands.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/SourceSinkRule.h>

namespace marianatrench {

void ListingCommands::run(Context& context) {
  // Execute requested listing commands
  if (context.options->list_all_rules()) {
    list_all_rules(context);
  }
  if (context.options->list_all_model_generators()) {
    list_all_model_generators(context);
  }
  if (context.options->list_all_kinds()) {
    list_all_kinds(context);
  }
  if (context.options->list_all_kinds_in_rules()) {
    list_all_kinds_in_rules(context);
  }

  if (context.options->list_all_lifecycles()) {
    list_all_lifecycles(context);
  }
}

// Extract model generator paths from configuration and search paths
std::vector<std::string> ListingCommands::get_model_generator_paths(Context& context) {
  std::vector<std::string> paths;
  
  // Add configured model generators
  for (const auto& config : context.options->model_generators_configuration()) {
    paths.push_back(config.name());
  }
  
  // Find model generators in search paths
  for (const auto& search_path : context.options->model_generator_search_paths()) {
    try {
      for (auto& entry : std::filesystem::recursive_directory_iterator(search_path)) {
        if (entry.path().extension() == ".models") {
          paths.push_back(entry.path().string());
        }
      }
    } catch (const std::exception& e) {
      std::cout << "Error scanning " << search_path << ": " << e.what() << std::endl;
    }
  }
  
  return paths;
}

void ListingCommands::list_all_rules(Context& context) {
  std::cout << "=== All Rules ===" << std::endl;
  
  // Iterate through loaded rules
  if (context.rules) {
    for (const auto* rule : *context.rules) {
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
  } else {
    std::cout << "No rules loaded in context." << std::endl;
  }
}

void ListingCommands::list_all_model_generators(Context& context) {
  std::cout << "=== All Model Generators ===" << std::endl;
  
  // Get all model generator paths
  auto generator_paths = get_model_generator_paths(context);
  
  std::cout << "\nConfigured generators:" << std::endl;
  for (const auto& config : context.options->model_generators_configuration()) {
    std::cout << fmt::format("  • {}", config.name()) << std::endl;
  }
  
  std::cout << "\nModel generator files found:" << std::endl;
  for (const auto& path : generator_paths) {
    if (path.length() >= 7 && path.substr(path.length() - 7) == ".models") {
      std::cout << fmt::format("  • {}", path) << std::endl;
    }
  }
  
  std::cout << "\nBuilt-in generators:" << std::endl;
  // Built-in model generators
  std::cout << "  • BroadcastReceiverGenerator" << std::endl;
  std::cout << "  • ContentProviderGenerator" << std::endl;
  std::cout << "  • ServiceSourceGenerator" << std::endl;
  std::cout << "  • TaintInTaintThisGenerator" << std::endl;
  std::cout << "  • TaintInTaintOutGenerator" << std::endl;
  std::cout << "  • BuilderPatternGenerator" << std::endl;
  std::cout << "  • JoinOverrideGenerator" << std::endl;
  std::cout << "  • ManifestSourceGenerator" << std::endl;
  std::cout << "  • DFASourceGenerator" << std::endl;
}

void ListingCommands::list_all_kinds(Context& context) {
  std::cout << "=== All Kinds ===" << std::endl;
  
  std::set<std::string> all_kinds;
  
  // Extract kinds from loaded rules
  if (context.rules) {
    for (const auto* rule : *context.rules) {
      if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
        for (const auto* source : source_sink_rule->source_kinds()) {
          all_kinds.insert(source->to_trace_string());
        }
        for (const auto* sink : source_sink_rule->sink_kinds()) {
          all_kinds.insert(sink->to_trace_string());
        }
      }
    }
  }
  
  // Get kinds from model generators
  auto generator_paths = get_model_generator_paths(context);
  for (const auto& path : generator_paths) {
    if (path.length() >= 7 && path.substr(path.length() - 7) == ".models") {
      try {
        Json::Value generator_json = JsonReader::parse_json_file(path);
        if (generator_json.isMember("model_generators")) {
          for (const auto& mg : generator_json["model_generators"]) {
            if (mg.isMember("model")) {
              auto model = mg["model"];
              // Extract kinds from model sources and sinks
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
        std::cout << "Error reading " << path << ": " << e.what() << std::endl;
      }
    }
  }
  
  std::cout << "\nFound kinds:" << std::endl;
  for (const auto& kind : all_kinds) {
    std::cout << fmt::format("  • {}", kind) << std::endl;
  }
}

void ListingCommands::list_all_kinds_in_rules(Context& context) {
  std::cout << "=== Kinds Used in Rules ===" << std::endl;
  
  if (context.rules) {
    std::map<std::string, std::vector<int>> source_kinds;
    std::map<std::string, std::vector<int>> sink_kinds;
    
    for (const auto* rule : *context.rules) {
      if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
        for (const auto* source : source_sink_rule->source_kinds()) {
          source_kinds[source->to_trace_string()].push_back(rule->code());
        }
        for (const auto* sink : source_sink_rule->sink_kinds()) {
          sink_kinds[sink->to_trace_string()].push_back(rule->code());
        }
      }
    }
    
    std::cout << "\nSource kinds:" << std::endl;
    for (const auto& [kind, rule_codes] : source_kinds) {
      std::cout << fmt::format("  • {} (used in rules: {})", kind, fmt::join(rule_codes, ", ")) << std::endl;
    }
    
    std::cout << "\nSink kinds:" << std::endl;
    for (const auto& [kind, rule_codes] : sink_kinds) {
      std::cout << fmt::format("  • {} (used in rules: {})", kind, fmt::join(rule_codes, ", ")) << std::endl;
    }
  } else {
    std::cout << "No rules loaded in context." << std::endl;
  }
}



void ListingCommands::list_all_lifecycles(Context& context) {
  std::cout << "=== All Lifecycle Definitions ===" << std::endl;
  
  auto lifecycle_paths = context.options->lifecycles_paths();
  
  for (const auto& lifecycle_path : lifecycle_paths) {
    std::cout << "\nLifecycles from: " << lifecycle_path << std::endl;
    
    try {
      Json::Value lifecycles_json = JsonReader::parse_json_file(lifecycle_path);
      
      for (const auto& lifecycle_json : lifecycles_json) {
        // Parse lifecycle method configuration
        auto lifecycle_method = LifecycleMethod::from_json(lifecycle_json);
        std::cout << fmt::format("  Method: {}", lifecycle_method.method_name()) << std::endl;
        
        // Print the JSON for full details since LifecycleMethod doesn't expose all details
        std::cout << "  Configuration: " << JsonWriter::to_styled_string(lifecycle_json) << std::endl;
        std::cout << std::endl;
      }
    } catch (const std::exception& e) {
      std::cout << "  Error loading lifecycles: " << e.what() << std::endl;
    }
  }
}

} // namespace marianatrench 