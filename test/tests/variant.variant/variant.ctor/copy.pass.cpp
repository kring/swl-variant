// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// UNSUPPORTED: c++03, c++11, c++14

// Throwing bad_variant_access is supported starting in macosx10.13
// XFAIL: with_system_cxx_lib=macosx10.12 && !no-exceptions
// XFAIL: with_system_cxx_lib=macosx10.11 && !no-exceptions
// XFAIL: with_system_cxx_lib=macosx10.10 && !no-exceptions
// XFAIL: with_system_cxx_lib=macosx10.9 && !no-exceptions

// <variant>

// template <class ...Types> class variant;

// variant(variant const&); // constexpr in C++20

 
#include <type_traits>
#include <swl_assert.hpp> 
#include <variant.hpp> 

#include "test_macros.h"
#include "test_workarounds.h"

struct NonT {
  NonT(int v) : value(v) {}
  NonT(const NonT &o) : value(o.value) {}
  int value;
};
static_assert(!std::is_trivially_copy_constructible<NonT>::value, "");

struct NoCopy {
  NoCopy(const NoCopy &) = delete;
};

struct MoveOnly {
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) = default;
};

struct MoveOnlyNT {
  MoveOnlyNT(const MoveOnlyNT &) = delete;
  MoveOnlyNT(MoveOnlyNT &&) {}
};

struct NTCopy {
  constexpr NTCopy(int v) : value(v) {}
  NTCopy(const NTCopy &that) : value(that.value) {}
  NTCopy(NTCopy &&) = delete;
  int value;
};

static_assert(!std::is_trivially_copy_constructible<NTCopy>::value, "");
static_assert(std::is_copy_constructible<NTCopy>::value, "");

struct TCopy {
  constexpr TCopy(int v) : value(v) {}
  TCopy(TCopy const &) = default;
  TCopy(TCopy &&) = delete;
  int value;
};

static_assert(std::is_trivially_copy_constructible<TCopy>::value, "");

struct TCopyNTMove {
  constexpr TCopyNTMove(int v) : value(v) {}
  TCopyNTMove(const TCopyNTMove&) = default;
  TCopyNTMove(TCopyNTMove&& that) : value(that.value) { that.value = -1; }
  int value;
};

static_assert(std::is_trivially_copy_constructible<TCopyNTMove>::value, "");

#ifndef TEST_HAS_NO_EXCEPTIONS
struct MakeEmptyT {
  static int alive;
  MakeEmptyT() { ++alive; }
  MakeEmptyT(const MakeEmptyT &) {
    ++alive;
    // Don't throw from the copy constructor since variant's assignment
    // operator performs a copy before committing to the assignment.
  }
  MakeEmptyT(MakeEmptyT &&) { throw 42; }
  MakeEmptyT &operator=(const MakeEmptyT &) { throw 42; }
  MakeEmptyT &operator=(MakeEmptyT &&) { throw 42; }
  ~MakeEmptyT() { --alive; }
};

int MakeEmptyT::alive = 0;

template <class Variant> void makeEmpty(Variant &v) {
  Variant v2(swl::in_place_type<MakeEmptyT>);
  try {
    v = std::move(v2);
    SWL_ASSERT(false);
  } catch (...) {
    SWL_ASSERT(v.valueless_by_exception());
  }
}
#endif // TEST_HAS_NO_EXCEPTIONS

void test_copy_ctor_sfinae() {
  {
    using V = swl::variant<int, long>;
    static_assert(std::is_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, NoCopy>;
    static_assert(!std::is_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, MoveOnly>;
    static_assert(!std::is_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, MoveOnlyNT>;
    static_assert(!std::is_copy_constructible<V>::value, "");
  }

  // Make sure we properly propagate triviality (see P0602R4).
#if TEST_STD_VER > 17
  {
    using V = swl::variant<int, long>;
    static_assert(std::is_trivially_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, NTCopy>;
    static_assert(!std::is_trivially_copy_constructible<V>::value, "");
    static_assert(std::is_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, TCopy>;
    static_assert(std::is_trivially_copy_constructible<V>::value, "");
  }
  {
    using V = swl::variant<int, TCopyNTMove>;
    static_assert(std::is_trivially_copy_constructible<V>::value, "");
  }
#endif // > C++17
}

void test_copy_ctor_basic() {
  {
    swl::variant<int> v(swl::in_place_index<0>, 42);
    swl::variant<int> v2 = v;
    SWL_ASSERT(v2.index() == 0);
    SWL_ASSERT(swl::get<0>(v2) == 42);
  }
  {
    swl::variant<int, long> v(swl::in_place_index<1>, 42);
    swl::variant<int, long> v2 = v;
    SWL_ASSERT(v2.index() == 1);
    SWL_ASSERT(swl::get<1>(v2) == 42);
  }
  {
    swl::variant<NonT> v(swl::in_place_index<0>, 42);
    SWL_ASSERT(v.index() == 0);
    swl::variant<NonT> v2(v);
    SWL_ASSERT(v2.index() == 0);
    SWL_ASSERT(swl::get<0>(v2).value == 42);
  }
  {
    swl::variant<int, NonT> v(swl::in_place_index<1>, 42);
    SWL_ASSERT(v.index() == 1);
    swl::variant<int, NonT> v2(v);
    SWL_ASSERT(v2.index() == 1);
    SWL_ASSERT(swl::get<1>(v2).value == 42);
  }

  // Make sure we properly propagate triviality, which implies constexpr-ness (see P0602R4).
#if TEST_STD_VER > 17
  {
    constexpr swl::variant<int> v(swl::in_place_index<0>, 42);
    static_assert(v.index() == 0, "");
    constexpr swl::variant<int> v2 = v;
    static_assert(v2.index() == 0, "");
    static_assert(swl::get<0>(v2) == 42, "");
  }
  {
    constexpr swl::variant<int, long> v(swl::in_place_index<1>, 42);
    static_assert(v.index() == 1, "");
    constexpr swl::variant<int, long> v2 = v;
    static_assert(v2.index() == 1, "");
    static_assert(swl::get<1>(v2) == 42, "");
  }
  {
    constexpr swl::variant<TCopy> v(swl::in_place_index<0>, 42);
    static_assert(v.index() == 0, "");
    constexpr swl::variant<TCopy> v2(v);
    static_assert(v2.index() == 0, "");
    static_assert(swl::get<0>(v2).value == 42, "");
  }
  {
    constexpr swl::variant<int, TCopy> v(swl::in_place_index<1>, 42);
    static_assert(v.index() == 1, "");
    constexpr swl::variant<int, TCopy> v2(v);
    static_assert(v2.index() == 1, "");
    static_assert(swl::get<1>(v2).value == 42, "");
  }
  {
    constexpr swl::variant<TCopyNTMove> v(swl::in_place_index<0>, 42);
    static_assert(v.index() == 0, "");
    constexpr swl::variant<TCopyNTMove> v2(v);
    static_assert(v2.index() == 0, "");
    static_assert(swl::get<0>(v2).value == 42, "");
  }
  {
    constexpr swl::variant<int, TCopyNTMove> v(swl::in_place_index<1>, 42);
    static_assert(v.index() == 1, "");
    constexpr swl::variant<int, TCopyNTMove> v2(v);
    static_assert(v2.index() == 1, "");
    static_assert(swl::get<1>(v2).value == 42, "");
  }
#endif // > C++17
}

void test_copy_ctor_valueless_by_exception() {
#ifndef TEST_HAS_NO_EXCEPTIONS
  using V = swl::variant<int, MakeEmptyT>;
  V v1;
  makeEmpty(v1);
  const V &cv1 = v1;
  V v(cv1);
  SWL_ASSERT(v.valueless_by_exception());
#endif // TEST_HAS_NO_EXCEPTIONS
}

template <size_t Idx>
constexpr bool test_constexpr_copy_ctor_imp(swl::variant<long, void*, const int> const& v) {
  auto v2 = v;
  return v2.index() == v.index() &&
         v2.index() == Idx &&
         swl::get<Idx>(v2) == swl::get<Idx>(v);
}

void test_constexpr_copy_ctor() {
  // Make sure we properly propagate triviality, which implies constexpr-ness (see P0602R4).
#if TEST_STD_VER > 17
  using V = swl::variant<long, void*, const int>;
#ifdef TEST_WORKAROUND_C1XX_BROKEN_IS_TRIVIALLY_COPYABLE
  static_assert(std::is_trivially_destructible<V>::value, "");
  static_assert(std::is_trivially_copy_constructible<V>::value, "");
  static_assert(std::is_trivially_move_constructible<V>::value, "");
  static_assert(!std::is_copy_assignable<V>::value, "");
  static_assert(!std::is_move_assignable<V>::value, "");
#else // TEST_WORKAROUND_C1XX_BROKEN_IS_TRIVIALLY_COPYABLE
  static_assert(std::is_trivially_copyable<V>::value, "");
#endif // TEST_WORKAROUND_C1XX_BROKEN_IS_TRIVIALLY_COPYABLE
  static_assert(test_constexpr_copy_ctor_imp<0>(V(42l)), "");
  static_assert(test_constexpr_copy_ctor_imp<1>(V(nullptr)), "");
  static_assert(test_constexpr_copy_ctor_imp<2>(V(101)), "");
#endif // > C++17
}

int main(int, char**) {
  test_copy_ctor_basic();
  test_copy_ctor_valueless_by_exception();
  test_copy_ctor_sfinae();
  test_constexpr_copy_ctor();
#if 0
// disable this for the moment; it fails on older compilers.
//  Need to figure out which compilers will support it.
{ // This is the motivating example from P0739R0
  swl::variant<int, double> v1(3);
  swl::variant v2 = v1;
  (void) v2;
}
#endif

  SWL_END_TEST_SIGNAL 
 return 0; 

}