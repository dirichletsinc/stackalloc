#include "allocate.h"
#include <cassert>
#include <memory>
#include <new>

namespace {
struct block_info {
  // The size of the previous block
  std::unique_ptr<char[]> previous_block;
  // The size of this block
  std::size_t size;
  // The next offset within our own block that can be allocated
  char *current_offset;
};

thread_local std::unique_ptr<char[]> current_block;
thread_local std::unique_ptr<char[]> spare_block;
thread_local size_t max_alloc_size = 64;

std::size_t round_up_to_power_of_2(std::size_t s) {
  s--;
  s |= s >> 1;
  s |= s >> 2;
  s |= s >> 4;
  if constexpr (sizeof s > 1)
    s |= s >> 8;
  if constexpr (sizeof s > 2)
    s |= s >> 16;
  if constexpr (sizeof s > 4)
    s |= s >> 32;
  s++;
  return s;
}

constexpr std::size_t round_to_cache_lines(std::size_t s) {
// Figure out cache line falling back to destructive interference size if no
// known cache line size is provided
#ifdef KNOWN_L1_CACHE_LINE_SIZE
  constexpr std::size_t cache_line_size = KNOWN_L1_CACHE_LINE_SIZE;
#else
  constexpr std::size_t cache_line_size =
      std::hardware_destructive_interference_size;
#endif
  return (s + cache_line_size - 1) / cache_line_size * cache_line_size;
}

block_info &get_block_info(std::unique_ptr<char[]> &block = current_block) {
  assert(block.get());
  return *reinterpret_cast<block_info *>(block.get());
}

bool has_space_for(std::size_t s,
                   std::unique_ptr<char[]> &block = current_block) {
  if (!block)
    return false;
  auto &info = get_block_info(block);
  return s <= info.size - (info.current_offset - block.get());
}

bool contains(char *p, std::unique_ptr<char[]> &block = current_block) {
  if (!block)
    return false;
  auto &info = get_block_info(block);
  return p >= block.get() && (p - block.get()) < info.size;
}
} // namespace

void stackalloc::detail::deallocate(char *p) {
  if (!p)
    return;
  if (!current_block)
    throw("deallocated unmanaged memory");

  if (contains(p)) {
    // Simply rewind the current offset to this pointer
    if (p < get_block_info().current_offset)
      get_block_info().current_offset = p;
    return;
  }

  // deallocate the whole block if p isn't in it
  auto previous_block = std::move(get_block_info().previous_block);
  // Overwrite the spare block if the one we're removing is bigger
  if (!spare_block || get_block_info(spare_block).size < get_block_info().size)
    spare_block = std::move(current_block);
  current_block = std::move(previous_block);
  deallocate(p);
}

char *stackalloc::detail::allocate(std::size_t s) {
  auto alloc_size = round_to_cache_lines(s);

  if (has_space_for(alloc_size)) {
    auto &info = get_block_info();
    auto ret_ptr = info.current_offset;
    info.current_offset += alloc_size;
    return ret_ptr;
  }

  // Try to use the spare block and avoid an unnecessary allocation
  if (has_space_for(alloc_size, spare_block)) {
    get_block_info(spare_block).previous_block = std::move(current_block);
    current_block = std::move(spare_block);
    return allocate(s);
  }

  // Make sure that we could produce at least four allocations in a block
  // without hitting the backing allocation implementation
  if (max_alloc_size <
      (alloc_size * 4 + round_to_cache_lines(sizeof(block_info))) * 2) {
    auto new_max_alloc_size = round_up_to_power_of_2(
        alloc_size * 4 + round_to_cache_lines(sizeof(block_info)));
    if (max_alloc_size < new_max_alloc_size)
      max_alloc_size = new_max_alloc_size;
  }

  auto new_block = std::make_unique<char[]>(max_alloc_size);
  auto &info = get_block_info(new_block);
  info.previous_block = std::move(current_block);
  info.size = max_alloc_size;
  info.current_offset =
      new_block.get() + round_to_cache_lines(sizeof(block_info));
  current_block = std::move(new_block);
  return allocate(s);
}
