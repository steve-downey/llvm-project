//===- unittests/Sema/UserOperatorDeclTest.cpp ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Regression test for the identifier namespace of a namespace-scope Unicode
// user-defined operator (-funicode-operators).
//
// Sema::LookupOperatorName searches Decl::IDNS_NonMemberOperator and nothing
// else, and a declaration only lands there if Sema::ActOnFunctionDeclarator
// calls setNonMemberOperator() on it. That call used to be guarded solely by
// FunctionDecl::isOverloadedOperator(), which is false for a user operator by
// construction -- so `operator⊞` declared at namespace scope was invisible to
// operator lookup, and therefore to operator-candidate assembly and ADL.
//
// The symptom of that bug is a missing overload candidate at a *use* site,
// several steps away from its cause; GCC's DEV-G05 is the same class of
// mistake found late. This test pins the cause instead, and it can run before
// any infix syntax exists.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclarationName.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace clang::tooling;

namespace {

// U+229E SQUARED PLUS, a member of the frozen U1 operator set.
constexpr uint32_t SquaredPlus = 0x229E;

class OperatorLookupAction : public ASTFrontendAction {
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &,
                                                 StringRef) override {
    return std::make_unique<clang::ASTConsumer>();
  }

  void ExecuteAction() override {
    CompilerInstance &CI = getCompilerInstance();
    ASSERT_FALSE(CI.hasSema());
    CI.createSema(getTranslationUnitKind(), nullptr);
    ASSERT_TRUE(CI.hasSema());
    Sema &S = CI.getSema();
    ParseAST(S);

    ASTContext &Ctx = S.getASTContext();
    DeclarationName Name =
        Ctx.DeclarationNames.getCXXUserOperatorName(SquaredPlus);

    // The cause: the declaration itself must carry IDNS_NonMemberOperator.
    unsigned NumFound = 0;
    for (Decl *D : Ctx.getTranslationUnitDecl()->decls()) {
      auto *FD = dyn_cast<FunctionDecl>(D);
      if (!FD || FD->getDeclName() != Name)
        continue;
      ++NumFound;
      EXPECT_TRUE(FD->isUserOperator());
      EXPECT_EQ(FD->getUserOperatorCodePoint(), SquaredPlus);
      EXPECT_TRUE(FD->isInIdentifierNamespace(Decl::IDNS_NonMemberOperator))
          << "a namespace-scope user operator is not in "
             "IDNS_NonMemberOperator; Sema::LookupOperatorName searches that "
             "identifier namespace and no other";
    }
    EXPECT_EQ(NumFound, 1u);

    // The consequence: operator lookup must actually find it. This is the
    // lookup operator-candidate assembly runs.
    LookupResult R(S, Name, SourceLocation(), Sema::LookupOperatorName);
    R.suppressDiagnostics();
    S.LookupQualifiedName(R, Ctx.getTranslationUnitDecl());
    EXPECT_FALSE(R.empty())
        << "Sema::LookupOperatorName found no candidate for operator⊞";
  }
};

TEST(UserOperatorDeclTest, NamespaceScopeOperatorIsVisibleToOperatorLookup) {
  static const char *Code = "int operator⊞(int, int);\n";
  ASSERT_TRUE(runToolOnCodeWithArgs(std::make_unique<OperatorLookupAction>(),
                                    Code,
                                    {"-std=c++20", "-funicode-operators"},
                                    "test.cc"));
}

} // namespace
