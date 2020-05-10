#include "allocate.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>

namespace {

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

constexpr std::size_t round_to_cache_lines(std::size_t s) {
  return (s + cache_line_size - 1) / cache_line_size * cache_line_size;
}

struct block {
private:
  struct block_info {
    // The underlying ptr to be deleted
    void *underlying_ptr;
    // The size of the previous block
    char *previous_block;
    // The size of this block
    std::size_t size;
    // The next offset within our own block that can be allocated
    char *current_offset;
  };

  char *aligned_alloc;

  block_info &get_info() {
    return *reinterpret_cast<block_info *>(aligned_alloc);
  }

  const block_info &get_info() const {
    return *reinterpret_cast<const block_info *>(aligned_alloc);
  }

  block(char *p) : aligned_alloc(p) {}

public:
  block() : aligned_alloc(nullptr) {}
  block(block &&b) : aligned_alloc(std::exchange(b.aligned_alloc, nullptr)) {}
  block(const block &b) = delete;
  block(std::size_t size, block &&b) {
    // Add an extra cache line for the alignment
    auto extended_size =
        size + cache_line_size + round_to_cache_lines(sizeof(block_info));
    void *alloc = ::operator new(extended_size);
    void *aligned_alloc_void = alloc;
    if (!std::align(cache_line_size, size, aligned_alloc_void, extended_size)) {
      ::operator delete(alloc);
      throw std::bad_alloc();
    }
    aligned_alloc = reinterpret_cast<char *>(aligned_alloc_void);
    auto &info = get_info();
    info.underlying_ptr = alloc;
    info.previous_block = std::exchange(b.aligned_alloc, nullptr);
    info.size = extended_size;
    info.current_offset = round_to_cache_lines(sizeof(block_info)) +
                          reinterpret_cast<char *>(aligned_alloc);
  }
  block(std::size_t size) : block(size, block()) {}

  block &operator=(block &&other) {
    aligned_alloc = std::exchange(other.aligned_alloc, nullptr);
    return *this;
  }
  block &operator=(const block &other) = delete;

  ~block() {
    if (aligned_alloc) {
      block inner_block(get_info().previous_block);
      ::operator delete(get_info().underlying_ptr);
    }
  }

  operator bool() { return aligned_alloc; }

  char *alloc(std::size_t s) {
    auto &info = get_info();
    if (!(aligned_alloc &&
          s <= info.size - (info.current_offset - aligned_alloc)))
      return nullptr;
    auto ret_ptr = info.current_offset;
    info.current_offset += s;
    return ret_ptr;
  }

  bool dealloc(char *p) {
    auto &info = get_info();
    if (!(aligned_alloc && p >= aligned_alloc &&
          std::size_t(p - aligned_alloc) < info.size))
      return false;
    if (p < info.current_offset)
      info.current_offset = p;
    return true;
  }

  std::size_t size() const { return get_info().size; }

  block previous_block() {
    return {std::exchange(get_info().previous_block, nullptr)};
  }

  void push_block(block &&b) {
    get_info().previous_block =
        std::exchange(b.aligned_alloc, get_info().previous_block);
  }
};

thread_local block current_block;
thread_local block spare_block;
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

} // namespace

void stackalloc::detail::deallocate(char *p) {
  if (!p)
    return;
  while (current_block) {
    if (current_block.dealloc(p))
      return;

    // deallocate the whole block if p isn't in it
    auto previous_block = current_block.previous_block();
    // Overwrite the spare block if the one we're removing is bigger
    if (!spare_block || spare_block.size() < current_block.size())
      spare_block = std::move(current_block);
    current_block = std::move(previous_block);
  }
  throw("deallocated unmanaged memory");
}

char *stackalloc::detail::allocate(std::size_t s) {
  auto alloc_size = round_to_cache_lines(s);

  if (auto ptr = current_block.alloc(alloc_size))
    return ptr;

  // Try to use the spare block and avoid an unnecessary allocation
  if (auto ptr = spare_block.alloc(alloc_size)){
    spare_block.push_block(std::move(current_block));
    current_block = std::move(spare_block);
    return ptr;
  }

  // Make sure that we could produce at least four allocations in a block
  // without hitting the backing allocation implementation
  if (max_alloc_size < (alloc_size * 4) * 2) {
    auto new_max_alloc_size = round_up_to_power_of_2(alloc_size * 4);
    if (max_alloc_size < new_max_alloc_size)
      max_alloc_size = new_max_alloc_size;
  }

  block new_block(max_alloc_size, std::move(current_block));
  current_block = std::move(new_block);
  return allocate(s);
}
