#include "catch.hpp"
#include "stackalloc/allocate.h"
#include <memory>

// Figure out cache line falling back to destructive interference size if no
// known cache line size is provided
#if defined(KNOWN_L1_CACHE_LINE_SIZE) && KNOWN_L1_CACHE_LINE_SIZE
constexpr std::size_t cache_line_size = KNOWN_L1_CACHE_LINE_SIZE;
#elif 0 // once compilers add support for this, check for the supporting version
constexpr std::size_t cache_line_size =
    std::hardware_constructive_interference_size;
#elif defined(__x86_64__) || defined(__i386__)
constexpr std::size_t cache_line_size = 64;
#else // this is a terrible fallback, but at least alignment will be no worse
      // than new
constexpr std::size_t cache_line_size = alignof(std::max_align_t);
#endif

struct example_class {
  int a;
  float b;
  bool c;
  example_class(int a, float b, bool c) : a(a), b(b), c(c) {}
};

TEST_CASE("Allocations are cache aligned", "[short]") {
  std::size_t space = 64;

  SECTION("Object allocations") {
    auto b = stackalloc::make_stack_ptr<bool>(false);
    auto obj0 = stackalloc::make_stack_ptr<example_class>(2, 2.4, false);
    auto obj1 = stackalloc::make_stack_ptr<example_class>(2, 2.4, false);

    auto p = reinterpret_cast<void *>(obj0.get());
    REQUIRE(std::align(cache_line_size, 1, p, space));
    REQUIRE(p == reinterpret_cast<void *>(obj0.get()));
    p = reinterpret_cast<void *>(obj1.get());
    REQUIRE(std::align(cache_line_size, 1, p, space));
    REQUIRE(p == reinterpret_cast<void *>(obj1.get()));
    p = reinterpret_cast<void *>(b.get());
    REQUIRE(std::align(cache_line_size, 1, p, space));
    REQUIRE(p == reinterpret_cast<void *>(b.get()));
  }

  SECTION("Array allocations") {
    auto a0 = stackalloc::make_stack_ptr<int[]>(1000);
    auto a1 = stackalloc::make_stack_ptr<double[]>(1500);

    auto p = reinterpret_cast<void *>(a0.get());
    REQUIRE(std::align(cache_line_size, 1, p, space));
    REQUIRE(p == reinterpret_cast<void *>(a0.get()));
    p = reinterpret_cast<void *>(a1.get());
    REQUIRE(std::align(cache_line_size, 1, p, space));
    REQUIRE(p == reinterpret_cast<void *>(a1.get()));
  }
}

TEST_CASE("Subsequent allocations should abutt", "[short]") {
  // first force a gigantic allocation to give ourselves lots of free space
  { auto a = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000); }
  auto a = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10);
  auto b = stackalloc::make_stack_ptr<int[]>(cache_line_size * 50);
  auto c = stackalloc::make_stack_ptr<int[]>(cache_line_size * 100);
  auto d = stackalloc::make_stack_ptr<double[]>(cache_line_size * 10);
  REQUIRE(a.end() == b.begin());
  REQUIRE(b.end() == c.begin());
  REQUIRE(c.end() == reinterpret_cast<int *>(d.begin()));
}

TEST_CASE("Deep nested allocations don't throw", "[short]") {
  auto a = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
  {
    auto a = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
    auto b = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
    auto c = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
    auto d = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
    {
      auto a = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
      auto b = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
      auto c = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);
      auto d = stackalloc::make_stack_ptr<int[]>(cache_line_size * 10000);

      REQUIRE_NOTHROW(a.begin()[0] = 1);
      REQUIRE_NOTHROW(a.end()[-1] = 2);
      REQUIRE(a[0] == 1);
      REQUIRE(a[a.size() - 1] == 2);

      REQUIRE_NOTHROW(d.begin()[0] = 3);
      REQUIRE_NOTHROW(d.end()[-1] = 4);
      REQUIRE(d[0] == 3);
      REQUIRE(d[d.size() - 1] == 4);
    }

    REQUIRE_NOTHROW(a.begin()[0] = 5);
    REQUIRE_NOTHROW(a.end()[-1] = 6);
    REQUIRE(a[0] == 5);
    REQUIRE(a[a.size() - 1] == 6);

    REQUIRE_NOTHROW(d.begin()[0] = 7);
    REQUIRE_NOTHROW(d.end()[-1] = 8);
    REQUIRE(d[0] == 7);
    REQUIRE(d[d.size() - 1] == 8);
  }

  REQUIRE_NOTHROW(a.begin()[0] = 9);
  REQUIRE_NOTHROW(a.end()[-1] = 0);
  REQUIRE(a[0] == 9);
  REQUIRE(a[a.size() - 1] == 0);
}
