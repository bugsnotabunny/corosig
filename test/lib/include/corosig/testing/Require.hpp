#pragma once

#define SIG_REQUIRE(expr)                                                                          \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      _exit(EXIT_FAILURE);                                                                         \
    }                                                                                              \
  } while (false)
