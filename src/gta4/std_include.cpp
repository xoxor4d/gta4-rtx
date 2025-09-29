#include "std_include.hpp"

// Ensure pointers are 4 bytes in size (32-bit)
static_assert(sizeof(intptr_t) == 4 && sizeof(void*) == 4 && sizeof(size_t) == 4, "This doesn't seem to be a 32-bit environment!");
