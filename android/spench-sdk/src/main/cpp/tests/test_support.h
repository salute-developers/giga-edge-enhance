#ifndef SPENCH_TEST_SUPPORT_H
#define SPENCH_TEST_SUPPORT_H

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

inline void expect_true(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(1);
  }
}

inline void expect_near(float actual, float expected, float tolerance, const std::string& message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " expected=" << expected << " actual=" << actual << std::endl;
    std::exit(1);
  }
}

#endif
