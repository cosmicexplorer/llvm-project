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
                     hasArgument(0, expr().bind("conv_arg"))),
            expr().bind("unrecognized_expr"));

  Finder->addMatcher(
      implicitCastExpr(
          hasImplicitDestinationType(
              hasDeclaration(namedDecl(
                                 /* hasName("::std::basic_string") */
                                 )
                                 .bind("dest_type_decl"))
              /* anyOf(recordType() */
              /*           .bind("string_impl_dest"), */
              /*       type().bind("impl_dest")), */
              )
          /* hasSourceExpression(convert_expr) */
          )
          .bind("cast_expression"),
      this);

  {
    std::stringstream s(ToTypesOption);
    std::string to_type;
    std::string replacement;
    while (std::getline(s, to_type, '?') && std::getline(s, replacement, ',')) {
      replacementSuggestions[to_type] = replacement;
    }
  }
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
      Result.Nodes.getNodeAs<ImplicitCastExpr>("cast_expression");
  const auto *SubExpr = ConversionExpr->getSubExprAsWritten();

  /* FIXME: This is taken from DurationConversionCastCheck.cpp -- is this what
   * we want? */
  if (isInMacro(Result, ConversionExpr)) {
    return;
  }

  /* if (const auto *ConverterFunction = */
  /*         Result.Nodes.getNodeAs<FunctionDecl>("conv_func_decl")) { */
  /*   /\* const auto *Arg = Result.Nodes.getNodeAs<Expr>("conv_arg"); *\/ */
  /*   std::stringstream s; */
  /*   s << "conversion function " */
  /*     << ConverterFunction->getNameInfo().getName().getAsString() */
  /*     << " was used"; */
  /*   diag(ConverterFunction->getLocation(), s.str(), DiagnosticIDs::Note); */
  /* } else if (const auto *ConstructorExpr = */
  /*                Result.Nodes.getNodeAs<CXXConstructExpr>("ctor_expr")) { */
  /*   const auto *Constructor = ConstructorExpr->getConstructor(); */
  /*   /\* const auto *Arg = Result.Nodes.getNodeAs<Expr>("ctor_arg"); *\/ */
  /*   std::stringstream s; */
  /*   s << "constructor " << Constructor->getNameInfo().getName().getAsString()
   */
  /*     << " was used"; */
  /*   diag(Constructor->getLocation(), s.str(), DiagnosticIDs::Note); */
  /* } else { */
  /*   const auto *UnrecognizedExpr = */
  /*       Result.Nodes.getNodeAs<Expr>("unrecognized_expr"); */
  /*   std::stringstream s; */
  /*   s << "unrecognized type of implicit conversion!!!"; */
  /*   diag(UnrecognizedExpr->getBeginLoc(), s.str(), DiagnosticIDs::Note); */
  /* } */

  const auto *RetTypeDecl = Result.Nodes.getNodeAs<NamedDecl>("dest_type_decl");
  std::string returnTypeStr = RetTypeDecl->getQualifiedNameAsString();
  /* returnTypeStr = RetType->getCanonicalTypeInternal() */
  /*                     .getDesugaredType(*Result.Context) */
  /*                     .getAsString(); */
  /* if (const auto *StrType = Result.Nodes.getNodeAs<Type>("string_impl_dest"))
   * { */
  /*   returnTypeStr = ":WOW"; */
  /* } */
  /* if (const auto *TemplateType =
   * RetType->getAs<TemplateSpecializationType>()) { */
  /*   const auto *TempDecl =
   * TemplateType->getTemplateName().getAsTemplateDecl(); */
  /*   returnTypeStr = TempDecl->getQualifiedNameAsString(); */
  /* } else { */
  /*   returnTypeStr = RetType->getLocallyUnqualifiedSingleStepDesugaredType()
   */
  /*                       .getDesugaredType(*Result.Context) */
  /*                       .getAsString(); */
  /* } */

  if (replacementSuggestions.end() ==
      replacementSuggestions.find(returnTypeStr)) {
    /* for (auto it = replacementSuggestions.begin(); */
    /*      it != replacementSuggestions.end(); ++it) { */
    /*   std::stringstream s; */
    /*   s << "first: " << it->first << ", second: " << it->second; */
    /*   diag(ConversionExpr->getBeginLoc(), s.str()); */
    /* } */
    /* diag(ConversionExpr->getBeginLoc(), ToTypesOption); */
    return;
  }

  {
    std::stringstream s;
    s << "expression contains an implicit conversion to " << returnTypeStr
      << " from " << SubExpr->getType().getAsString();
    diag(ConversionExpr->getBeginLoc(), s.str());
  }

  std::string replacement = replacementSuggestions[returnTypeStr];
  {
    std::stringstream s;

    const std::string exprText =
        rangeText(ConversionExpr->getSourceRange(), *Result.Context);
    const std::string normalizedExprText =
        normalizeCastExpressionText(exprText);

    s << replacement << "(" << normalizedExprText << ")";
    diag(ConversionExpr->getBeginLoc(),
         "insert an explicit std::string conversion")
        << FixItHint::CreateReplacement(ConversionExpr->getSourceRange(),
                                        s.str());
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
