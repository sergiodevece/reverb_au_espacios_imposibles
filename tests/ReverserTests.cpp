#include "TestHarness.h"
#include "dsp/Reverser.h"
#include <random>

namespace {
const double kSr = 48000;
}

// A decaying burst (loud start, quiet end) must come out as a swell (quiet start, loud end):
// around the output peak, the energy sits before it, not after it.
static void burstEnergyAroundPeak(const std::vector<float>& y, double& before, double& after) {
  size_t p = 0; for (size_t i = 0; i < y.size(); ++i) if (std::fabs(y[i]) > std::fabs(y[p])) p = i;
  before = after = 0;
  for (size_t i = 1; i <= 150; ++i) {
    if (p >= i) before += double(y[p - i]) * y[p - i];
    if (p + i < y.size()) after += double(y[p + i]) * y[p + i];
  }
}
TEST(Rev_reversesBurst) {
  ei::Reverser r; r.prepare(kSr); r.setWindowSamples(1000); r.reset();
  std::vector<float> in, out;
  for (int i = 0; i < 6000; ++i) {
    float l = (i >= 2100 && i < 2300) ? float(std::exp(-(i - 2100) / 30.0) * std::cos((i - 2100) * 0.9)) : 0.0f, rr = l;
    in.push_back(l); r.process(l, rr); out.push_back(l);
  }
  double before, after;
  burstEnergyAroundPeak(in, before, after);
  CHECK(after > 5.0 * before);          // sanity: the source really is a decaying burst
  burstEnergyAroundPeak(out, before, after);
  CHECK(before > 5.0 * after);
}
TEST(Rev_unityLevel) {
  std::mt19937 rng(4); std::uniform_real_distribution<float> u(-1, 1);
  ei::Reverser r; r.prepare(kSr); r.setWindowSamples(4800); r.reset();
  std::vector<float> in, out;
  for (int i = 0; i < 96000; ++i) { float l = u(rng), rr = l; in.push_back(l); r.process(l, rr); out.push_back(l); }
  CHECK_NEAR(th::rmsDb(out, 24000, 72000), th::rmsDb(in, 24000, 72000), 1.5);
}
TEST(Rev_latencyReported) {
  ei::Reverser r; r.prepare(kSr);
  r.setWindowSamples(24000); CHECK(r.latencySamples() == 24000);
}
TEST(Rev_windowChangeNoGlitch) {
  ei::Reverser r; r.prepare(kSr); r.setWindowSamples(500);
  float prev = 0, maxJump = 0;
  for (int i = 0; i < 48000; ++i) {
    if (i == 10000) r.setWindowSamples(5000);
    if (i == 30000) r.setWindowSamples(700);
    float l = 0.5f * float(std::sin(2 * M_PI * 200 * i / kSr)), rr = l;
    r.process(l, rr);
    maxJump = std::fmax(maxJump, std::fabs(l - prev)); prev = l;
  }
  CHECK(maxJump < 0.5f);
}
TEST(Rev_clampsWindow) {
  ei::Reverser r; r.prepare(kSr, 1.0f);
  for (int w : {0, -100, 1 << 30}) {
    r.setWindowSamples(w);
    CHECK(r.latencySamples() >= 64); CHECK(r.latencySamples() <= 48000);
    for (int i = 0; i < 200000; ++i) { float l = 0.3f, rr = -0.3f; r.process(l, rr); CHECK(std::isfinite(l) && std::isfinite(rr)); }
  }
}
