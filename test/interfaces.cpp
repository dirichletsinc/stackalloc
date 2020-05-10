#include "stackalloc/allocate.h"
#include "catch.hpp"

struct example_class {
  int a;
  float b;
  bool c;
  example_class(int a, float b, bool c) : a(a), b(b), c(c) {}
};

TEST_CASE("Single object interface works", "[short]") {
  auto obj = stackalloc::make_stack_ptr<example_class>(2, 2.4, false);
  static_assert(!std::is_copy_constructible_v<decltype(obj)>);
  static_assert(!std::is_move_constructible_v<decltype(obj)>);
  static_assert(!std::is_copy_assignable_v<decltype(obj)>);
  static_assert(!std::is_move_assignable_v<decltype(obj)>);

  REQUIRE(obj.get() != nullptr);
  REQUIRE(&(*obj) == obj.get());
  REQUIRE(obj->a == 2);
  REQUIRE(obj->b == 2.4f);
  REQUIRE(obj->c == false);
}

TEST_CASE("Array interface works", "[short]") {
  auto obj = stackalloc::make_stack_ptr<int[]>(1000);
  static_assert(!std::is_copy_constructible_v<decltype(obj)>);
  static_assert(!std::is_move_constructible_v<decltype(obj)>);
  static_assert(!std::is_copy_assignable_v<decltype(obj)>);
  static_assert(!std::is_move_assignable_v<decltype(obj)>);

  REQUIRE(obj.get() != nullptr);
  REQUIRE(obj.size() == 1000);
  for (size_t i = 0; i < std::size(obj); ++i)
    obj[i] = i;

  int count = 0;
  for (auto &x : obj)
    REQUIRE(x == count++);

  count = 0;
  for (const auto &x : obj)
    REQUIRE(x == count++);

  REQUIRE(obj.get() == obj.data());
}
