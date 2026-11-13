#include "TestHarness.h"
#include <cstring>

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int ran = 0, failedCases = 0;
  for (auto& c : th::cases()) {
    if (filter && !std::strstr(c.name, filter)) continue;
    int before = th::failures();
    c.fn();
    ++ran;
    bool ok = th::failures() == before;
    if (!ok) ++failedCases;
    std::printf("%s %s\n", ok ? "[ ok ]" : "[FAIL]", c.name);
  }
  std::printf("%d tests, %d failed\n", ran, failedCases);
  return failedCases == 0 ? 0 : 1;
}
