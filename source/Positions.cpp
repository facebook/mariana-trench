/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>
#include <re2/re2.h>

#include <sparta/WorkQueue.h>

#include <ConcurrentContainers.h>
#include <DexClass.h>
#include <IRCode.h>
#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

namespace {

constexpr int k_unknown_line = -1;

struct GrepoPaths {
  std::vector<std::string> actual_paths;
  std::unordered_map<std::string, std::string> actual_to_repo_paths;
};

// Performance optimization to avoid calling more expensive regex matches on
// every line.
bool maybe_class(const std::string& line) {
  return line.find("class") != std::string::npos ||
      line.find("interface") != std::string::npos ||
      line.find("object") != std::string::npos ||
      line.find("enum") != std::string::npos;
}

GrepoPaths get_grepo_paths(
    std::string command_output,
    Json::Value metadata_json) {
  using split_iterator =
      boost::algorithm::split_iterator<std::string::iterator>;
  std::vector<std::string> actual_paths{};
  std::unordered_map<std::string, std::string> actual_to_repo_paths{};

  for (split_iterator iterator = boost::algorithm::make_split_iterator(
           command_output, boost::first_finder("\n", boost::is_equal()));
       iterator != split_iterator();
       ++iterator) {
    if (iterator->empty()) {
      continue;
    }

    std::vector<std::string> splits;
    boost::split(
        splits,
        std::string{iterator->begin(), iterator->end()},
        boost::is_any_of(":"));

    if (splits.size() != 3) {
      WARNING(
          2,
          "Invalid line. Expected: `<REPO_PATH>:<path/to/repo/root>:<path/to/file>`. Skipping...");
      continue;
    }

    // Find the prefix
    auto lookup_key = boost::algorithm::replace_all_copy(splits[0], "/", "-");
    const auto& metadata =
        JsonValidation::null_or_object(metadata_json, lookup_key);
    if (metadata.isNull()) {
      WARNING(
          2, "Could not find metadata for repo: `{}`. Skipping...", splits[0]);
      continue;
    }

    auto absolute_path = fmt::format("{}/{}", splits[1], splits[2]);
    actual_paths.push_back(absolute_path);
    actual_to_repo_paths.emplace(
        absolute_path,
        fmt::format(
            "{}/{}",
            JsonValidation::string(metadata, "sapp_repo_key"),
            splits[2]));
  }

  return GrepoPaths{actual_paths, actual_to_repo_paths};
}

void add_to_class_to_path_map(
    std::vector<std::string>& exclude_directories,
    std::atomic<std::size_t>& iteration,
    std::vector<std::string>& paths,
    ConcurrentMap<std::string, std::string>& class_to_path,
    const std::unordered_map<std::string, std::string>& repo_paths) {
  Timer index_timer;
  LOG(2, "Indexing {} files...", paths.size());

  re2::RE2 package_regex("^package\\s+([^;]+)(?:;|$)");
  re2::RE2 class_regex(
      "^\\s*(?:/\\*.*\\*/)?\\s*(?:public|internal|private)?\\s*(?:abstract|data|final|open)?\\s*(?:class|enum|interface|object)\\s+([A-z0-9]+)");
  std::unordered_set<std::string> skipped_package_prefixes = {
      "android/",
  };

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
        std::string final_path = *path;
        if (auto find = repo_paths.find(*path); find != repo_paths.end()) {
          final_path = find->second;
        }

        std::ifstream stream(*path);
        std::string line;
        while (std::getline(stream, line)) {
          re2::StringPiece package_match;
          // Using capturing groups with `re2` is very slow, so we only
          // capture if we know the regex matches. This gives a huge
          // performance boost.
          if (!package && re2::RE2::PartialMatch(line, package_regex) &&
              re2::RE2::PartialMatch(line, package_regex, &package_match)) {
            package = std::string{package_match};
            boost::replace_all(*package, ".", "/");
            if (std::any_of(
                    skipped_package_prefixes.begin(),
                    skipped_package_prefixes.end(),
                    [&package](const auto& skipped_prefix) {
                      return boost::starts_with(*package, skipped_prefix);
                    })) {
              LOG(3, "Skipping module `{}` at `{}`...", *package, *path);
              return;
            }
            if (boost::ends_with(*path, ".kt")) {
              auto pos = path->find_last_of("/");
              if (pos != std::string::npos) {
                auto filename = path->substr(pos + 1, path->size() - pos - 4);
                auto classname = fmt::format("L{}/{}Kt;", *package, filename);

                class_to_path.update(
                    classname,
                    [&final_path](
                        const std::string& /* classname */,
                        std::string& value,
                        bool exists) mutable {
                      if (exists && value < final_path) {
                        return;
                      }
                      value = final_path;
                    });
              }
            }
          }

          re2::StringPiece class_match;
          if (package && maybe_class(line) &&
              re2::RE2::PartialMatch(line, class_regex) &&
              re2::RE2::PartialMatch(line, class_regex, &class_match)) {
            auto classname = fmt::format("L{}/{};", *package, class_match);

            class_to_path.update(
                classname,
                [&final_path](
                    const std::string& /* classname */,
                    std::string& value,
                    bool exists) mutable {
                  if (exists && value < final_path) {
                    return;
                  }
                  value = final_path;
                });
          }
        }
      },
      sparta::parallel::default_num_threads());
  for (auto& path : paths) {
    queue.add_item(&path);
  }
  queue.run_all();

  LOG(2,
      "Indexed {} top-level classes in {:.2f}s.",
      class_to_path.size(),
      index_timer.duration_in_seconds());
}

} // namespace

Positions::Positions(const Options& options, const DexStoresVector& stores) {
  Timer source_indexing_timer;

  if (options.skip_source_indexing()) {
    // Create a dummy path for all methods.
    for (auto& scope : DexStoreClassesIterator(stores)) {
      walk::parallel::methods(scope, [this](DexMethod* method) {
        auto class_name = method->get_class()->str_copy();
        class_name = class_name.substr(0, class_name.find(";"));
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

    // Save current path
    auto current_path = std::filesystem::current_path();
    std::filesystem::path source_root_directory{
        options.source_root_directory()};
    // Switch to source root directory.
    std::filesystem::current_path(source_root_directory);

    auto exclude_directories = options.source_exclude_directories();
    for (auto& exclude_directory : exclude_directories) {
      if (!boost::ends_with(exclude_directory, "/")) {
        exclude_directory.push_back('/');
      }
    }

    std::atomic<std::size_t> iteration(0);
    ConcurrentMap<std::string, std::string> class_to_path;

    if (auto grepo_metadata_path = options.grepo_metadata_path();
        !grepo_metadata_path.empty()) {
      auto metadata_json = JsonValidation::parse_json_file(grepo_metadata_path);
      JsonValidation::validate_object(metadata_json);

      // This command lists all tracked java and kotlin files in all sub
      // git-repos under the source root directory excluding files under
      // `test/*` directories.
      //
      // The output format is:
      //   <REPO_PATH>:<path/to/root/of/subrepo>:<path/to/file/within/subrepo>
      // - The absolute path on the filesystem is:
      //   <path/to/root/of/subrepo>/<path/to/file/within/subrepo>
      // - <REPO_PATH> is used to look up the grepo_metadata_json for the
      //   <prefix> to use for sapp.
      // - The final path for sapp is:
      //   <prefix>/<path/to/file/within/subrepo>
      std::string repo_command =
          "repo forall -c 'git ls-files -- '\''*java'\'' '\''*kt'\'' '\'':!:test/*'\'' | xargs -n1 printf \"$REPO_PATH:$PWD:%s\\n\"'";

      int return_code = -1;
      std::string output = execute_and_catch_output(repo_command, return_code);

      if (return_code == EXIT_SUCCESS) {
        auto grepo_paths =
            get_grepo_paths(std::move(output), std::move(metadata_json));

        LOG(2,
            "Found {} files in {:.2f}s.",
            grepo_paths.actual_paths.size(),
            paths_timer.duration_in_seconds());

        add_to_class_to_path_map(
            exclude_directories,
            iteration,
            grepo_paths.actual_paths,
            class_to_path,
            grepo_paths.actual_to_repo_paths);
      } else {
        ERROR(1, "`{}` failed, no source file will be indexed.", repo_command);
      }
    } else {
      std::string hg_command =
          "hg files --include=**.java --include=**.kt --include=**.mustache --exclude=.ovrsource-rest";
      std::string find_command =
          "find . -type f \\( -iname \\*.java -o -iname \\*.kt -o -iname \\*.mustache \\) -not -path ./.ovrsource-rest/\\*";

      int return_code = -1;
      std::string output = execute_and_catch_output(hg_command, return_code);

      if (return_code != EXIT_SUCCESS) {
        WARNING(
            1,
            "Source directory is not a mercurial repository. Trying `find` to discover files.");
        output = execute_and_catch_output(find_command, return_code);
        if (return_code != EXIT_SUCCESS) {
          ERROR(1, "`find` failed, no source file will be indexed.");
        }
      }

      if (return_code == EXIT_SUCCESS) {
        std::vector<std::string> paths;
        boost::split(paths, output, boost::is_any_of("\n"));

        LOG(2,
            "Found {} files in {:.2f}s.",
            paths.size(),
            paths_timer.duration_in_seconds());

        add_to_class_to_path_map(
            exclude_directories,
            iteration,
            paths,
            class_to_path,
            /* actual_to_repo_paths */ {});
      }
    }

    // Switch back to current path.
    std::filesystem::current_path(current_path);
    Timer method_paths_timer;
    LOG(2, "Indexing method paths...");

    for (auto& scope : DexStoreClassesIterator(stores)) {
      walk::parallel::methods(scope, [this, &class_to_path](DexMethod* method) {
        auto class_name = method->get_class()->str_copy();
        class_name = class_name.substr(0, class_name.find(";"));
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
    walk::parallel::methods(scope, [this](DexMethod* method) {
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

  LOG(2,
      "Total source indexing time: {:.2f}s.",
      source_indexing_timer.duration_in_seconds());
}

std::string Positions::execute_and_catch_output(
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

const std::string* MT_NULLABLE
Positions::get_path(const DexMethod* method) const {
  return method_to_path_.get(method, /* default */ nullptr);
}

} // namespace marianatrench
