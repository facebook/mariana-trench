/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <sparta/WorkQueue.h>

#include <DexUtil.h>
#include <MethodUtil.h>

#include <mariana-trench/FrameType.h>
#include <mariana-trench/Highlights.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

namespace {
using Bounds = Highlights::Bounds;
using FileLines = Highlights::FileLines;

/*
 * Given a line and column that is assumed not to be a whitespace's position,
 * returns a Bounds object describing the location of the next non-whitespace
 * character, if one exists before EOF.
 */
std::optional<Bounds> get_next_non_whitespace_position(
    const FileLines& lines,
    std::size_t current_line_number,
    std::size_t current_column) {
  current_column++;
  while (lines.has_line_number(current_line_number)) {
    const auto& line = lines.line(current_line_number);
    while (current_column < line.length()) {
      if (!std::isspace(line[current_column])) {
        return Bounds(
            {static_cast<int>(current_line_number),
             static_cast<int>(current_column),
             static_cast<int>(current_column)});
      }
      current_column++;
    }
    current_column = 0;
    current_line_number++;
  }
  return std::nullopt;
}

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
    const FileLines& lines,
    const Bounds& callee_name_bounds) {
  auto current_parameter_position = first_parameter_position;
  std::size_t line_number = callee_name_bounds.line;
  auto current_line = lines.line(line_number);
  std::size_t arguments_start = callee_name_bounds.end + 2;
  if (arguments_start > current_line.length() - 1) {
    if (!lines.has_line_number(line_number + 1)) {
      return callee_name_bounds;
    }
    line_number++;
    current_line = lines.line(line_number);
    arguments_start = 0;
  }
  std::size_t end = current_line.length() - 1;
  int balanced_parentheses_counter = 1;

  while (lines.has_line_number(line_number)) {
    current_line = lines.line(line_number);
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
      !lines.has_line_number(line_number)) {
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

std::optional<std::string_view> get_class_name(const DexMethod* callee) {
  const auto& class_name = callee->get_class()->get_name()->str();
  auto length = class_name.length();
  if (length <= 2 || class_name[length - 1] != ';' ||
      class_name[length - 2] == '/') {
    return std::nullopt;
  }
  auto i = class_name.rfind('/', /* start */ length - 2);
  if (i == std::string::npos) {
    return std::nullopt;
  }
  return class_name.substr(i + 1, length - i - 2);
}

Bounds get_iput_local_position_bounds(
    const FileLines& lines,
    std::string_view field_name,
    int line_number) {
  auto line = lines.line(line_number);
  auto field_start = line.find(field_name);
  if (field_start == std::string::npos) {
    return {line_number, 0, 0};
  }
  Bounds field_name_bounds = {
      line_number,
      static_cast<int>(field_start),
      static_cast<int>(field_start + field_name.length() - 1)};
  // The next non-whitespace character read must be an = sign
  auto next_character = get_next_non_whitespace_position(
      lines, line_number, field_name_bounds.end);
  if (!next_character ||
      lines.line(next_character->line)[next_character->start] != '=') {
    return field_name_bounds;
  }
  auto assignee = get_next_non_whitespace_position(
      lines, next_character->line, next_character->start);
  if (!assignee) {
    return field_name_bounds;
  }

  line = lines.line(assignee->line);
  Bounds highlight_bounds = {
      static_cast<int>(assignee->line),
      static_cast<int>(assignee->start),
      static_cast<int>(line.length() - 1)};
  return remove_surrounding_whitespace(highlight_bounds, line);
}

LocalPositionSet augment_local_positions(
    const LocalPositionSet& local_positions,
    const FileLines& lines,
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
    auto bounds = Highlights::get_local_position_bounds(*local_position, lines);
    new_local_positions.add(context.positions->get(
        local_position->path(), bounds.line, bounds.start, bounds.end));
  }
  return Highlights::filter_overlapping_highlights(new_local_positions);
}

const Position* augment_frame_position(
    const Method* callee,
    const AccessPath* callee_port,
    const Position* position,
    const FileLines& lines,
    const Context& context) {
  mt_assert(position != nullptr);
  mt_assert(callee != nullptr);
  mt_assert(callee_port != nullptr);

  auto bounds = Highlights::get_callee_highlight_bounds(
      callee->dex_method(), lines, position->line(), callee_port->root());
  return context.positions->get(
      position->path(), bounds.line, bounds.start, bounds.end);
}

Taint augment_taint_positions(
    Taint taint,
    const FileLines& lines,
    const Context& context) {
  return taint.update_non_declaration_positions(
      [&lines, &context](
          const Method* callee,
          const AccessPath* MT_NULLABLE callee_port,
          const Position* MT_NULLABLE position) {
        if (position == nullptr) {
          // Unknown position.
          return position;
        }

        if (callee_port == nullptr) {
          // Cannot determine position if callee port is unknown. Return the
          // original position without instruction and port.
          return context.positions->get(
              position->path(),
              position->line(),
              position->start(),
              position->end());
        }
        return augment_frame_position(
            callee, callee_port, position, lines, context);
      },
      [&lines, &context](const LocalPositionSet& local_positions) {
        return augment_local_positions(local_positions, lines, context);
      });
}

TaintAccessPathTree augment_taint_tree_positions(
    TaintAccessPathTree taint_tree,
    const FileLines& lines,
    const Context& context) {
  taint_tree.transform([&lines, &context](Taint taint) {
    return augment_taint_positions(taint, lines, context);
  });
  return taint_tree;
}

IssueSet augment_issue_positions(
    const IssueSet& issues,
    const FileLines& lines,
    const Context& context) {
  IssueSet result{};

  for (const auto& issue : issues) {
    // We do not use IssueSet::transform() and instead build a new set here. The
    // transform() method of the underlying GroupHashedSetAbstractDomain is only
    // safe if the elements are updated without affecting the grouping. But
    // since we remove the instruction and port from position, the issue
    // grouping can potentially change.
    const auto* issue_position = issue.position();
    mt_assert(issue_position != nullptr);
    result.add(Issue(
        augment_taint_positions(issue.sources(), lines, context),
        augment_taint_positions(issue.sinks(), lines, context),
        issue.rule(),
        issue.callee(),
        issue.sink_index(),
        context.positions->get(
            issue_position->path(),
            issue_position->line(),
            issue_position->start(),
            issue_position->end())));
  }

  return result;
}

/**
 * Run a fixpoint to go through all the sources(or sinks) involved in issues
 * to add all the relevant files and methods to the mapping
 * `issue_files_to_methods`.
 */
void get_frames_files_to_methods(
    ConcurrentMap<const std::string*, std::unordered_set<const Method*>>&
        issue_files_to_methods,
    const ConcurrentSet<const LocalTaint*>& frames,
    const Context& context,
    const Registry& registry,
    FrameType frame_type) {
  auto frames_to_check =
      std::make_unique<ConcurrentSet<const LocalTaint*>>(frames);
  auto seen_frames = std::make_unique<ConcurrentSet<const LocalTaint*>>(frames);

  while (frames_to_check->size() != 0) {
    auto new_frames_to_check =
        std::make_unique<ConcurrentSet<const LocalTaint*>>();
    auto queue =
        sparta::work_queue<const LocalTaint*>([&](const LocalTaint* frame) {
          const auto* callee = frame->callee();
          if (!callee) {
            return;
          }
          auto callee_model = registry.get(callee);
          const auto* callee_port = frame->callee_port();
          const auto* method_position = context.positions->get(callee);
          if (method_position && method_position->path()) {
            issue_files_to_methods.update(
                method_position->path(),
                [callee](
                    const std::string* /*filepath*/,
                    std::unordered_set<const Method*>& methods,
                    bool) { methods.emplace(callee); });
          }

          Taint taint;
          if (frame_type == FrameType::source()) {
            taint = callee_model.generations().raw_read(*callee_port).root();
          } else if (frame_type == FrameType::sink()) {
            taint = callee_model.sinks().raw_read(*callee_port).root();
          }
          taint.visit_local_taint([&seen_frames, &new_frames_to_check](
                                      const LocalTaint& local_taint) {
            if (local_taint.callee() == nullptr ||
                !seen_frames->emplace(&local_taint)) {
              return;
            }
            new_frames_to_check->emplace(&local_taint);
          });
        });
    for (const auto* taint : UnorderedIterable(*frames_to_check)) {
      queue.add_item(taint);
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
  ConcurrentSet<const LocalTaint*> sources;
  ConcurrentSet<const LocalTaint*> sinks;

  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto model = registry.get(method);
    if (model.issues().size() == 0) {
      return;
    }
    for (const auto& issue : model.issues()) {
      const auto& issue_sinks = issue.sinks();
      issue_sinks.visit_local_taint([&sinks](const LocalTaint& local_taint) {
        if (!local_taint.call_info().is_leaf()) {
          sinks.emplace(&local_taint);
        }
      });

      const auto& issue_sources = issue.sources();
      issue_sources.visit_local_taint(
          [&sources](const LocalTaint& local_taint) {
            if (!local_taint.call_info().is_leaf()) {
              sources.emplace(&local_taint);
            }
          });
    }
    auto* method_position = context.positions->get(method);
    if (!method_position || !method_position->path()) {
      return;
    }
    issue_files_to_methods.update(
        method_position->path(),
        [method](
            const std::string* /*filepath*/,
            std::unordered_set<const Method*>& methods,
            bool) { methods.emplace(method); });
  });
  for (const auto* method : *context.methods) {
    queue.add_item(method);
  }
  queue.run_all();

  get_frames_files_to_methods(
      issue_files_to_methods, sources, context, registry, FrameType::source());
  get_frames_files_to_methods(
      issue_files_to_methods, sinks, context, registry, FrameType::sink());
  return issue_files_to_methods;
}

} // namespace

FileLines::FileLines(std::ifstream& stream) {
  std::string line;
  while (std::getline(stream, line)) {
    lines_.push_back(line);
  }
}

bool FileLines::has_line_number(std::size_t index) const {
  return index >= 1 && index <= lines_.size();
}

const std::string& FileLines::line(std::size_t index) const {
  mt_assert(has_line_number(index));
  return lines_[index - 1];
}

std::size_t FileLines::size() const {
  return lines_.size();
}

Bounds Highlights::get_local_position_bounds(
    const Position& local_position,
    const FileLines& lines) {
  auto line_number = local_position.line();
  Bounds empty_bounds = {line_number, 0, 0};
  if (!lines.has_line_number(line_number)) {
    WARNING(
        3,
        "Trying to access line {} of a file with {} lines",
        line_number,
        lines.size());
    return empty_bounds;
  }
  mt_assert(local_position.instruction() != nullptr);
  if (opcode::is_an_iput(local_position.instruction()->opcode())) {
    const auto field_name =
        local_position.instruction()->get_field()->get_name()->str();
    return get_iput_local_position_bounds(lines, field_name, line_number);
  }
  if (opcode::is_an_invoke(local_position.instruction()->opcode())) {
    const auto* callee_dex_method = local_position.instruction()->get_method();
    if (!callee_dex_method->as_def()) {
      return empty_bounds;
    }
    mt_assert(local_position.port() != std::nullopt);
    return get_callee_highlight_bounds(
        callee_dex_method->as_def(),
        lines,
        local_position.line(),
        *local_position.port());
  }
  return empty_bounds;
}

Bounds Highlights::get_callee_highlight_bounds(
    const DexMethod* callee,
    const FileLines& lines,
    int callee_line_number,
    const Root& callee_port_root) {
  if (!lines.has_line_number(callee_line_number)) {
    WARNING(
        3,
        "Trying to access line {} of a file with {} lines",
        callee_line_number,
        lines.size());
    return {callee_line_number, 0, 0};
  }
  auto line = lines.line(callee_line_number);
  auto callee_name = callee->get_name()->str_copy();
  if (method::is_init(callee)) {
    auto class_name = get_class_name(callee);
    if (!class_name) {
      return {callee_line_number, 0, 0};
    }
    callee_name = "new " + *class_name;
  }
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
  if (!callee_port_root.is_argument() ||
      (method::is_init(callee) && callee_port_root.parameter_position() == 0)) {
    return callee_name_bounds;
  }
  bool is_static = ::is_static(callee);
  if (callee_port_root.parameter_position() == 0 && !is_static) {
    return get_callee_this_parameter_bounds(line, callee_name_bounds);
  }
  return get_argument_bounds(
      callee_port_root.parameter_position(),
      is_static ? 0 : 1,
      lines,
      callee_name_bounds);
}

LocalPositionSet Highlights::filter_overlapping_highlights(
    const LocalPositionSet& local_positions) {
  mt_assert(local_positions.is_value());
  std::unordered_map<int, std::vector<const Position*>> grouped_by_line;
  for (const auto* local_position : local_positions.elements()) {
    auto line = local_position->line();
    auto& same_line_positions = grouped_by_line[line];
    if (same_line_positions.empty()) {
      same_line_positions.push_back(local_position);
      continue;
    }
    // No need to replace any existing highlights if the current one is empty
    if (local_position->end() <= 0) {
      continue;
    }
    auto current_start = local_position->start();
    auto current_end = local_position->end();
    std::vector<const Position*> new_positions;
    bool seen_shorter_overlapping_with_current = false;
    for (const auto* position : same_line_positions) {
      if (position->end() <= 0) {
        continue;
      }
      if (!position->overlaps(*local_position)) {
        new_positions.push_back(position);
      } else if (
          current_end - current_start > position->end() - position->start()) {
        new_positions.push_back(position);
        seen_shorter_overlapping_with_current = true;
      }
    }
    if (!seen_shorter_overlapping_with_current) {
      new_positions.push_back(local_position);
    }
    same_line_positions = std::move(new_positions);
  }
  auto new_local_positions = LocalPositionSet();
  for (const auto& [_, positions] : grouped_by_line) {
    for (const auto* position : positions) {
      new_local_positions.add(position);
    }
  }
  return new_local_positions;
}

void Highlights::augment_positions(Registry& registry, const Context& context) {
  auto current_path = std::filesystem::current_path();
  std::filesystem::current_path(context.options->source_root_directory());

  auto issue_files_to_methods = get_issue_files_to_methods(context, registry);
  auto file_queue =
      sparta::work_queue<const std::string*>([&](const std::string* filepath) {
        std::ifstream stream(*filepath);
        if (stream.bad()) {
          WARNING(1, "File {} was not found.", *filepath);
          return;
        }
        auto lines = FileLines(stream);
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

  for (const auto& [filepath, _] : UnorderedIterable(issue_files_to_methods)) {
    file_queue.add_item(filepath);
  }
  file_queue.run_all();

  std::filesystem::current_path(current_path);
}

} // namespace marianatrench
