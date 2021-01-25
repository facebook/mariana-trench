/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <SpartaWorkQueue.h>

#include <mariana-trench/Highlights.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

namespace {

using Bounds = Highlights::Bounds;
enum class FrameType { Source, Sink };

Bounds remove_surrounding_whitespace(Bounds bounds, const std::string& line) {
  auto new_start = bounds.start;
  auto new_end = bounds.end;
  while (new_start < bounds.end && std::isspace(line[new_start])) {
    new_start++;
  }
  while (new_end > new_start && std::isspace(line[new_end])) {
    new_end--;
  }
  return {bounds.line, new_start, new_end};
}

Bounds get_argument_bounds(
    ParameterPosition callee_parameter_position,
    ParameterPosition first_parameter_position,
    const std::vector<std::string>& lines,
    const Bounds& callee_name_bounds) {
  auto current_parameter_position = first_parameter_position;
  std::size_t line_number = callee_name_bounds.line;
  std::string current_line = lines[line_number - 1];
  std::size_t arguments_start = callee_name_bounds.end + 2;
  if (arguments_start > current_line.length() - 1) {
    if (line_number == lines.size()) {
      return callee_name_bounds;
    }
    line_number++;
    current_line = lines[line_number - 1];
    arguments_start = 0;
  }
  std::size_t end = current_line.length() - 1;
  int balanced_parentheses_counter = 1;

  while (line_number <= lines.size()) {
    current_line = lines[line_number - 1];
    end = current_line.length() - 1;
    for (auto i = arguments_start; i < current_line.length(); i++) {
      if (current_line[i] == '(') {
        balanced_parentheses_counter++;
      } else if (current_line[i] == ')') {
        balanced_parentheses_counter--;
      } else if (current_line[i] == ',' && balanced_parentheses_counter == 1) {
        if (current_parameter_position == callee_parameter_position) {
          end = i - 1;
          break;
        }
        current_parameter_position++;
        if (current_parameter_position == callee_parameter_position) {
          arguments_start = i + 1;
        }
      }
      if (balanced_parentheses_counter == 0) {
        end = i - 1;
        break;
      }
    }
    mt_assert(current_parameter_position <= callee_parameter_position);
    auto highlighted_portion =
        current_line.substr(arguments_start, end - arguments_start + 1);
    if (balanced_parentheses_counter == 0 ||
        (callee_parameter_position == current_parameter_position &&
         std::any_of(
             std::begin(highlighted_portion),
             std::end(highlighted_portion),
             [](unsigned char c) { return std::isalpha(c); }))) {
      break;
    }

    line_number++;
    arguments_start = 0;
  }
  // In either of these cases, we have failed to find the argument
  if (current_parameter_position < callee_parameter_position ||
      line_number > lines.size()) {
    return callee_name_bounds;
  }
  Bounds highlight_bounds = {
      static_cast<int>(line_number),
      static_cast<int>(arguments_start),
      static_cast<int>(end)};
  return remove_surrounding_whitespace(highlight_bounds, current_line);
}

Bounds get_callee_this_parameter_bounds(
    const std::string& line,
    const Bounds& callee_name_bounds) {
  auto callee_start = callee_name_bounds.start;
  if (callee_start - 1 < 0 || line[callee_start - 1] != '.') {
    return callee_name_bounds;
  }
  auto start = callee_start - 1;
  while (start >= 1 && !std::isspace(line[start - 1])) {
    start--;
  }
  if (start != callee_start - 1) {
    return {callee_name_bounds.line, start, callee_start - 2};
  }
  // If the current line doesn't contain the argument, just highlight the
  // callee so that we don't highlight a previous chained method call, etc
  return callee_name_bounds;
}

Bounds get_local_position_bounds(
    const Position& local_position,
    const std::vector<std::string>& lines) {
  std::string line;
  try {
    // Subtracting 1 below as lines in files are counted starting at 1
    line = lines.at(local_position.line() - 1);
  } catch (const std::out_of_range&) {
    WARNING(
        3,
        "Trying to access line {} of a file with {} lines",
        local_position.line(),
        lines.size());
    return {local_position.line(), 0, 0};
  }

  return {local_position.line(), 0, 0};
}

LocalPositionSet augment_local_positions(
    const LocalPositionSet& local_positions,
    const std::vector<std::string>& lines,
    const Context& context) {
  if (local_positions.is_bottom() || local_positions.is_top()) {
    return local_positions;
  }
  auto new_local_positions = LocalPositionSet();
  for (const auto* local_position : local_positions.elements()) {
    if (!local_position->path() || !local_position->instruction() ||
        local_position->port() == std::nullopt) {
      new_local_positions.add(local_position);
      continue;
    }
    auto bounds = get_local_position_bounds(*local_position, lines);
    new_local_positions.add(context.positions->get(
        local_position, bounds.line, bounds.start, bounds.end));
  }
  return new_local_positions;
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

  const auto* callee = frame.callee();
  mt_assert(callee != nullptr);
  auto bounds = Highlights::get_highlight_bounds(
      callee, lines, position->line(), frame.callee_port());
  return Frame(
      frame.kind(),
      frame.callee_port(),
      callee,
      context.positions->get(position, bounds.line, bounds.start, bounds.end),
      frame.distance(),
      frame.origins(),
      frame.inferred_features(),
      frame.user_features(),
      augment_local_positions(frame.local_positions(), lines, context));
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

} // namespace

Bounds Highlights::get_highlight_bounds(
    const Method* callee,
    const std::vector<std::string>& lines,
    int callee_line_number,
    const AccessPath& callee_port) {
  std::string line;
  try {
    // Subtracting 1 below as lines in files are counted starting at 1
    line = lines.at(callee_line_number - 1);
  } catch (const std::out_of_range&) {
    WARNING(
        3,
        "Trying to access line {} of a file with {} lines",
        callee_line_number,
        lines.size());
    return {callee_line_number, 0, 0};
  }

  const auto& callee_name = callee->get_name();
  auto callee_start = line.find(callee_name + "(");
  if (callee_start == std::string::npos) {
    return {callee_line_number, 0, 0};
  }
  std::size_t callee_end =
      std::min(callee_start + callee_name.length() - 1, line.length() - 1);
  Bounds callee_name_bounds = {
      callee_line_number,
      static_cast<int>(callee_start),
      static_cast<int>(callee_end)};
  if (!callee_port.root().is_argument()) {
    return callee_name_bounds;
  }
  if (callee_port.root().parameter_position() == 0 && !callee->is_static()) {
    return get_callee_this_parameter_bounds(line, callee_name_bounds);
  }
  return get_argument_bounds(
      callee_port.root().parameter_position(),
      callee->first_parameter_index(),
      lines,
      callee_name_bounds);
}

void Highlights::augment_positions(Registry& registry, const Context& context) {
  auto current_path = boost::filesystem::current_path();
  boost::filesystem::current_path(context.options->source_root_directory());

  auto issue_files_to_methods = get_issue_files_to_methods(context, registry);
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
          const auto old_model = registry.get(method);
          auto new_model = old_model;
          new_model.set_issues(
              augment_issue_positions(old_model.issues(), lines, context));
          new_model.set_sinks(
              augment_taint_tree_positions(old_model.sinks(), lines, context));
          new_model.set_generations(augment_taint_tree_positions(
              old_model.generations(), lines, context));
          new_model.set_parameter_sources(augment_taint_tree_positions(
              old_model.parameter_sources(), lines, context));
          registry.set(new_model);
        }
      });

  for (const auto& [filepath, _] : issue_files_to_methods) {
    file_queue.add_item(filepath);
  }
  file_queue.run_all();

  boost::filesystem::current_path(current_path);
}

} // namespace marianatrench
