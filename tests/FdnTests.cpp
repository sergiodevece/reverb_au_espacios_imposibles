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
  for (float size : {1.0f, 1000.0f}) for (float damp : {-1.0f, 1.0f}) for (float pitch : {-12.0f, 12.0f}) {
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
