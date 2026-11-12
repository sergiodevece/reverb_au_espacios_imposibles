#include "TestHarness.h"
#include "dsp/TailEnvelope.h"

namespace {
const double kSr = 48000;
// level(t) -> hold(t); returns hold sampled at the given times.
std::vector<float> run(ei::TailEnvelope& e, double seconds, const std::vector<std::pair<double, double>>& hits,
                       const std::vector<double>& probes) {
  std::vector<float> out;
  size_t p = 0; float h = 0;
  for (int i = 0; i < int(seconds * kSr); ++i) {
    double t = i / kSr; float lvl = 0;
    for (auto& hit : hits) if (t >= hit.first && t < hit.second) lvl = 0.5f;
    h = e.process(lvl);
    if (p < probes.size() && t >= probes[p]) { out.push_back(h); ++p; }
  }
  return out;
}
}

TEST(Tail_zeroSustainIsZero) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(0);
  auto h = run(e, 1.0, {{0.0, 0.5}}, {0.05, 0.3, 0.9});
  for (float v : h) CHECK_NEAR(v, 0.0, 1e-6);
}
TEST(Tail_holdsThenReleases) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(2.0f);
  auto h = run(e, 2.5, {{0.0, 0.1}}, {0.05, 1.9, 2.2});
  CHECK_NEAR(h[0], 1.0, 1e-6); CHECK_NEAR(h[1], 1.0, 1e-6); CHECK_NEAR(h[2], 0.0, 1e-6);
}
TEST(Tail_retriggers) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(2.0f);
  auto h = run(e, 3.5, {{0.0, 0.1}, {1.0, 1.1}}, {2.5, 3.0, 3.3});
  CHECK_NEAR(h[0], 1.0, 1e-6); CHECK_NEAR(h[1], 1.0, 1e-6); CHECK_NEAR(h[2], 0.0, 1e-6);
}
TEST(Tail_ignoresNoiseFloor) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(2.0f);
  float h = 0; for (int i = 0; i < 48000; ++i) h = e.process(0.001f);   // -60 dBFS, below the -50 dB threshold
  CHECK_NEAR(h, 0.0, 1e-6);
}
