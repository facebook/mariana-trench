/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fmt/format.h>
#include <re2/re2.h>

#include <ConcurrentContainers.h>
#include <DexClass.h>
#include <IRCode.h>
#include <Show.h>
#include <SpartaWorkQueue.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

namespace {

constexpr int k_unknown_line = -1;

std::string execute_and_catch_output(
    const std::string& command,
    int& return_code) {
  auto pclose_wrapper = [&return_code](FILE* command) {
    return_code = pclose(command);
  };

  std::string output;

  std::unique_ptr<FILE, decltype(pclose_wrapper)> pipe(
      popen(command.c_str(), "r"), pclose_wrapper);
  if (!pipe) {
    throw std::runtime_error(
        fmt::format("Unable to open pipe to command `{}`.", command));
  }
  std::array<char, 128> buffer;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    output += buffer.data();
  }

  // Function scope ensures the unique pointer goes out of scope and
  // thus ensures invoking the close wrapper
  return output;
}

} // namespace

Positions::Positions() {}

Positions::Positions(const Options& options, const DexStoresVector& stores) {
  if (options.skip_source_indexing()) {
    // Create a dummy path for all methods.
    for (auto& scope : DexStoreClassesIterator(stores)) {
      walk::parallel::methods(scope, [&](DexMethod* method) {
        std::string method_signature = show(method);
        std::string class_name =
            method_signature.substr(0, method_signature.find(";"));
        class_name = class_name.substr(0, class_name.find("$")) + ";";
        if (class_name.size() > 2) {
          class_name = class_name.substr(1, class_name.size() - 2);
        }
        std::string path = class_name + ".java";
        method_to_path_.emplace(method, paths_.insert(path).first);
      });
    }
  } else {
    // Find all Java/Kotlin files in the source root directory.
    Timer paths_timer;
    LOG(2,
        "Finding files to index in `{}`...",
        options.source_root_directory());

    auto current_path = boost::filesystem::current_path();
    boost::filesystem::current_path(options.source_root_directory());

    std::string hg_command =
        "hg files --include=**.java --include=**.kt --include=**.mustache";
    std::string find_command =
        "find . -type f \\( -iname \\*.java -o -iname \\*.kt -iname \\*.mustache \\)";

    int return_code = -1;
    std::string output;
    output = execute_and_catch_output(hg_command, return_code);

    if (return_code != EXIT_SUCCESS) {
      WARNING(
          1,
          "Source directory is not a mercurial repository. Trying `find` to discover files.");
      output = execute_and_catch_output(find_command, return_code);
      if (return_code != EXIT_SUCCESS) {
        ERROR(1, "`find` failed, no source file will be indexed.");
      }
    }

    std::vector<std::string> paths;
    boost::split(paths, output, boost::is_any_of("\n"));

    LOG(2,
        "Found {} files in {:.2f}s.",
        paths.size(),
        paths_timer.duration_in_seconds());

    // Find top-level classes in files.
    Timer index_timer;
    LOG(2, "Indexing classes...");

    std::atomic<std::size_t> iteration(0);
    ConcurrentMap<std::string, std::string> class_to_path;
    re2::RE2 package_regex("^package\\s+([^;]+)(?:;|$)");
    re2::RE2 class_regex(
        "^\\s*(?:/\\*.*\\*/)?\\s*(?:public|internal)?\\s*(?:abstract|final)?\\s*(?:class|enum|interface|object)\\s+([A-z0-9]+)");
    std::unordered_set<std::string> skipped_package_prefixes = {
        "android/",
    };

    auto exclude_directories = options.source_exclude_directories();
    for (auto& exclude_directory : exclude_directories) {
      if (!boost::ends_with(exclude_directory, "/")) {
        exclude_directory.push_back('/');
      }
    }

    auto queue = sparta::work_queue<std::string*>(
        [&](std::string* path) {
          iteration++;
          if (iteration % 10000 == 0) {
            LOG(2, "Indexed {} of {} files.", iteration.load(), paths.size());
          }

          if (boost::starts_with(*path, "./")) {
            // Remove the `./` prefix added by `find`.
            path->erase(0, 2);
          }
          if (path->empty()) {
            return;
          }
          for (const auto& exclude_directory : exclude_directories) {
            if (boost::starts_with(*path, exclude_directory)) {
              return;
            }
          }

          std::optional<std::string> package = std::nullopt;

          std::ifstream stream(*path);
          std::string line;
          while (std::getline(stream, line)) {
            re2::StringPiece package_match;
            // Using capturing groups with `re2` is very slow, so we only
            // capture if we know the regex matches. This gives a huge
            // performance boost.
            if (!package && re2::RE2::PartialMatch(line, package_regex) &&
                re2::RE2::PartialMatch(line, package_regex, &package_match)) {
              package = package_match.as_string();
              boost::replace_all(*package, ".", "/");
              if (std::any_of(
                      skipped_package_prefixes.begin(),
                      skipped_package_prefixes.end(),
                      [&](const auto& skipped_prefix) {
                        return boost::starts_with(*package, skipped_prefix);
                      })) {
                LOG(3, "Skipping module `{}` at `{}`...", *package, *path);
                return;
              }
              if (boost::ends_with(*path, ".kt")) {
                auto pos = path->find_last_of("/");
                if (pos != std::string::npos) {
                  auto filename = path->substr(pos + 1, path->size() - pos - 4);
                  class_to_path.emplace(
                      fmt::format("L{}/{}Kt;", *package, filename), *path);
                }
              }
            }

            re2::StringPiece class_match;
            if (package && re2::RE2::PartialMatch(line, class_regex) &&
                re2::RE2::PartialMatch(line, class_regex, &class_match)) {
              class_to_path.emplace(
                  fmt::format("L{}/{};", *package, class_match), *path);
            }
          }
        },
        sparta::parallel::default_num_threads());
    for (auto& path : paths) {
      queue.add_item(&path);
    }
    queue.run_all();

    boost::filesystem::current_path(current_path);

    LOG(2,
        "Indexed {} top-level classes in {:.2f}s.",
        class_to_path.size(),
        index_timer.duration_in_seconds());

    Timer method_paths_timer;
    LOG(2, "Indexing method paths...");

    for (auto& scope : DexStoreClassesIterator(stores)) {
      walk::parallel::methods(scope, [&](DexMethod* method) {
        std::string method_signature = show(method);
        std::string class_name =
            method_signature.substr(0, method_signature.find(";"));
        class_name = class_name.substr(0, class_name.find("$")) + ";";
        std::string path = class_to_path.get(class_name, /* default */ "");

        if (!path.empty()) {
          method_to_path_.emplace(method, paths_.insert(path).first);
        }
      });
    }

    LOG(2,
        "Indexed {} method paths in {:.2f}s.",
        method_to_path_.size(),
        method_paths_timer.duration_in_seconds());
  }

  Timer method_lines_timer;
  LOG(2, "Indexing method lines...");

  // Index first lines of methods because building the control flow graph will
  // destroy them.
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(scope, [&](DexMethod* method) {
      const auto* code = method->get_code();
      if (!code) {
        return;
      }

      for (const auto& instruction : *code) {
        if (instruction.type == MFLOW_POSITION) {
          const auto* instruction_position = instruction.pos.get();
          if (instruction_position) {
            // Assume the method signature is on the previous line.
            int line =
                std::max(static_cast<int>(instruction_position->line) - 1, 0);
            method_to_line_.emplace(method, line);
          }
          break;
        }
      }
    });
  }

  LOG(2,
      "Indexed {} method lines in {:.2f}s.",
      method_to_line_.size(),
      method_lines_timer.duration_in_seconds());
}

const Position* Positions::get(
    const DexMethod* method,
    const DexPosition* position,
    std::optional<Root> port,
    const IRInstruction* instruction) const {
  mt_assert(method != nullptr);

  const std::string* path = method_to_path_.get(method, /* default */ nullptr);

  int line = k_unknown_line;
  if (position) {
    line = position->line;
  } else {
    line = method_to_line_.get(method, /* default */ k_unknown_line);
  }

  return positions_.insert(Position(path, line, port, instruction)).first;
}

const Position* Positions::get(
    const Method* method,
    const DexPosition* position,
    std::optional<Root> port,
    const IRInstruction* instruction) const {
  return get(method->dex_method(), position, port, instruction);
}

const Position* Positions::get(
    const DexMethod* method,
    int line,
    std::optional<Root> port,
    const IRInstruction* instruction,
    int start,
    int end) const {
  mt_assert(method != nullptr);

  const std::string* path = method_to_path_.get(method, /* default */ nullptr);

  return positions_.insert(Position(path, line, port, instruction, start, end))
      .first;
}

const Position* Positions::get(
    const std::optional<std::string>& path,
    int line,
    std::optional<Root> port,
    const IRInstruction* instruction,
    int start,
    int end) const {
  auto position = Position(
      /* path */ path ? paths_.insert(*path).first : nullptr,
      /* line */ line,
      /* port */ port,
      /* instruction */ instruction,
      /* start */ start,
      /* end */ end);
  return positions_.insert(position).first;
}

const Position* Positions::get(
    const Position* position,
    std::optional<Root> port,
    const IRInstruction* instruction) const {
  auto new_position = Position(
      /* path */ position->path() ? paths_.insert(*position->path()).first
                                  : nullptr,
      /* line */ position->line(),
      /* port */ port,
      /* instruction */ instruction);
  return positions_.insert(new_position).first;
}

const Position*
Positions::get(const Position* position, int line, int start, int end) const {
  auto new_position = Position(
      /* path */ position->path() ? paths_.insert(*position->path()).first
                                  : nullptr,
      /* line */ line,
      /* port */ position->port(),
      /* instruction */ position->instruction(),
      /* start */ start,
      /* end */ end);
  return positions_.insert(new_position).first;
};

const Position* Positions::unknown() const {
  return positions_.insert(Position(nullptr, k_unknown_line)).first;
}

} // namespace marianatrench
