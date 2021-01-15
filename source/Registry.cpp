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

enum class FrameType { Source, Sink };

struct Bound {
  int start;
  int end;
};

Bound get_highlight_bounds(
    const Method* callee,
    const std::string& line,
    const AccessPath& callee_port) {
  const auto& callee_name = callee->get_name();
  auto callee_start = line.find(callee_name + "(");
  if (callee_start == std::string::npos) {
    return {0, 0};
  }
  if (!callee_port.root().is_argument() ||
      (!callee->is_static() && callee_port.root().parameter_position() == 0)) {
    std::size_t end =
        std::min(callee_start + callee_name.length() - 1, line.length() - 1);
    return {static_cast<int>(callee_start), static_cast<int>(end)};
  }
  // Highlight the parameter or till the end of the line if the parameter is
  // not on this line
  auto callee_parameter_position = callee_port.root().parameter_position();
  int balanced_parentheses_counter = 1;
  std::size_t current_parameter_position = callee->first_parameter_index();
  std::size_t arguments_start =
      std::min(callee_start + callee_name.length() + 1, line.length() - 1);
  std::size_t end = line.length() - 1;

  for (auto i = arguments_start; i < line.length(); i++) {
    if (line[i] == '(') {
      balanced_parentheses_counter++;
    } else if (line[i] == ')') {
      balanced_parentheses_counter--;
    } else if (line[i] == ',' && balanced_parentheses_counter == 1) {
      if (current_parameter_position == callee_parameter_position) {
        end = i - 1;
        break;
      }
      current_parameter_position++;
      if (current_parameter_position == callee_parameter_position) {
        // The argument usually starts one space after the comma (current
        // position) so add 2
        arguments_start = i + 2;
      }
    }
    if (balanced_parentheses_counter == 0) {
      end = i - 1;
      break;
    }
  }
  return {static_cast<int>(arguments_start), static_cast<int>(end)};
}

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
        3,
        "Trying to access line {} of a file with {} lines",
        position->line(),
        lines.size());
    return frame;
  }

  const auto* callee = frame.callee();
  mt_assert(callee != nullptr);
  auto bounds = get_highlight_bounds(callee, current_line, frame.callee_port());
  return Frame(
      frame.kind(),
      frame.callee_port(),
      callee,
      context.positions->get(position, bounds.start, bounds.end),
      frame.distance(),
      frame.origins(),
      frame.inferred_features(),
      frame.user_features(),
      frame.local_positions());
}

Taint augment_taint_positions(
    Taint taint,
    const std::vector<std::string>& lines,
    const Context& context) {
  taint.map([&](FrameSet& frames) {
    auto new_frames = FrameSet::bottom();
    for (const auto& frame : frames) {
      new_frames.add(augment_frame_position(frame, lines, context));
    }
    frames = std::move(new_frames);
  });
  return taint;
}

TaintAccessPathTree augment_taint_tree_positions(
    TaintAccessPathTree taint_tree,
    const std::vector<std::string>& lines,
    const Context& context) {
  taint_tree.map([&](Taint& taint) {
    auto new_taint = augment_taint_positions(taint, lines, context);
    taint = std::move(new_taint);
  });
  return taint_tree;
}

IssueSet augment_issue_positions(
    IssueSet issues,
    const std::vector<std::string>& lines,
    const Context& context) {
  issues.map([&](Issue& issue) {
    Issue augmented_issue = Issue(
        augment_taint_positions(issue.sources(), lines, context),
        augment_taint_positions(issue.sinks(), lines, context),
        issue.rule(),
        issue.position());
    issue = std::move(augmented_issue);
  });
  return issues;
}

/**
 * Run a fixpoint to go through all the sources(or sinks) involved in issues
 * to add all the relevant files and methods to the mapping
 * `issue_files_to_methods`.
 */
void get_frames_files_to_methods(
    ConcurrentMap<const std::string*, std::unordered_set<const Method*>>&
        issue_files_to_methods,
    const ConcurrentSet<const Frame*>& frames,
    const Context& context,
    const Registry& registry,
    FrameType frame_type) {
  auto frames_to_check = std::make_unique<ConcurrentSet<const Frame*>>(frames);
  auto seen_frames = std::make_unique<ConcurrentSet<const Frame*>>(frames);

  while (frames_to_check->size() != 0) {
    auto new_frames_to_check = std::make_unique<ConcurrentSet<const Frame*>>();
    auto queue = sparta::work_queue<const Frame*>([&](const Frame* frame) {
      const auto* callee = frame->callee();
      if (!callee) {
        return;
      }
      auto callee_model = registry.get(callee);
      const auto& callee_port = frame->callee_port();
      const auto* method_position = context.positions->get(callee);
      if (method_position && method_position->path()) {
        issue_files_to_methods.update(
            method_position->path(),
            [&](const std::string* /*filepath*/,
                std::unordered_set<const Method*>& methods,
                bool) { methods.emplace(callee); });
      }

      Taint taint;
      if (frame_type == FrameType::Source) {
        taint = callee_model.generations().raw_read(callee_port).root();
      } else if (frame_type == FrameType::Sink) {
        taint = callee_model.sinks().raw_read(callee_port).root();
      }
      for (const auto& frame_set : taint) {
        for (const auto& callee_frame : frame_set) {
          if (callee_frame.is_leaf() || !seen_frames->emplace(&callee_frame)) {
            continue;
          }
          new_frames_to_check->emplace(&callee_frame);
        }
      }
    });
    for (const auto& frame : *frames_to_check) {
      queue.add_item(frame);
    }
    queue.run_all();
    frames_to_check = std::move(new_frames_to_check);
  }
}

/**
 * Returns a map of files involved in issues to the set of all the methods
 * defined in that file that are involved in issues. This way, when finding
 * highlights, we can open each file only once and only consider the
 * relevant methods in that file.
 */
ConcurrentMap<const std::string*, std::unordered_set<const Method*>>
get_issue_files_to_methods(const Context& context, const Registry& registry) {
  ConcurrentMap<const std::string*, std::unordered_set<const Method*>>
      issue_files_to_methods;
  ConcurrentSet<const Frame*> sources;
  ConcurrentSet<const Frame*> sinks;

  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto model = registry.get(method);
    if (model.issues().size() == 0) {
      return;
    }
    for (const auto& issue : model.issues()) {
      for (const auto& sink_frame_set : issue.sinks()) {
        for (const auto& sink : sink_frame_set) {
          if (!sink.is_leaf()) {
            sinks.emplace(&sink);
          }
        }
      }

      for (const auto& source_frame_set : issue.sources()) {
        for (const auto& source : source_frame_set) {
          if (!source.is_leaf()) {
            sources.emplace(&source);
          }
        }
      }
    }
    auto* method_position = context.positions->get(method);
    if (!method_position || !method_position->path()) {
      return;
    }
    issue_files_to_methods.update(
        method_position->path(),
        [&](const std::string* /*filepath*/,
            std::unordered_set<const Method*>& methods,
            bool) { methods.emplace(method); });
  });
  for (const auto* method : *context.methods) {
    queue.add_item(method);
  }
  queue.run_all();

  get_frames_files_to_methods(
      issue_files_to_methods, sources, context, registry, FrameType::Source);
  get_frames_files_to_methods(
      issue_files_to_methods, sinks, context, registry, FrameType::Sink);
  return issue_files_to_methods;
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

  auto issue_files_to_methods = get_issue_files_to_methods(context_, *this);
  auto file_queue =
      sparta::work_queue<const std::string*>([&](const std::string* filepath) {
        std::ifstream stream(*filepath);
        if (stream.bad()) {
          WARNING(1, "File {} was not found.", *filepath);
          return;
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line)) {
          lines.push_back(line);
        }
        for (const auto* method : issue_files_to_methods.get(filepath, {})) {
          const auto old_model = models_.at(method);
          auto new_model = old_model;
          new_model.set_issues(
              augment_issue_positions(old_model.issues(), lines, context_));
          new_model.set_sinks(
              augment_taint_tree_positions(old_model.sinks(), lines, context_));
          new_model.set_generations(augment_taint_tree_positions(
              old_model.generations(), lines, context_));
          new_model.set_parameter_sources(augment_taint_tree_positions(
              old_model.parameter_sources(), lines, context_));
          models_.insert_or_assign(std::pair(method, new_model));
        }
      });

  for (const auto& [filepath, _] : issue_files_to_methods) {
    file_queue.add_item(filepath);
  }
  file_queue.run_all();

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
