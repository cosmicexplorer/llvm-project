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
  const auto string_type_decl =
      classTemplateSpecializationDecl(hasName("::std::basic_string"));

  /* Match implicit type conversions (some stolen from StringCompareCheck.cpp!).
   */
  const auto string_type_matcher = hasDeclaration(string_type_decl);
  Finder->addMatcher(
      implicitCastExpr(hasImplicitDestinationType(string_type_matcher))
          .bind("implicit_str_conversion"),
      this);

  /* Match explicit type conversions (some stolen from
   * DurationConversionCastCheck.cpp!). */
  const auto explicit_call_matcher = ignoringImpCasts(callExpr(
      callee(functionDecl().bind("explicit_conv_func_decl")),
      hasArgument(0,
                  expr(hasType(string_type_decl)).bind("explicit_conv_arg"))));
  Finder->addMatcher(
      expr(anyOf(
          cxxStaticCastExpr(hasSourceExpression(explicit_call_matcher))
              .bind("explicit_cast_expr"),
          cStyleCastExpr(hasSourceExpression(explicit_call_matcher))
              .bind("explicit_cast_expr"),
          cxxFunctionalCastExpr(hasSourceExpression(explicit_call_matcher))
              .bind("explicit_cast_expr"))),
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

  if (const auto *ConversionExpr =
          Result.Nodes.getNodeAs<ImplicitCastExpr>("implicit_str_conversion")) {
    /* Operate on implicit casts! */
    const std::string exprText =
        rangeText(ConversionExpr->getSourceRange(), *Result.Context);
    const std::string normalizedExprText =
        normalizeCastExpressionText(exprText);

    std::stringstream s1;
    s1 << "expression `" << exprText
       << "` contains an implicit conversion to ::std::basic_string";
    diag(ConversionExpr->getBeginLoc(), s1.str(), DiagnosticIDs::Note);

    if (const auto *ConverterNamedDecl =
            ConversionExpr->getConversionFunction()) {
      std::stringstream s2;
      const auto *nameId = ConverterNamedDecl->getIdentifier();
      const auto declName = DeclarationName(nameId);
      s2 << "conversion function " << declName.getAsString() << " was used";
      diag(ConverterNamedDecl->getLocation(), s2.str());
    }

    std::stringstream s3;
    s3 << "std::string(" << normalizedExprText << ")";
    diag(ConversionExpr->getBeginLoc(),
         "insert an explicit std::string() conversion")
        << FixItHint::CreateReplacement(ConversionExpr->getSourceRange(),
                                        s3.str());
  } else if (const auto *MatchedCast = Result.Nodes.getNodeAs<ExplicitCastExpr>(
                 "explicit_cast_expr")) {
    /* FIXME: This is taken from DurationConversionCastCheck.cpp -- is this what
     * we want? */
    if (isInMacro(Result, MatchedCast)) {
      return;
    }
    /* Operate on explicit casts! */
    /* const auto* FuncDecl = Result.Nodes.getNodeAs<FunctionDecl>(""); */
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
