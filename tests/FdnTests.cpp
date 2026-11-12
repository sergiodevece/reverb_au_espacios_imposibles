#include "TestHarness.h"
#include "dsp/Fdn.h"
#include <random>

namespace {
const double kSr = 48000;

// RT60 from the decay slope between two 100 ms windows.
double measureRt60(const std::vector<float>& y, double sr, double t1, double t2) {
  int w = int(0.1 * sr);
  double l1 = th::rmsDb(y, int(t1 * sr), w), l2 = th::rmsDb(y, int(t2 * sr), w);
  return 60.0 * (t2 - t1) / (l1 - l2);
}
std::vector<float> impulseResponse(ei::Fdn& f, double seconds, std::vector<float>* right = nullptr) {
  int n = int(seconds * kSr);
  std::vector<float> l(n);
  if (right) right->resize(n);
  for (int i = 0; i < n; ++i) {
    float x = i == 0 ? 1.0f : 0.0f, a, b;
    f.process(x, x, a, b);
    l[i] = a; if (right) (*right)[i] = b;
  }
  return l;
}
}

TEST(Fdn_rt60MatchesRelease) {
  for (float rt : {0.5f, 2.0f, 8.0f}) {
    ei::Fdn f; f.prepare(kSr);
    f.setSize(30); f.setDamping(0); f.setDiffusion(0.7f); f.setRelease(rt, false);
    auto y = impulseResponse(f, rt * 1.0 + 0.5);
    double m = measureRt60(y, kSr, 0.25 * rt, 0.8 * rt);
    CHECK_NEAR(m, rt, 0.2 * rt);
  }
}

TEST(Fdn_holdSustains) {
  ei::Fdn f; f.prepare(kSr);
  f.setSize(30); f.setDamping(-0.5f); f.setRelease(1.0f, false); f.setHold(1.0f);
  auto y = impulseResponse(f, 5.5);
  float a = th::rmsDb(y, int(1.0 * kSr), 24000), b = th::rmsDb(y, int(4.5 * kSr), 24000);
  CHECK(a > -80.0f);
  CHECK_NEAR(a, b, 1.0);
}

TEST(Fdn_infiniteReleaseDoesNotDecay) {
  ei::Fdn f; f.prepare(kSr);
  f.setSize(30); f.setDamping(0); f.setRelease(1.0f, true);
  auto y = impulseResponse(f, 5.5);
  CHECK_NEAR(th::rmsDb(y, int(1.0 * kSr), 24000), th::rmsDb(y, int(4.5 * kSr), 24000), 1.0);
}

TEST(Fdn_freezeIgnoresInput) {
  std::mt19937 rng(1); std::uniform_real_distribution<float> u(-1, 1);
  ei::Fdn a, b; a.prepare(kSr); b.prepare(kSr);
  for (auto* f : {&a, &b}) { f->setSize(20); f->setRelease(2.0f, false); }
  float l, r, l2, r2;
  for (int i = 0; i < 24000; ++i) { float x = u(rng); a.process(x, x, l, r); b.process(x, x, l2, r2); }
  a.setFreeze(true); b.setFreeze(true);
  for (int i = 0; i < 4800; ++i) { a.process(0, 0, l, r); b.process(0, 0, l2, r2); }   // let the freeze ramp settle
  double maxDiff = 0, energy = 0;
  for (int i = 0; i < 96000; ++i) {
    float x = u(rng);
    a.process(x, x, l, r); b.process(0, 0, l2, r2);
    maxDiff = std::fmax(maxDiff, std::fabs(l - l2)); energy += double(l) * l;
  }
  CHECK(maxDiff < 1e-6);
  CHECK(energy > 1.0);   // and it is still sounding
}

TEST(Fdn_boundedAtExtremes) {
  std::mt19937 rng(2); std::uniform_real_distribution<float> u(-1, 1);
  for (float size : {1.0f, 1000.0f}) for (float damp : {-1.0f, 1.0f}) for (float pitch : {-24.0f, 24.0f}) {
    ei::Fdn f; f.prepare(kSr);
    f.setSize(size); f.setDamping(damp); f.setPitch(pitch, 1.0f); f.setRelease(60.0f, true); f.setHold(1.0f); f.setWidth(2.0f);
    float peak = 0; bool finite = true;
    for (int i = 0; i < int(10 * kSr); ++i) {
      float l, r; f.process(u(rng), u(rng), l, r);
      finite = finite && std::isfinite(l) && std::isfinite(r);
      peak = std::fmax(peak, std::fmax(std::fabs(l), std::fabs(r)));
    }
    CHECK(finite);
    CHECK(peak <= 8.0f);
  }
}

TEST(Fdn_stereoDecorrelated) {
  ei::Fdn f; f.prepare(kSr); f.setSize(30); f.setRelease(3.0f, false);
  std::vector<float> r; auto l = impulseResponse(f, 2.0, &r);
  double sLR = 0, sLL = 0, sRR = 0;
  for (size_t i = 4800; i < l.size(); ++i) { sLR += double(l[i]) * r[i]; sLL += double(l[i]) * l[i]; sRR += double(r[i]) * r[i]; }
  CHECK(std::fabs(sLR / std::sqrt(sLL * sRR)) < 0.5);

  ei::Fdn m; m.prepare(kSr); m.setSize(30); m.setRelease(3.0f, false); m.setWidth(0.0f);
  std::vector<float> mr; auto ml = impulseResponse(m, 1.0, &mr);
  double d = 0; for (size_t i = 0; i < ml.size(); ++i) d = std::fmax(d, std::fabs(ml[i] - mr[i]));
  CHECK(d < 1e-6);
}

TEST(Fdn_silenceDecaysToZero) {
  ei::Fdn f; f.prepare(kSr); f.setSize(30); f.setDamping(-0.3f); f.setRelease(0.5f, false);
  auto y = impulseResponse(f, 20.0);
  float peak = 0; for (size_t i = y.size() - 4800; i < y.size(); ++i) peak = std::fmax(peak, std::fabs(y[i]));
  CHECK(peak < 1e-15f);
}

TEST(Fdn_sizeChangesAreFinite) {
  std::mt19937 rng(3); std::uniform_real_distribution<float> u(-1, 1);
  ei::Fdn f; f.prepare(kSr); f.setRelease(4.0f, false);
  bool finite = true; float peak = 0;
  for (int i = 0; i < 480000; ++i) {
    if (i % 4800 == 0) { f.setSize(1.0f + 999.0f * std::fabs(u(rng))); f.setSizeMod(0.5f * u(rng)); }
    float l, r; f.process(u(rng), u(rng), l, r);
    finite = finite && std::isfinite(l) && std::isfinite(r); peak = std::fmax(peak, std::fabs(l));
  }
  CHECK(finite); CHECK(peak <= 8.0f);
}

// Brightening must not turn the loop into an oscillator: the tail still dies within the release time.
TEST(Fdn_tailDecaysAcrossDampingRange) {
  for (float damp : {-1.0f, -0.3f, 0.0f, 0.3f, 0.8f, 1.0f}) {
    ei::Fdn f; f.prepare(kSr); f.setSize(8); f.setDamping(damp); f.setRelease(2.0f, false);
    std::mt19937 rng(9); std::uniform_real_distribution<float> u(-0.1f, 0.1f);
    std::vector<float> y; float l, r;
    for (int i = 0; i < int(0.2 * kSr); ++i) { float x = u(rng); f.process(x, x, l, r); y.push_back(l); }
    for (int i = 0; i < int(5.0 * kSr); ++i) { f.process(0, 0, l, r); y.push_back(l); }
    float early = th::rmsDb(y, int(0.1 * kSr), 4800), late = th::rmsDb(y, int(4.9 * kSr), 4800);
    CHECK(late < early - 60.0f);
  }
}


// Bender: while held, the level can be made to sink or to grow at a set rate.
TEST(Fdn_holdRateBendsTheSustain) {
  for (float rate : {6.0f, -3.0f}) {                       // dB per second; negative = the tail grows
    ei::Fdn f; f.prepare(kSr); f.setSize(30); f.setDamping(0); f.setRelease(1.0f, false); f.setHold(1.0f); f.setHoldRate(rate);
    std::vector<float> y; float l, r;
    for (int i = 0; i < int(3.6 * kSr); ++i) { float x = i == 0 ? 0.05f : 0.0f; f.process(x, x, l, r); y.push_back(l); }
    float a = th::rmsDb(y, int(1.0 * kSr), 24000), b = th::rmsDb(y, int(3.0 * kSr), 24000);
    CHECK_NEAR(a - b, 2.0 * rate, 1.5);
  }
}
TEST(Fdn_releaseScaleSpeedsUpOrSlowsTheDecay) {
  ei::Fdn f; f.prepare(kSr); f.setSize(30); f.setDamping(0); f.setRelease(2.0f, false); f.setReleaseScale(2.0f);
  auto y = impulseResponse(f, 1.5);
  CHECK_NEAR(measureRt60(y, kSr, 0.25, 0.8), 1.0, 0.2);
  ei::Fdn g; g.prepare(kSr); g.setSize(30); g.setDamping(0); g.setRelease(2.0f, false); g.setReleaseScale(0.5f);
  auto z = impulseResponse(g, 4.5);
  CHECK_NEAR(measureRt60(z, kSr, 1.0, 3.2), 4.0, 0.8);
}

// --- Density ---
namespace {
// Normalised echo density (Abel & Huang): share of samples beyond one standard deviation, relative to
// what Gaussian noise gives. ~0 for a few isolated echoes, ~1 for a fully dense tail.
double echoDensity(const std::vector<float>& y, int from, int count) {
  double s = 0; for (int i = from; i < from + count; ++i) s += double(y[size_t(i)]) * y[size_t(i)];
  const double sigma = std::sqrt(s / count);
  if (sigma < 1e-12) return 0.0;
  int outside = 0; for (int i = from; i < from + count; ++i) if (std::fabs(y[size_t(i)]) > sigma) ++outside;
  return (double(outside) / count) / 0.3173;
}
std::vector<float> densityResponse(float density, double seconds, bool hold = false) {
  ei::Fdn f; f.prepare(kSr);
  f.setSize(30); f.setDamping(0); f.setDiffusion(0); f.setRelease(3.0f, false); f.setDensity(density);
  if (hold) f.setHold(1.0f);
  f.reset();                                   // apply the density from the first sample instead of gliding to it
  return impulseResponse(f, seconds);
}
}

TEST(Fdn_densityControlsEchoDensity) {
  const int from = int(0.15 * kSr), count = int(0.10 * kSr);          // 150-250 ms after the impulse
  const double sparse = echoDensity(densityResponse(0.0f, 0.3), from, count);
  const double normal = echoDensity(densityResponse(0.5f, 0.3), from, count);
  const double dense = echoDensity(densityResponse(1.0f, 0.3), from, count);
  CHECK(sparse < 0.25);                       // separate, countable echoes
  CHECK(normal > sparse + 0.2);
  CHECK(dense > normal + 0.05);               // denser than the plain network
  CHECK(dense > 0.8);
}

TEST(Fdn_densityMidpointIsThePlainNetwork) {
  ei::Fdn a, b; a.prepare(kSr); b.prepare(kSr);
  for (auto* f : {&a, &b}) { f->setSize(30); f->setRelease(3.0f, false); }
  b.setDensity(0.5f);
  auto ya = impulseResponse(a, 0.5), yb = impulseResponse(b, 0.5);
  double d = 0; for (size_t i = 0; i < ya.size(); ++i) d = std::fmax(d, std::fabs(ya[i] - yb[i]));
  CHECK(d == 0.0);                            // the default must not change the sound already approved
}

TEST(Fdn_densityKeepsEnergyAndDecay) {
  for (float d : {0.0f, 0.25f, 0.75f, 1.0f}) {
    auto held = densityResponse(d, 4.5, true);
    CHECK_NEAR(th::rmsDb(held, int(1.0 * kSr), 24000), th::rmsDb(held, int(4.0 * kSr), 24000), 1.0);   // lossless at any density
    ei::Fdn f; f.prepare(kSr); f.setSize(30); f.setDamping(0); f.setRelease(2.0f, false); f.setDensity(d); f.reset();
    auto y = impulseResponse(f, 2.5);
    CHECK_NEAR(measureRt60(y, kSr, 0.5, 1.6), 2.0, 0.5);
  }
}

TEST(Fdn_densitySweepIsSafe) {
  std::mt19937 rng(21); std::uniform_real_distribution<float> u(-1, 1);
  ei::Fdn f; f.prepare(kSr); f.setRelease(4.0f, false);
  bool finite = true; float peak = 0;
  for (int i = 0; i < 480000; ++i) {
    if (i % 2400 == 0) { f.setDensity(std::fabs(u(rng))); f.setSize(1.0f + 400.0f * std::fabs(u(rng))); f.setSizeMod(0.5f * u(rng)); }
    float l, r; f.process(u(rng), u(rng), l, r);
    finite = finite && std::isfinite(l) && std::isfinite(r); peak = std::fmax(peak, std::fabs(l));
  }
  CHECK(finite); CHECK(peak <= 8.0f);
}
