/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/string_file.hpp>
#include <fmt/format.h>
#include <re2/re2.h>

#include <SpartaWorkQueue.h>
#include <Walkers.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Statistics.h>

namespace marianatrench {

Registry::Registry(Context& context, const DexStoresVector& /* stores */)
    : context_(context) {
  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) { set(Model(method, context)); },
      sparta::parallel::default_num_threads());
  for (const auto* method : *context.methods) {
    queue.add_item(method);
  }
  queue.run_all();
}

Registry::Registry(Context& context, const std::vector<Model>& models)
    : context_(context) {
  for (const auto& model : models) {
    join_with(model);
  }
}

Registry::Registry(Context& context, const Json::Value& models_value)
    : context_(context) {
  for (const auto& value : JsonValidation::null_or_array(models_value)) {
    const auto* method = Method::from_json(value["method"], context);
    mt_assert(method != nullptr);
    join_with(Model::from_json(method, value, context));
  }
}

Registry Registry::load(
    Context& context,
    const Options& options,
    const DexStoresVector& stores,
    const std::vector<Model>& generated_models) {
  // Create a registry with the generated models
  Registry registry(context, generated_models);

  // Load models json input
  for (const auto& models_path : options.models_paths()) {
    registry.join_with(
        Registry(context, JsonValidation::parse_json_file(models_path)));
  }

  // Add a default model for methods that don't have one
  registry.add_default_models();

  return registry;
}

void Registry::add_default_models() {
  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) {
        models_.insert(std::make_pair(method, Model(method, context_)));
      },
      sparta::parallel::default_num_threads());
  for (const auto* method : *context_.methods) {
    queue.add_item(method);
  }
  queue.run_all();
}

Model Registry::get(const Method* method) const {
  if (!method) {
    throw std::runtime_error("Trying to get model for the `null` method");
  }

  try {
    return models_.at(method);
  } catch (const std::out_of_range&) {
    throw std::runtime_error(fmt::format(
        "Trying to get model for untracked method `{}`.", show(method)));
  }
}

void Registry::set(const Model& model) {
  models_.insert_or_assign(std::make_pair(model.method(), model));
}

std::size_t Registry::models_size() const {
  return models_.size();
}

std::size_t Registry::issues_size() const {
  std::size_t result = 0;
  for (const auto& entry : models_) {
    result += entry.second.issues().size();
  }
  return result;
}

void Registry::join_with(const Model& model) {
  auto* method = model.method();
  auto iterator = models_.find(method);
  if (iterator != models_.end()) {
    iterator->second.join_with(model);
  } else {
    models_.insert(std::make_pair(method, model));
  }
}

void Registry::join_with(const Registry& other) {
  for (const auto& other_model : other.models_) {
    join_with(other_model.second);
  }
}

namespace {

Frame augment_frame_position(
    const Frame& frame,
    const std::vector<std::string>& lines,
    const Context& context) {
  if (frame.is_leaf()) {
    return frame;
  }
  const auto* position = frame.call_position();
  mt_assert(position != nullptr);

  std::string current_line;
  try {
    // Subtracting 1 below as lines in files are counted starting at 1
    current_line = lines.at(position->line() - 1);
  } catch (const std::out_of_range&) {
    WARNING(
        1,
        "Trying to access line {} of a file with {} lines",
        position->line(),
        lines.size());
    return frame;
  }

  const auto* callee = frame.callee();
  mt_assert(callee != nullptr);
  const auto& callee_name = callee->get_name();
  std::size_t start = current_line.find(callee_name + "(");
  if (start == std::string::npos) {
    return frame;
  }
  std::size_t end =
      std::min(start + callee_name.length() - 1, current_line.length() - 1);

  return Frame(
      frame.kind(),
      frame.callee_port(),
      callee,
      context.positions->get(position, start, end),
      frame.distance(),
      frame.origins(),
      frame.features(),
      frame.local_positions());
}

bool is_valid_generation(const Frame& generation, const Registry& registry) {
  if (generation.is_leaf()) {
    return true;
  }
  auto model = registry.get(generation.callee());
  auto sources = model.generations().raw_read(generation.callee_port()).root();
  return std::any_of(
      sources.begin(), sources.end(), [&](const FrameSet& frames) {
        return frames.kind() == generation.kind();
      });
}

bool is_valid_sink(const Frame& sink, const Registry& registry) {
  if (sink.is_leaf()) {
    return true;
  }
  auto model = registry.get(sink.callee());
  auto sinks = model.sinks().raw_read(sink.callee_port()).root();
  return std::any_of(sinks.begin(), sinks.end(), [&](const FrameSet& frames) {
    return frames.kind() == sink.kind();
  });
}

TaintAccessPathTree cull_collapsed_generations(
    TaintAccessPathTree generation_tree,
    const Registry& registry) {
  generation_tree.map([&](Taint& generation_taint) {
    generation_taint.map([&](FrameSet& generations) {
      generations.filter([&](const Frame& generation) {
        return is_valid_generation(generation, registry);
      });
    });
  });
  return generation_tree;
}

TaintAccessPathTree cull_collapsed_sinks(
    TaintAccessPathTree sink_tree,
    const Registry& registry) {
  sink_tree.map([&](Taint& sink_taint) {
    sink_taint.map([&](FrameSet& sinks) {
      sinks.filter(
          [&](const Frame& sink) { return is_valid_sink(sink, registry); });
    });
  });
  return sink_tree;
}

IssueSet cull_collapsed_issues(IssueSet issues, const Registry& registry) {
  issues.map([&](Issue& issue) {
    issue.filter_sources([&](const Frame& generation) {
      return is_valid_generation(generation, registry);
    });
    issue.filter_sinks(
        [&](const Frame& sink) { return is_valid_sink(sink, registry); });
  });
  return issues;
}

} // namespace

void Registry::postprocess_remove_collapsed_traces() {
  // We need to compute a decreasing fixpoint since we might remove empty
  // generations or sinks that are referenced in other models.

  auto methods = std::make_unique<ConcurrentSet<const Method*>>();
  for (const auto& [method, _] : models_) {
    methods->insert(method);
  }

  while (methods->size() > 0) {
    auto new_methods = std::make_unique<ConcurrentSet<const Method*>>();

    auto queue = sparta::work_queue<const Method*>(
        [&, this](const Method* method) {
          const auto old_model = models_.at(method);

          auto model = old_model;
          model.set_generations(
              cull_collapsed_generations(model.generations(), *this));
          model.set_sinks(cull_collapsed_sinks(model.sinks(), *this));
          model.set_issues(cull_collapsed_issues(model.issues(), *this));

          if (!old_model.leq(model)) {
            for (const auto* dependency :
                 context_.dependencies->dependencies(method)) {
              new_methods->insert(dependency);
            }
          }

          models_.insert_or_assign(std::make_pair(method, model));
        },
        sparta::parallel::default_num_threads());
    for (const auto* method : *methods) {
      queue.add_item(method);
    }
    queue.run_all();
    methods = std::move(new_methods);
  }
}

void Registry::augment_positions() {
  auto current_path = boost::filesystem::current_path();
  boost::filesystem::current_path(context_.options->source_root_directory());

  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) {
        const auto old_model = models_.at(method);
        if (old_model.issues().size() == 0) {
          return;
        }
        auto* method_position = context_.positions->get(method);
        if (!method_position || !method_position->path()) {
          LOG(3,
              "Method {} does not have a position or a path recorded",
              method->get_name());
          return;
        }
        const auto& filepath = *(method_position->path());
        std::ifstream stream(filepath);
        if (stream.bad()) {
          WARNING(1, "File {} was not found.", filepath);
          return;
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }

        IssueSet augmented_issues = {};
        for (const auto& issue : old_model.issues()) {
          Taint sources = Taint::bottom();
          Taint sinks = Taint::bottom();
          for (const auto& frames : issue.sources()) {
            for (const auto& frame : frames) {
              sources.add(augment_frame_position(frame, lines, context_));
            }
          }
          for (const auto& frames : issue.sinks()) {
            for (const auto& frame : frames) {
              sinks.add(augment_frame_position(frame, lines, context_));
            }
          }
          Issue augmented_issue =
              Issue(sources, sinks, issue.rule(), issue.position());
          augmented_issues.add(std::move(augmented_issue));
        }

        auto new_model = old_model;
        new_model.set_issues(augmented_issues);
        models_.insert_or_assign(std::pair(method, new_model));
      },
      sparta::parallel::default_num_threads());

  for (const auto& [method, _] : models_) {
    queue.add_item(method);
  }
  queue.run_all();

  boost::filesystem::current_path(current_path);
}

void Registry::dump_metadata(const boost::filesystem::path& path) const {
  auto value = Json::Value(Json::objectValue);

  auto codes = Json::Value(Json::objectValue);
  for (const auto* rule : *context_.rules) {
    codes[std::to_string(rule->code())] = Json::Value(rule->name());
  }
  value["codes"] = codes;

  auto rules = Json::Value(Json::arrayValue);
  for (const auto* rule : *context_.rules) {
    rules.append(rule->to_json());
  }
  value["rules"] = rules;

  auto statistics = context_.statistics->to_json();
  statistics["issues"] = issues_size();
  statistics["methods_analyzed"] = models_.size();
  statistics["methods_without_code"] = Json::Value(
      std::count_if(models_.begin(), models_.end(), [](const auto& model) {
        return model.first->get_code() == nullptr;
      }));
  statistics["methods_skipped"] = Json::Value(
      std::count_if(models_.begin(), models_.end(), [](const auto& model) {
        return model.second.skip_analysis();
      }));
  value["stats"] = statistics;

  value["filename_spec"] = Json::Value("model@*.json");
  value["repo_root"] =
      Json::Value(context_.options->repository_root_directory());
  value["root"] = Json::Value(context_.options->source_root_directory());
  value["tool"] = Json::Value("mariana_trench");
  value["version"] = Json::Value("0.1");

  boost::filesystem::save_string_file(path, value.toStyledString());
}

std::string Registry::dump_models() const {
  Json::FastWriter writer;
  std::string string;
  string.append("// @");
  string.append("generated\n");
  for (const auto& model : models_) {
    string.append(writer.write(model.second.to_json(context_)));
  }
  return string;
}

Json::Value Registry::models_to_json() const {
  auto models_value = Json::Value(Json::arrayValue);
  for (auto model : models_) {
    models_value.append(model.second.to_json(context_));
  }
  return models_value;
}

void Registry::dump_models(
    const boost::filesystem::path& path,
    const std::size_t batch_size) const {
  // Remove existing model files under this directory.
  for (auto& file : boost::filesystem::directory_iterator(path)) {
    const auto& file_path = file.path();
    if (boost::filesystem::is_regular_file(file_path) &&
        boost::starts_with(file_path.filename().string(), "model@")) {
      boost::filesystem::remove(file_path);
    }
  }

  std::vector<Model> models;
  for (const auto& model : models_) {
    models.push_back(model.second);
  }

  const auto total_batch = models_.size() / batch_size + 1;
  const auto padded_total_batch = fmt::format("{:0>5}", total_batch);

  auto queue = sparta::work_queue<std::size_t>(
      [&](std::size_t batch) {
        // Construct a valid sharded file name for SAPP.
        const auto padded_batch = fmt::format("{:0>5}", batch);
        const auto batch_path = path /
            ("model@" + padded_batch + "-of-" + padded_total_batch + ".json");

        std::fstream batch_stream(batch_path.native(), std::ios_base::out);
        if (!batch_stream.is_open()) {
          ERROR(1, "Unable to write models to `{}`.", batch_path.native());
          return;
        }
        batch_stream << "// @"
                     << "generated\n";

        // Write the current batch of models to file.
        Json::FastWriter writer;
        for (std::size_t i = batch_size * batch;
             i < batch_size * (batch + 1) && i < models.size();
             i++) {
          batch_stream << writer.write(models[i].to_json(context_));
        }
        batch_stream.close();
      },
      sparta::parallel::default_num_threads());

  for (std::size_t batch = 0; batch < total_batch; batch++) {
    queue.add_item(batch);
  }
  queue.run_all();

  LOG(1, "Wrote models to {} shards.", total_batch);
}

} // namespace marianatrench
