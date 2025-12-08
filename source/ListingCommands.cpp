/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iostream>
#include <set>

#include <fmt/format.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/ListingCommands.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/SourceSinkWithExploitabilityRule.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>

namespace marianatrench {

void ListingCommands::run(Context& context) {
  // Execute requested listing commands
  if (context.options->list_all_rules()) {
    list_all_rules(context);
  }
  if (context.options->list_all_model_generators()) {
    list_all_model_generators(context);
  }
  if (context.options->list_all_kinds_in_rules()) {
    list_all_kinds_in_rules(context);
  }
  if (context.options->list_all_lifecycles()) {
    list_all_lifecycles(context);
  }
}

void ListingCommands::list_all_rules(Context& context) {
  std::cout << "=== All Rules ===" << std::endl;

  // Iterate through loaded rules
  mt_assert(context.rules);
  for (const auto* rule : *context.rules) {
    std::cout << fmt::format("Rule: {}", rule->name()) << std::endl;
    std::cout << fmt::format("  Code: {}", rule->code()) << std::endl;
    std::cout << fmt::format("  Description: {}", rule->description())
              << std::endl;

    if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
      std::cout << "  Type: SourceSinkRule" << std::endl;
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
    } else if (
        auto* exploitability_rule =
            rule->as<SourceSinkWithExploitabilityRule>()) {
      std::cout << "  Type: SourceSinkWithExploitabilityRule" << std::endl;
      std::cout << "  Effect Sources: ";
      for (const auto* effect_source :
           exploitability_rule->effect_source_kinds()) {
        std::cout << effect_source->to_trace_string() << " ";
      }
      std::cout << std::endl;
      std::cout << "  Sources: ";
      for (const auto* source : exploitability_rule->source_kinds()) {
        std::cout << source->to_trace_string() << " ";
      }
      std::cout << std::endl;
      std::cout << "  Sinks: ";
      for (const auto* sink : exploitability_rule->sink_kinds()) {
        std::cout << sink->to_trace_string() << " ";
      }
      std::cout << std::endl;
    } else if (auto* multi_rule = rule->as<MultiSourceMultiSinkRule>()) {
      std::cout << "  Type: MultiSourceMultiSinkRule" << std::endl;
      std::cout << "  Multi Sources: " << std::endl;
      for (const auto& [label, kinds] : multi_rule->multi_source_kinds()) {
        std::cout << fmt::format("    {}: ", label);
        for (const auto* kind : kinds) {
          std::cout << kind->to_trace_string() << " ";
        }
        std::cout << std::endl;
      }
    } else {
      std::cout << "  Type: Unknown rule type" << std::endl;
      WARNING(
          1,
          "Unknown rule type for rule '{}' (code {})",
          rule->name(),
          rule->code());
    }
    std::cout << std::endl;
  }
  std::cout << fmt::format("Total rules: {}", context.rules->size())
            << std::endl;
}

void ListingCommands::list_all_model_generators(Context& context) {
  std::cout << "=== All Model Generators ===" << std::endl;

  std::cout << "\nConfigured generators:" << std::endl;
  for (const auto& config : context.options->model_generators_configuration()) {
    std::cout << fmt::format("  - {}", config.name()) << std::endl;
  }

  std::cout << "\nAll available generators (including built-in and JSON):"
            << std::endl;
  auto generators = ModelGeneration::make_model_generators(nullptr, context);
  for (const auto& [name, generator] : generators) {
    std::cout << fmt::format("  - {}", name->identifier()) << std::endl;
  }

  std::cout << fmt::format("\nTotal generators: {}", generators.size())
            << std::endl;
}

void ListingCommands::list_all_kinds_in_rules(Context& context) {
  std::cout << "=== All Kinds in Rules ===" << std::endl;

  std::set<std::string> all_kinds;

  // Extract kinds from rules
  mt_assert(context.rules);
  for (const auto* rule : *context.rules) {
    if (auto* source_sink_rule = rule->as<SourceSinkRule>()) {
      for (const auto* source : source_sink_rule->source_kinds()) {
        all_kinds.insert(source->to_trace_string());
      }
      for (const auto* sink : source_sink_rule->sink_kinds()) {
        all_kinds.insert(sink->to_trace_string());
      }
    } else if (
        auto* exploitability_rule =
            rule->as<SourceSinkWithExploitabilityRule>()) {
      for (const auto* effect_source :
           exploitability_rule->effect_source_kinds()) {
        all_kinds.insert(effect_source->to_trace_string());
      }
      for (const auto* source : exploitability_rule->source_kinds()) {
        all_kinds.insert(source->to_trace_string());
      }
      for (const auto* sink : exploitability_rule->sink_kinds()) {
        all_kinds.insert(sink->to_trace_string());
      }
    } else if (auto* multi_rule = rule->as<MultiSourceMultiSinkRule>()) {
      for (const auto& [label, kinds] : multi_rule->multi_source_kinds()) {
        for (const auto* kind : kinds) {
          all_kinds.insert(kind->to_trace_string());
        }
      }
    }
  }

  std::cout << "\nAll kinds found:" << std::endl;
  for (const auto& kind : all_kinds) {
    std::cout << fmt::format("  - {}", kind) << std::endl;
  }

  std::cout << fmt::format("\nTotal kinds: {}", all_kinds.size()) << std::endl;
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
        std::cout << fmt::format("  Method: {}", lifecycle_method.method_name())
                  << std::endl;

        // Print the JSON for full details since LifecycleMethod doesn't expose
        // all details
        std::cout << "  Configuration: "
                  << JsonWriter::to_styled_string(lifecycle_json) << std::endl;
        std::cout << std::endl;
      }
    } catch (const std::exception& e) {
      std::cout << "  Error loading lifecycles: " << e.what() << std::endl;
    }
  }
}

} // namespace marianatrench
