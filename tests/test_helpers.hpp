#pragma once

#include <iostream>

#define TEST_CHECK(condition)                                                      \
  do {                                                                             \
    if (!(condition)) {                                                            \
      std::cerr << "CHECK FAILED: " << #condition << " at " << __FILE__ << ":"    \
                << __LINE__ << std::endl;                                          \
      return 1;                                                                    \
    }                                                                              \
  } while (false)
