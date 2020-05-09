#include "allocate.h"

void stackalloc::detail::deallocate(char *p) { delete[] p; }
char *stackalloc::detail::allocate(std::size_t s) { return new char[s]; }
