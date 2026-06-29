// Pipeline-shaped idioms built on the backtick infix operator (design §16).
// Self-contained and constexpr: no standard library, so this is a fast,
// portable regression on the *backtick feature surface* those idioms rely on
// — slot-as-arbitrary-callable, left-associative chaining, and a
// short-circuiting operator whose right operand is a thunk.  The §16.2
// interaction with the real std::ranges adaptor closures and the
// std::optional/and_then form of §16.5 are validated against libstdc++
// separately (design §16.8).

// RUN: %clang_cc1 -std=c++20 -fbacktick -fsyntax-only -verify %s
// expected-no-diagnostics

// Helpers — each a few lines, as in §16 (here without std::invoke/forward).
inline constexpr auto pipe =
    [](auto&& x, auto&& f) constexpr -> decltype(auto) { return f(x); };          // §16.1

inline constexpr auto then =
    [](auto f, auto g) constexpr {
      return [=](auto&&... a) constexpr -> decltype(auto) { return g(f(a...)); };
    };                                                                            // §16.4

inline constexpr auto implies =
    [](bool p, auto&& q) constexpr -> bool { return !p || q(); };                 // §16.5

inline constexpr auto mbind =
    [](auto&& m, auto&& f) constexpr { return m.and_then(f); };                   // §16.5

constexpr auto inc = [](int x) constexpr { return x + 1; };
constexpr auto dbl = [](int x) constexpr { return x * 2; };
constexpr auto neg = [](int x) constexpr { return -x; };

// §16.1 threading:  x `pipe` f `pipe` g `pipe` h  ==  h(g(f(x)))
static_assert((3 `pipe` inc `pipe` dbl `pipe` neg) == -8);

// §16.2 driving a callable adaptor-closure object.  Range adaptor closures are
// exactly objects with operator(); model that minimally.
struct AddN { int n; constexpr int operator()(int x) const { return x + n; } };
static_assert((10 `pipe` AddN{5}) == 15);

// §16.3 parameterized stage (a capturing closure, as bind_back would produce).
constexpr auto add_k = [](int k) constexpr { return [k](int x) constexpr { return x + k; }; };
static_assert((100 `pipe` add_k(7)) == 107);

// §16.4 reusable point-free composition.
constexpr auto pipeline = inc `then` dbl `then` neg;   // neg(dbl(inc(x)))
static_assert(pipeline(3) == -8);

// §16.5a short-circuiting operator: the RHS thunk is skipped when p is false.
// `boom` reads a mutable global, so it is *not* a constant expression if
// evaluated; that it isn't marked constexpr and these asserts still hold
// *proves* the right operand is not evaluated on the short-circuit path.
int runtime_only = 1;
auto boom = []() -> bool { return runtime_only != 0; };
static_assert(false `implies` boom);                     // p false -> RHS skipped -> true
static_assert(true `implies` []() constexpr { return true; });    // p true -> RHS runs -> true
static_assert(!(true `implies` []() constexpr { return false; })); // p true -> RHS runs -> false

// §16.5b monadic chain over a minimal optional; short-circuits on empty.
template <class T> struct Opt {
  bool has = false;
  T val{};
  constexpr Opt() = default;
  constexpr Opt(T v) : has(true), val(v) {}
  template <class F> constexpr auto and_then(F f) const {
    return has ? f(val) : decltype(f(val)){};
  }
};
constexpr auto parse    = [](int s) constexpr -> Opt<int> { return s > 0  ? Opt<int>(s) : Opt<int>(); };
constexpr auto validate = [](int x) constexpr -> Opt<int> { return x < 100 ? Opt<int>(x) : Opt<int>(); };
constexpr auto store    = [](int x) constexpr -> Opt<int> { return Opt<int>(x + 1); };
static_assert((parse(5) `mbind` validate `mbind` store).val == 6);
static_assert(!(parse(-1) `mbind` validate `mbind` store).has);   // short-circuits to empty
