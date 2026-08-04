//===- unittests/AST/DeclarationNameTest.cpp - DeclarationName tests ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests for DeclarationName::CXXUserOperatorName, the name kind for Unicode
// user-defined operators (-funicode-operators).  Nothing parses these yet
// (U07); these tests exercise the AST layer directly.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclarationName.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

namespace clang {
namespace {

// U+229E SQUARED PLUS and U+2297 CIRCLED TIMES, both members of the frozen
// U1 operator set.
constexpr uint32_t SquaredPlus = 0x229E;
constexpr uint32_t CircledTimes = 0x2297;

class UserOperatorNameTest : public ::testing::Test {
protected:
  void SetUp() override {
    AST = tooling::buildASTFromCode("", "input.cc");
    ASSERT_TRUE(AST);
  }

  DeclarationNameTable &names() {
    return AST->getASTContext().DeclarationNames;
  }

  std::unique_ptr<ASTUnit> AST;
};

// The identity of a user-operator name is its code point, so asking twice for
// the same code point must yield the very same name -- not merely an equal
// one.  This is what makes redeclaration, lookup and DenseMap keying work.
TEST_F(UserOperatorNameTest, Uniques) {
  DeclarationName A = names().getCXXUserOperatorName(SquaredPlus);
  DeclarationName B = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_TRUE(A == B);
  EXPECT_EQ(A.getAsOpaquePtr(), B.getAsOpaquePtr());
}

TEST_F(UserOperatorNameTest, DistinctCodePointsAreDistinctNames) {
  DeclarationName Plus = names().getCXXUserOperatorName(SquaredPlus);
  DeclarationName Times = names().getCXXUserOperatorName(CircledTimes);
  EXPECT_TRUE(Plus != Times);
  EXPECT_NE(Plus.getAsOpaquePtr(), Times.getAsOpaquePtr());
}

// The kind survives the trip through DeclarationNameExtra, and the payload
// comes back unchanged.  CXXUserOperatorName is an "uncommon" kind: it is not
// one of the seven that fit in DeclarationName's three spare pointer bits, so
// getNameKind() has to recover it from the extra structure.
TEST_F(UserOperatorNameTest, KindRoundTrips) {
  DeclarationName N = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_EQ(N.getNameKind(), DeclarationName::CXXUserOperatorName);
  EXPECT_EQ(N.getCXXUserOperatorCodePoint(), SquaredPlus);

  // Round-trip through the opaque representation too -- that is how the name
  // travels through DeclarationNameLoc, DenseMap and the diagnostics engine.
  DeclarationName Opaque = DeclarationName::getFromOpaquePtr(N.getAsOpaquePtr());
  EXPECT_EQ(Opaque.getNameKind(), DeclarationName::CXXUserOperatorName);
  EXPECT_EQ(Opaque.getCXXUserOperatorCodePoint(), SquaredPlus);
}

// Inserting CXXUserOperatorName into DeclarationNameExtra::ExtraKind moved
// ObjCMultiArgSelector, whose encoding is ObjCMultiArgSelector + NumArgs.
// If the renumbering had gone wrong, multi-keyword selectors would report the
// wrong kind (or the wrong arity).  Guard it here rather than in ObjC tests.
TEST_F(UserOperatorNameTest, OtherKindsUnaffected) {
  ASTContext &Ctx = AST->getASTContext();
  const IdentifierInfo *Set = &Ctx.Idents.get("setFoo");
  const IdentifierInfo *With = &Ctx.Idents.get("with");
  const IdentifierInfo *IIs[] = {Set, With};

  Selector Sel = Ctx.Selectors.getSelector(2, IIs);
  DeclarationName SelName(Sel);
  EXPECT_EQ(SelName.getNameKind(), DeclarationName::ObjCMultiArgSelector);
  EXPECT_EQ(SelName.getObjCSelector().getNumArgs(), 2u);
  EXPECT_EQ(SelName.getObjCSelector().getAsString(), "setFoo:with:");

  Selector One = Ctx.Selectors.getSelector(1, IIs);
  EXPECT_EQ(DeclarationName(One).getNameKind(),
            DeclarationName::ObjCOneArgSelector);

  DeclarationName UsingDir = DeclarationName::getUsingDirectiveName();
  EXPECT_EQ(UsingDir.getNameKind(), DeclarationName::CXXUsingDirective);

  DeclarationName Lit =
      names().getCXXLiteralOperatorName(&Ctx.Idents.get("_km"));
  EXPECT_EQ(Lit.getNameKind(), DeclarationName::CXXLiteralOperatorName);
  EXPECT_EQ(Lit.getAsString(), "operator\"\"_km");

  // Accessors that key off the kind must not claim these.
  EXPECT_EQ(Lit.getCXXUserOperatorCodePoint(), 0u);
  EXPECT_EQ(UsingDir.getCXXUserOperatorCodePoint(), 0u);
  EXPECT_EQ(names().getCXXUserOperatorName(SquaredPlus).getCXXLiteralIdentifier(),
            nullptr);
}

TEST_F(UserOperatorNameTest, Prints) {
  DeclarationName N = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_EQ(N.getAsString(), "operator⊞");

  std::string Out;
  llvm::raw_string_ostream OS(Out);
  N.print(OS, AST->getASTContext().getPrintingPolicy());
  EXPECT_EQ(Out, "operator⊞");

  EXPECT_EQ(names().getCXXUserOperatorName(CircledTimes).getAsString(),
            "operator⊗");
}

// The acceptance criterion for this step: two spellings of the same operator
// must yield the same DeclarationName.  That holds by construction because the
// name is keyed on the code point and the lexer canonicalizes every spelling
// to a code point before we are reached (Lexer::getUserOperatorCodePoint).
// The glyph spelling is exercised here; the universal-character-name spellings
// land in U04 and route through the same funnel, so they cannot diverge.
TEST_F(UserOperatorNameTest, IdentityIsTheCodePointNotTheSpelling) {
  // "⊞" in the *source* sense: the UTF-8 bytes of the glyph.
  uint32_t FromGlyph = Lexer::getUserOperatorCodePoint("⊞");
  ASSERT_EQ(FromGlyph, SquaredPlus);

  DeclarationName FromSpelling = names().getCXXUserOperatorName(FromGlyph);
  DeclarationName FromScalar = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_EQ(FromSpelling.getAsOpaquePtr(), FromScalar.getAsOpaquePtr());
}

// DeclarationName::compare orders user operators by code point; it is used by
// the deterministic-output paths (module writing, diagnostics ordering).
TEST_F(UserOperatorNameTest, Compares) {
  DeclarationName Times = names().getCXXUserOperatorName(CircledTimes);
  DeclarationName Plus = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_LT(DeclarationName::compare(Times, Plus), 0);
  EXPECT_GT(DeclarationName::compare(Plus, Times), 0);
  EXPECT_EQ(DeclarationName::compare(Plus, Plus), 0);
}

// FETokenInfo lives in the extra structure for every uncommon kind; without a
// case in getFETokenInfoSlow/setFETokenInfoSlow this asserts.
TEST_F(UserOperatorNameTest, FETokenInfo) {
  DeclarationName N = names().getCXXUserOperatorName(SquaredPlus);
  EXPECT_EQ(N.getFETokenInfo(), nullptr);
  int Marker = 0;
  N.setFETokenInfo(&Marker);
  EXPECT_EQ(N.getFETokenInfo(), &Marker);
  EXPECT_EQ(names().getCXXUserOperatorName(SquaredPlus).getFETokenInfo(),
            &Marker);
  N.setFETokenInfo(nullptr);
}

// U07 needs somewhere to put the operator's source location.
TEST_F(UserOperatorNameTest, NameLoc) {
  DeclarationName N = names().getCXXUserOperatorName(SquaredPlus);
  DeclarationNameInfo Info(N, SourceLocation());
  EXPECT_TRUE(Info.getCXXUserOperatorNameLoc().isInvalid());

  SourceLocation Loc = SourceLocation::getFromRawEncoding(42);
  Info.setCXXUserOperatorNameLoc(Loc);
  EXPECT_EQ(Info.getCXXUserOperatorNameLoc(), Loc);
  EXPECT_EQ(Info.getEndLoc(), Loc);

  // Kind-keyed accessors for other kinds must stay silent.
  EXPECT_TRUE(Info.getCXXLiteralOperatorNameLoc().isInvalid());
  EXPECT_TRUE(Info.getCXXOperatorNameRange().isInvalid());

  EXPECT_FALSE(Info.isInstantiationDependent());
  EXPECT_FALSE(Info.containsUnexpandedParameterPack());
  EXPECT_EQ(Info.getAsString(), "operator⊞");
}

} // namespace
} // namespace clang
