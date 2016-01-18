/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <pbxbuild/Tool/SearchPaths.h>
#include <pbxbuild/Tool/Context.h>

namespace Tool = pbxbuild::Tool;
using libutil::FSUtil;

Tool::SearchPaths::
SearchPaths(
    std::vector<std::string> const &headerSearchPaths,
    std::vector<std::string> const &userHeaderSearchPaths,
    std::vector<std::string> const &frameworkSearchPaths,
    std::vector<std::string> const &librarySearchPaths) :
    _headerSearchPaths    (headerSearchPaths),
    _userHeaderSearchPaths(userHeaderSearchPaths),
    _frameworkSearchPaths (frameworkSearchPaths),
    _librarySearchPaths   (librarySearchPaths)
{
}

static void
AppendPaths(std::vector<std::string> *args, pbxsetting::Environment const &environment, std::string const &workingDirectory, std::vector<std::string> const &paths)
{
    for (std::string const &path : paths) {
        std::string recursive = "**";
        if (path.size() >= recursive.size() && path.substr(path.size() - recursive.size()) == recursive) {
            std::string root = path.substr(0, path.size() - recursive.size());
            args->push_back(root);

            std::string absoluteRoot = FSUtil::ResolveRelativePath(root, workingDirectory);
            FSUtil::EnumerateRecursive(absoluteRoot, [&](std::string const &path) -> bool {
                // TODO(grp): Use build settings for included and excluded recursive paths.
                // Included: INCLUDED_RECURSIVE_SEARCH_PATH_SUBDIRECTORIES
                // Excluded: EXCLUDED_RECURSIVE_SEARCH_PATH_SUBDIRECTORIES
                // Follow: RECURSIVE_SEARCH_PATHS_FOLLOW_SYMLINKS

                if (FSUtil::TestForDirectory(path)) {
                    std::string relativePath = root + "/" + path.substr(absoluteRoot.size() + 1);
                    args->push_back(relativePath);
                }
                return true;
            });
        } else {
            args->push_back(path);
        }
    }
}

std::vector<std::string> Tool::SearchPaths::
ExpandRecursive(std::vector<std::string> const &paths, pbxsetting::Environment const &environment, std::string const &workingDirectory)
{
    std::vector<std::string> result;
    AppendPaths(&result, environment, workingDirectory, paths);
    return result;
}

Tool::SearchPaths Tool::SearchPaths::
Create(pbxsetting::Environment const &environment, std::string const &workingDirectory)
{
    std::vector<std::string> headerSearchPaths;
    AppendPaths(&headerSearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("PRODUCT_TYPE_HEADER_SEARCH_PATHS")));
    AppendPaths(&headerSearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("HEADER_SEARCH_PATHS")));

    std::vector<std::string> userHeaderSearchPaths;
    AppendPaths(&userHeaderSearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("USER_HEADER_SEARCH_PATHS")));

    std::vector<std::string> frameworkSearchPaths;
    AppendPaths(&frameworkSearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("FRAMEWORK_SEARCH_PATHS")));
    AppendPaths(&frameworkSearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("PRODUCT_TYPE_FRAMEWORK_SEARCH_PATHS")));

    std::vector<std::string> librarySearchPaths;
    AppendPaths(&librarySearchPaths, environment, workingDirectory, pbxsetting::Type::ParseList(environment.resolve("LIBRARY_SEARCH_PATHS")));

    return Tool::SearchPaths(headerSearchPaths, userHeaderSearchPaths, frameworkSearchPaths, librarySearchPaths);
}
