//===--- FlaggedImplicitConversionsCheck.cpp - clang-tidy -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "FlaggedImplicitConversionsCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/FixIt.h"

#include <sstream>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace bugprone {

static const char *s = "wow";

void FlaggedImplicitConversionsCheck::registerMatchers(MatchFinder *Finder) {
  /* FIXME: this is supposed to trigger the check! */
  std::string a = s;

  /* NB: `std::string` is a template instantiation -- do we need this? */
  /* const auto string_type_decl = */
  /*     classTemplateSpecializationDecl(hasName("::std::basic_string")); */

  /* Match implicit type conversions (some stolen from
   * StringCompareCheck.cpp!).
   */
  /* const auto string_type_matcher = hasDeclaration(string_type_decl); */
  const auto convert_expr =
      anyOf(cxxConstructExpr(hasArgument(0, expr().bind("ctor_arg")))
                .bind("ctor_expr"),
            callExpr(callee(functionDecl().bind("conv_func_decl")),
                     hasArgument(0, expr().bind("conv_arg"))));

  Finder->addMatcher(implicitCastExpr(hasSourceExpression(convert_expr))
                         .bind("cast_expression"),
                     this);
}

static std::string rangeText(SourceRange range, ASTContext &ctx) {
  return clang::tooling::fixit::internal::getText(
             CharSourceRange::getTokenRange(range), ctx)
      .str();
}

/* NB: Remove parens around the expression so we can avoid suggesting
 * replacements with doubled parens like (()). */
static std::string
normalizeCastExpressionText(std::string castExpressionSourceText) {
  static auto parenRegex = llvm::Regex("^\\((.*)\\)$");
  return parenRegex.sub("\\1", castExpressionSourceText);
}

/* From DurationRewriter.cpp! */
static bool isInMacro(const MatchFinder::MatchResult &Result, const Expr *E) {
  if (!E->getBeginLoc().isMacroID())
    return false;

  SourceLocation Loc = E->getBeginLoc();
  // We want to get closer towards the initial macro typed into the source only
  // if the location is being expanded as a macro argument.
  while (Result.SourceManager->isMacroArgExpansion(Loc)) {
    // We are calling getImmediateMacroCallerLoc, but note it is essentially
    // equivalent to calling getImmediateSpellingLoc in this context according
    // to Clang implementation. We are not calling getImmediateSpellingLoc
    // because Clang comment says it "should not generally be used by clients."
    Loc = Result.SourceManager->getImmediateMacroCallerLoc(Loc);
  }
  return Loc.isMacroID();
}

void FlaggedImplicitConversionsCheck::check(
    const MatchFinder::MatchResult &Result) {

  const auto *ConversionExpr =
      Result.Nodes.getNodeAs<CastExpr>("cast_expression");

  /* FIXME: This is taken from DurationConversionCastCheck.cpp -- is this what
   * we want? */
  if (isInMacro(Result, ConversionExpr)) {
    return;
  }

  {
    std::stringstream s;
    s << "expression contains an implicit conversion to ::std::basic_string";
    diag(ConversionExpr->getBeginLoc(), s.str(), DiagnosticIDs::Note);
  }

  if (const auto *ConverterFunction =
          Result.Nodes.getNodeAs<FunctionDecl>("conv_func_decl")) {
    /* const auto *Arg = Result.Nodes.getNodeAs<Expr>("conv_arg"); */
    std::stringstream s;
    s << "conversion function "
      << ConverterFunction->getNameInfo().getName().getAsString()
      << " was used";
    diag(ConverterFunction->getLocation(), s.str(), DiagnosticIDs::Note);
  } else if (const auto *ConstructorExpr =
                 Result.Nodes.getNodeAs<CXXConstructExpr>("ctor_expr")) {
    const auto *Constructor = ConstructorExpr->getConstructor();
    /* const auto *Arg = Result.Nodes.getNodeAs<Expr>("ctor_arg"); */
    std::stringstream s;
    s << "constructor " << Constructor->getNameInfo().getName().getAsString()
      << " was used";
    diag(Constructor->getLocation(), s.str(), DiagnosticIDs::Note);
  }

  {
    std::stringstream s;

    const std::string exprText =
        rangeText(ConversionExpr->getSourceRange(), *Result.Context);
    const std::string normalizedExprText =
        normalizeCastExpressionText(exprText);

    s << "std::string(" << normalizedExprText << ")";
    diag(ConversionExpr->getBeginLoc(),
         "insert an explicit std::string conversion")
        << FixItHint::CreateReplacement(ConversionExpr->getSourceRange(),
                                        s.str());
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
