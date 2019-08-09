//===--- FlaggedImplicitConversionsCheck.h - clang-tidy ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_FLAGGEDIMPLICITCONVERSIONSCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_FLAGGEDIMPLICITCONVERSIONSCHECK_H

#include "../ClangTidyCheck.h"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace clang {
namespace tidy {
namespace bugprone {

/// FIXME: Write a short description.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/bugprone-flagged-implicit-conversions.html
class FlaggedImplicitConversionsCheck : public ClangTidyCheck {
  const std::string ToTypesOption;

  std::unordered_map<std::string, std::string> replacementSuggestions;

public:
  FlaggedImplicitConversionsCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context),
        ToTypesOption(Options.get("ToTypes", "")), replacementSuggestions() {}

  void storeOptions(ClangTidyOptions::OptionMap &Opts) override {
    Options.store(Opts, "ToTypes", ToTypesOption);
  }
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace bugprone
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_FLAGGEDIMPLICITCONVERSIONSCHECK_H
