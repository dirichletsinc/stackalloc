#include <type_traits>
#include <utility>

namespace stackalloc {

namespace detail {
void deallocate(char *p);
char *allocate(std::size_t);
} // namespace detail

// Forward decls for friend functions
template <typename T> class stack_ptr;
template <
    typename T,
    typename = std::enable_if_t<!std::is_abstract_v<T> && !std::is_function_v<T> &&
                                !std::is_array_v<T>>,
    class... Args>
stack_ptr<T> make_stack_ptr(Args &&... args);
template <typename T,
          typename = std::enable_if_t<!std::is_abstract_v<T> &&
                                      !std::is_function_v<T> && std::is_array_v<T>>>
stack_ptr<T> make_stack_ptr(std::size_t size);

// A class for a managed allocation (object variation)
// Thse objects cannot be copied, and will deallocate themselves at the end of
// the scope they were allocated in
template <typename T> class stack_ptr {
public:
  using pointer = T *;
  using element_type = T;

private:
  // The underlying pointer
  pointer p;

  // Constructs a stack_ptr from a raw pointer and size
  stack_ptr(pointer p) : p(p) {}
  stack_ptr(stack_ptr &&s) = default;
  stack_ptr &&operator=(stack_ptr &&s) = delete;
  stack_ptr(const stack_ptr &s) = delete;
  stack_ptr &operator=(const stack_ptr &s) = delete;

  // This friend function needs access to the constructors to perform allocation
  template <class... Args> friend stack_ptr<T> make_stack_ptr(Args &&...);

public:
  ~stack_ptr() { detail::deallocate(reinterpret_cast<char *>(p)); }
  // Observers:

  // Returns a pointer to the managed object
  pointer get() const noexcept { return p; }

  // Provides access to the managed object
  typename std::add_lvalue_reference<T>::type operator&() const { return *p; }
  pointer operator->() const noexcept { return p; }
};

// stack_ptr specialization for array types
template <typename T> class stack_ptr<T[]> {
public:
  using pointer = T *;
  using element_type = T;

private:
  // The underlying pointer
  pointer p;

  // The size of the allocated block (1 for
  std::size_t s;

  // Constructs a stack_ptr from a raw pointer and size
  stack_ptr(pointer p, std::size_t s) : p(p), s(s) {}
  stack_ptr &&operator=(stack_ptr &&s) = delete;
  stack_ptr(const stack_ptr &s) = delete;
  stack_ptr &operator=(const stack_ptr &s) = delete;

  // This friend function needs access to the constructors to perform allocation
  friend stack_ptr<T> make_stack_ptr(std::size_t);

public:
  ~stack_ptr() { detail::deallocate(reinterpret_cast<char *>(p)); }

  // Observers:

  // Returns a pointer to the managed object
  pointer get() const noexcept { return p; }
  pointer data() const noexcept { return get(); }

  // Returns the number of elements in the allocation
  std::size_t size() const noexcept { return s; }

  // Provides access to elements of managed array
  T &operator[](std::size_t i) const { return get()[i]; }

  // Iterators:
  pointer begin() const noexcept { return p; }
  const pointer cbegin() const noexcept { return begin(); }
  pointer end() const noexcept { return p + s; }
  const pointer cend() const noexcept { return end(); }
};

// Allocates and constructs stack_ptr from provided arguments
// (drop in replacement to std::make_unique)
template <typename T, typename, class... Args>
stack_ptr<T> make_stack_ptr(Args &&... args) {
  return {new (detail::allocate(sizeof(T))) T(std::forward<Args>(args)...)};
}
template <typename T, typename> stack_ptr<T> make_stack_ptr(std::size_t size) {
  return {reinterpret_cast<typename stack_ptr<T>::pointer>(
              detail::allocate(sizeof(stack_ptr<T>::element_type) * size)),
          size};
}

} // namespace stackalloc
