// Minimal dependency-free test harness.
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace th {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, std::function<void()> f) { cases().push_back({n, std::move(f)}); } };

inline void fail(const char* file, int line, const std::string& msg) {
  ++failures();
  std::printf("    FAIL %s:%d  %s\n", file, line, msg.c_str());
}

inline float rmsDb(const float* x, int n) {
  double s = 0;
  for (int i = 0; i < n; ++i) s += double(x[i]) * x[i];
  return float(10.0 * std::log10(s / (n > 0 ? n : 1) + 1e-30));
}
inline float rmsDb(const std::vector<float>& v, int from, int count) { return rmsDb(v.data() + from, count); }
}  // namespace th

#define TEST(name)                                              \
  static void test_##name();                                    \
  static th::Registrar reg_##name(#name, test_##name);          \
  static void test_##name()

#define CHECK(cond) \
  do { if (!(cond)) th::fail(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_NEAR(a, b, tol)                                                             \
  do {                                                                                    \
    double _a = double(a), _b = double(b), _t = double(tol);                              \
    if (!(std::fabs(_a - _b) <= _t))                                                      \
      th::fail(__FILE__, __LINE__, std::string(#a " ~= " #b "  got ") +                   \
               std::to_string(_a) + " vs " + std::to_string(_b) + " (tol " + std::to_string(_t) + ")"); \
  } while (0)
