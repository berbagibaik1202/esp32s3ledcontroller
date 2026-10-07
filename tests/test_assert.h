#pragma once
#include <cstdio>
#include <cstdlib>

// Use a terminal-only assertion so failing tests cannot block on a GUI dialog.
#ifdef assert
#undef assert
#endif
#define assert(condition) do { \
  if (!(condition)) { \
    std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
    std::exit(EXIT_FAILURE); \
  } \
} while (false)
