// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck --implicit-check-not=op_u0000 %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -fbacktick -triple x86_64-linux-gnu -emit-llvm -o - %s | FileCheck --implicit-check-not=op_u0000 %s
// RUN: %clang_cc1 -std=c++23 -funicode-operators -triple x86_64-pc-windows-msvc -emit-llvm -o /dev/null -verify=msvc -DMSVC_UNSUPPORTED %s

// U09: `operator⊞` mangles through the Itanium *vendor-extended operator*
// production (U8, U-design section 9):
//
//     <operator-name> ::= v <digit> <source-name>
//
// with the <digit> the operator's declared arity (1 prefix, 2 infix, counting
// a member's implicit object parameter) and the <source-name> derived from the
// operator's Unicode code point as "op_u" + uppercase hex, no "U+" prefix, at
// least four digits. The full rule is stated in ItaniumMangle.cpp next to the
// code that implements it; it is a de facto ABI decision for the prototype.
//
// NOT this step: infix or prefix *uses* (U11/U12) -- every occurrence here is
// a declaration or an explicit call. Serialization is still U17's, so nothing
// here goes near -emit-pch or modules.

#ifdef MSVC_UNSUPPORTED

// U8 and U-design section 9 record the MSVC scheme as unexamined, and U09
// deliberately does not invent one: it diagnoses. This is the whole of the
// Microsoft story, and it is an honest error rather than a crash or a
// fabricated ABI.
// Only the first is pinned: CodeGen stops emitting once a mangling error is
// reported, so the second definition never reaches the mangler.
struct S {};
int operator⊞(S, S) { return 1; }
// msvc-error@-1 {{cannot mangle this Unicode user-defined operator yet}}
int operator⊖(S) { return 2; }

// A *declaration* is fine -- the name itself is representable, it is the
// symbol that is not -- so nothing here is rejected before codegen.
int operator⊗(S, S);

#else

struct S {};

// --- free infix: two parameters, arity 2 -----------------------------------
// CHECK-DAG: define {{.*}} @_Zv28op_u229E1SS_(
int operator⊞(S, S) { return 1; }

// --- free prefix: one parameter, arity 1 -----------------------------------
// CHECK-DAG: define {{.*}} @_Zv18op_u22961S(
int operator⊖(S) { return 2; }

// --- member infix: ONE declared parameter, arity 2 -------------------------
// This is the case the arity computation is really about. Before U09 the
// CXXUserOperatorName arm sat below the fallthrough that computes arity for
// CXXOperatorName, so a member operator arrived with UnknownArity.
struct T {
  int operator⊞(T) const;
  int operator⊖() const;
};
// CHECK-DAG: define {{.*}} @_ZNK1Tv28op_u229EES_(
int T::operator⊞(T) const { return 3; }
// --- member prefix: no declared parameters, arity 1 ------------------------
// CHECK-DAG: define {{.*}} @_ZNK1Tv18op_u2296Ev(
int T::operator⊖() const { return 4; }

// --- explicit object parameter (C++23): declared parameters are the operands
struct E {
  int operator⊗(this E, E);
  int operator⊘(this E);
};
// CHECK-DAG: define {{.*}} @_ZNH1Ev28op_u2297ES_S_(
int E::operator⊗(this E, E) { return 5; }
// CHECK-DAG: define {{.*}} @_ZNH1Ev18op_u2298ES_(
int E::operator⊘(this E) { return 6; }

// --- template instantiation ------------------------------------------------
template <class X> int operator⊠(X, X) { return 7; }
// CHECK-DAG: define {{.*}} @_Zv28op_u22A0IiEiT_S0_(
template int operator⊠<int>(int, int);

// --- namespace scope, and a nested-name substitution -----------------------
namespace N {
// CHECK-DAG: define {{.*}} @_ZN1Nv28op_u229FE1SS0_(
int operator⊟(S, S) { return 8; }
} // namespace N

// --- the hex-derivation rule across the U1 range ---------------------------
// Every U1 code point is in the BMP (0x2190-0x2BFF), so every derived name is
// exactly four hex digits today; these pin the two ends of the range and a
// code point whose hex has no letters.
// CHECK-DAG: define {{.*}} @_Zv28op_u21901SS_(
int operator←(S, S) { return 9; }
// CHECK-DAG: define {{.*}} @_Zv28op_u2BFF1SS_(
int operator⯿(S, S) { return 10; }
// CHECK-DAG: define {{.*}} @_Zv28op_u22681SS_(
int operator≨(S, S) { return 11; }

// --- spelling independence: U04/U11 ----------------------------------------
// The derivation reads the DeclarationName's code point; no spelling ever
// reaches the mangler. So a universal-character-name spelling needs no
// mangling code at all -- but that is a claim about identity, not about
// lexing, and it is exactly the claim that would fail silently if the UCN
// decoded to a different scalar value. Here the operator is *declared* with a
// named UCN and *defined* with the glyph: one symbol, and no second one.
// (--implicit-check-not=op_u0000 on the RUN lines is the specific guard: a
// spelling that failed to decode would yield code point 0 and mangle as
// `op_u0000`, and every dump and every -ast-print would still look correct.)
int operator\N{SQUARED PLUS}(S, S);

// --- an explicit call still resolves to the same symbol --------------------
// CHECK-DAG: define {{.*}} @_Z8call_allv(
// CHECK: call {{.*}} @_Zv28op_u229E1SS_(
// CHECK: call {{.*}} @_Zv18op_u22961S(
// ... including a call written with a UCN spelling, which must target the
// symbol the glyph-spelled definition emitted.
// CHECK: call {{.*}} @_Zv28op_u229E1SS_(
int call_all() {
  S s;
  return operator⊞(s, s) + operator⊖(s) +
         operator\N{SQUARED PLUS}(s, s);
}

// --- U-design section 9's disjointness claim -------------------------------
// A function *named* with an extended identifier -- Clang's D137051
// mathematical-notation identifier extension -- mangles as an ordinary
// <source-name>: a decimal byte length followed by the UTF-8 bytes. The
// operator names above are <operator-name>s beginning with `v`. A source-name
// begins with a digit and an operator-name with a letter, so the two
// productions cannot collide, which is what section 9 claims. (Under U10 the
// question is doubly moot: no code point is legal in both roles -- `⊞` is not
// an identifier character and `∂` U+2202 is excluded from U1 precisely
// because it is one.)
// CHECK-DAG: define {{.*}} @"_Z3\E2\88\82i"(
// (The extension warns -- "mathematical notation character '∂' U+2202 in an
// identifier is a C++2d extension" -- on stderr; these RUN lines FileCheck
// stdout, so it is not pinned here.)
int ∂(int x) { return x; }

#endif
