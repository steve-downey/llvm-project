//===- unittests/Lex/UnicodeOperatorCharSetsTest.cpp ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests for the frozen U1 user-operator code point set and its exclusion
// table (clang/lib/Lex/UnicodeOperatorCharSets.h, generated from UCD 17.0.0).
//
// The most valuable assertion here is UserOperatorsAreNeverIdentifierChars:
// the whole no-ambiguity argument for Unicode user-defined operators rests on
// the operator set and the identifier set being disjoint, so that a lexed code
// point classifies as exactly one of the two without context.
//
//===----------------------------------------------------------------------===//

#include "../../lib/Lex/UnicodeOperatorCharSets.h"
#include "../../lib/Lex/UnicodeCharSets.h"
#include "llvm/Support/UnicodeCharRanges.h"
#include "gtest/gtest.h"
#include <iterator>

using namespace clang;

namespace {

// A few members of the set, by the names they are known by in the design.
constexpr uint32_t SquaredPlus = 0x229E;      // ⊞
constexpr uint32_t CircledTimes = 0x2297;     // ⊗
constexpr uint32_t RightwardsBarArrow = 0x21A6; // ↦

// Counts pinned by the derivation against UCD 17.0.0. A change here is a
// change to the frozen set, not a test to update casually.
constexpr size_t ExpectedRanges = 32;
constexpr size_t ExpectedCodePoints = 1381;

size_t countCodePoints() {
  size_t N = 0;
  for (const llvm::sys::UnicodeCharRange &R : UserOperatorRanges)
    N += R.Upper - R.Lower + 1;
  return N;
}

TEST(UnicodeOperatorCharSetsTest, FrozenCountsMatchTheDerivation) {
  EXPECT_EQ(ExpectedRanges, std::size(UserOperatorRanges));
  EXPECT_EQ(ExpectedCodePoints, countCodePoints());
  // U§8's "256-byte sorted table, one binary search".
  EXPECT_EQ(256u, sizeof(UserOperatorRanges));
}

TEST(UnicodeOperatorCharSetsTest, TableIsSortedAndNonOverlapping) {
  // llvm::sys::UnicodeCharSet asserts this too, but only when NDEBUG is off;
  // a binary search over an unsorted table fails silently otherwise.
  uint32_t Prev = 0;
  bool First = true;
  for (const llvm::sys::UnicodeCharRange &R : UserOperatorRanges) {
    EXPECT_LE(R.Lower, R.Upper);
    if (!First) {
      // Strictly greater than Prev + 1: adjacent ranges would have been
      // merged by the generator, so a gap of exactly one means a bug.
      EXPECT_GT(R.Lower, Prev + 1);
    }
    Prev = R.Upper;
    First = false;
  }
  // Constructing the set exercises LLVM's own sortedness assertion.
  llvm::sys::UnicodeCharSet Set(UserOperatorRanges);
  EXPECT_TRUE(Set.contains(SquaredPlus));
}

TEST(UnicodeOperatorCharSetsTest, Members) {
  EXPECT_TRUE(isUserOperatorChar(SquaredPlus));
  EXPECT_TRUE(isUserOperatorChar(CircledTimes));
  EXPECT_TRUE(isUserOperatorChar(RightwardsBarArrow));
  // The one character assigned by Unicode 17.0 itself inside the blocks.
  EXPECT_TRUE(isUserOperatorChar(0x2B96)); // ⮖ EQUALS SIGN WITH INFINITY ABOVE
}

TEST(UnicodeOperatorCharSetsTest, AsciiIsNeverAUserOperator) {
  // Predicate 2: every ASCII candidate is already claimed by C++.
  for (uint32_t C = 0; C < 0x80; ++C)
    EXPECT_FALSE(isUserOperatorChar(C)) << "U+" << C;
  EXPECT_FALSE(isUserOperatorChar('+'));
  EXPECT_FALSE(isUserOperatorChar('<'));
}

TEST(UnicodeOperatorCharSetsTest, OutsideTheBlocks) {
  // Pattern_Syntax members that fail predicate 3.
  EXPECT_FALSE(isUserOperatorChar(0x00D7)); // × MULTIPLICATION SIGN (Latin-1)
  EXPECT_FALSE(isUserOperatorChar(0x00F7)); // ÷ DIVISION SIGN (Latin-1)
  EXPECT_FALSE(isUserOperatorChar(0x2E55)); // ⹕ (Supplemental Punctuation)
  EXPECT_FALSE(isUserOperatorChar(0x205E)); // ⁞ VERTICAL FOUR DOTS
}

TEST(UnicodeOperatorCharSetsTest, PairedBracketsAreNotOperators) {
  // Predicate 4 keeps Ps/Pe out: delimiters, not infix material.
  EXPECT_FALSE(isUserOperatorChar(0x27E8)); // ⟨ MATHEMATICAL LEFT ANGLE BRACKET
  EXPECT_FALSE(isUserOperatorChar(0x27E9)); // ⟩
  EXPECT_FALSE(isUserOperatorChar(0x27E6)); // ⟦
  EXPECT_FALSE(isUserOperatorChar(0x27E7)); // ⟧
  EXPECT_FALSE(isUserOperatorChar(0x2308)); // ⌈
  EXPECT_FALSE(isUserOperatorChar(0x230B)); // ⌋
}

TEST(UnicodeOperatorCharSetsTest, UnassignedCodePointsAreNotOperators) {
  // U+2B74/U+2B75 are Pattern_Syntax code points inside the blocks with no
  // character assigned as of 17.0. R3c: unassigned code points are not
  // characters. A later UCD may fill them; the frozen set will not follow.
  EXPECT_FALSE(isUserOperatorChar(0x2B74));
  EXPECT_FALSE(isUserOperatorChar(0x2B75));
  EXPECT_TRUE(isUserOperatorChar(0x2B73));
  EXPECT_TRUE(isUserOperatorChar(0x2B76));
}

TEST(UnicodeOperatorCharSetsTest, IdentifierProfileExclusions) {
  // ∂ ∇ ∞ are ceded to the identifier side (U10) — and Clang already lexes
  // them as identifier characters via the mathematical notation profile.
  for (uint32_t C : {0x2202u, 0x2207u, 0x221Eu}) {
    EXPECT_FALSE(isUserOperatorChar(C)) << "U+" << C;
    EXPECT_EQ(UserOperatorExclusionReason::IdentifierProfile,
              getExclusionReason(C))
        << "U+" << C;
    EXPECT_EQ(nullptr, getUserOperatorExclusion(C)->Confusable);
  }
}

TEST(UnicodeOperatorCharSetsTest, ConfusableExclusionsCarryTheTokenTheyApe) {
  struct {
    uint32_t CodePoint;
    const char *Token;
  } Cases[] = {
      {0x2212, "-"},  {0x2215, "/"},   {0x2044, "/"},  {0x2217, "*"},
      {0x2223, "|"},  {0x2236, ":"},   {0x2219, "."},  {0x22C5, "."},
      {0x2264, "<="}, {0x2265, ">="},  {0x21D0, "<="}, {0x21D2, "=>"},
      {0x21D4, "<=>"},
  };
  for (const auto &Case : Cases) {
    EXPECT_FALSE(isUserOperatorChar(Case.CodePoint)) << "U+" << Case.CodePoint;
    EXPECT_EQ(UserOperatorExclusionReason::ConfusableWith,
              getExclusionReason(Case.CodePoint))
        << "U+" << Case.CodePoint;
    const UserOperatorExclusion *E = getUserOperatorExclusion(Case.CodePoint);
    ASSERT_NE(nullptr, E);
    ASSERT_NE(nullptr, E->Confusable);
    EXPECT_STREQ(Case.Token, E->Confusable);
  }
  // Neighbours of the excluded characters stay in the set: the exclusion is a
  // hole punched in a range, not a widened gap.
  EXPECT_TRUE(isUserOperatorChar(0x2211));  // ∑, just below U+2212
  EXPECT_TRUE(isUserOperatorChar(0x2213));  // ∓, just above
  EXPECT_TRUE(isUserOperatorChar(0x21D1));  // ⇑, between ⇐ and ⇒
}

TEST(UnicodeOperatorCharSetsTest, EmojiPresentationExclusions) {
  for (uint32_t C : {0x231Au, 0x231Bu, 0x23E9u, 0x23F0u, 0x2B1Bu, 0x2B50u,
                     0x2B55u}) {
    EXPECT_FALSE(isUserOperatorChar(C)) << "U+" << C;
    EXPECT_EQ(UserOperatorExclusionReason::EmojiPresentation,
              getExclusionReason(C))
        << "U+" << C;
  }
}

TEST(UnicodeOperatorCharSetsTest, ExclusionTableIsSortedAndConsistent) {
  uint32_t Prev = 0;
  for (const UserOperatorExclusion &E : ExcludedOperatorChars) {
    EXPECT_GT(E.CodePoint, Prev);
    Prev = E.CodePoint;
    // An exclusion that is also a member would make the tables contradict.
    EXPECT_FALSE(isUserOperatorChar(E.CodePoint)) << "U+" << E.CodePoint;
    EXPECT_NE(UserOperatorExclusionReason::None, E.Reason);
    if (E.Reason == UserOperatorExclusionReason::ConfusableWith)
      EXPECT_NE(nullptr, E.Confusable);
    else
      EXPECT_EQ(nullptr, E.Confusable);
  }
  // 16 named exclusions (U§5 predicate 5) + 12 emoji-presentation code points
  // inside the blocks.
  EXPECT_EQ(28u, std::size(ExcludedOperatorChars));
}

TEST(UnicodeOperatorCharSetsTest, NonExclusionsReportNoReason) {
  EXPECT_EQ(UserOperatorExclusionReason::None,
            getExclusionReason(SquaredPlus));
  EXPECT_EQ(UserOperatorExclusionReason::None, getExclusionReason('a'));
  EXPECT_EQ(UserOperatorExclusionReason::None, getExclusionReason(0x2B74));
  EXPECT_EQ(nullptr, getUserOperatorExclusion(0x27E8)); // a bracket, no reason
}

// The invariant U10 rests on. Cross-checked against Clang's own in-tree
// identifier tables — which are labelled Unicode 18.0 while U1 is frozen at
// UCD 17.0 (DEV-U01), so this is a *stronger* check than the design claims:
// U1@17.0 stays out of identifier space even after the UCD moved under it.
TEST(UnicodeOperatorCharSetsTest, UserOperatorsAreNeverIdentifierChars) {
  llvm::sys::UnicodeCharSet XIDStart(XIDStartRanges);
  llvm::sys::UnicodeCharSet XIDContinue(XIDContinueRanges);
  llvm::sys::UnicodeCharSet MathStart(
      MathematicalNotationProfileIDStartRanges);
  llvm::sys::UnicodeCharSet MathContinue(
      MathematicalNotationProfileIDContinueRanges);
  llvm::sys::UnicodeCharSet C11Allowed(C11AllowedIDCharRanges);

  size_t Checked = 0;
  for (const llvm::sys::UnicodeCharRange &R : UserOperatorRanges) {
    for (uint32_t C = R.Lower; C <= R.Upper; ++C) {
      ++Checked;
      EXPECT_FALSE(XIDStart.contains(C)) << "XID_Start U+" << C;
      EXPECT_FALSE(XIDContinue.contains(C)) << "XID_Continue U+" << C;
      // The math identifier profile Clang ships (D137051/P3658R1) must not
      // reach into the operator set either.
      EXPECT_FALSE(MathStart.contains(C)) << "ID_Compat_Math_Start U+" << C;
      EXPECT_FALSE(MathContinue.contains(C))
          << "ID_Compat_Math_Continue U+" << C;
      // C++11-C++20 [charname.allowed], for the historical claim in U§7.1.
      EXPECT_FALSE(C11Allowed.contains(C)) << "C11 allowed U+" << C;
    }
  }
  EXPECT_EQ(ExpectedCodePoints, Checked);
}

// The tightest gap in the disjointness argument, worth its own case: the
// Pattern_Syntax run ends at U+205E and ID_Compat_Math_Continue starts at
// U+2070, eighteen code points later. Neither side reaches the other, and
// both are outside the U1 blocks anyway.
TEST(UnicodeOperatorCharSetsTest, TightestGapAgainstMathIdentifiers) {
  llvm::sys::UnicodeCharSet MathContinue(
      MathematicalNotationProfileIDContinueRanges);
  EXPECT_FALSE(isUserOperatorChar(0x205E));
  EXPECT_FALSE(MathContinue.contains(0x205E));
  EXPECT_TRUE(MathContinue.contains(0x2070)); // ⁰, the identifier side
  EXPECT_FALSE(isUserOperatorChar(0x2070));
  // Nothing below the Arrows block is a user operator at all.
  for (uint32_t C = 0x2000; C < 0x2190; ++C)
    EXPECT_FALSE(isUserOperatorChar(C)) << "U+" << C;
}

} // namespace
