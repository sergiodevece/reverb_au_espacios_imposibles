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
  auto h = run(e, 2.5, {{0.0, 0.1}}, {0.2, 1.9, 2.2});
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
// Holding while the source still plays would pile energy up without limit: hold only once it stops.
TEST(Tail_notHeldWhileSourcePlays) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(2.0f);
  auto h = run(e, 1.5, {{0.0, 1.0}}, {0.5, 0.95, 1.2});
  CHECK_NEAR(h[0], 0.0, 1e-6); CHECK_NEAR(h[1], 0.0, 1e-6); CHECK_NEAR(h[2], 1.0, 1e-6);
}

// --- Attack, release curve ---
namespace {
struct Probe { float hold, inputGain, releaseScale; };
std::vector<Probe> runFull(ei::TailEnvelope& e, double seconds, const std::vector<std::pair<double, double>>& hits, const std::vector<double>& probes) {
  std::vector<Probe> out; size_t p = 0;
  for (int i = 0; i < int(seconds * kSr); ++i) {
    double t = i / kSr; float lvl = 0;
    for (auto& hit : hits) if (t >= hit.first && t < hit.second) lvl = 0.5f;
    float h = e.process(lvl);
    if (p < probes.size() && t >= probes[p]) { out.push_back({h, e.inputGain(), e.releaseScale()}); ++p; }
  }
  return out;
}
}

TEST(Tail_noAttackMeansFullInput) {
  ei::TailEnvelope e; e.prepare(kSr); e.setAttack(0);
  auto o = runFull(e, 1.0, {{0.1, 0.9}}, {0.05, 0.1001, 0.5});
  for (auto& s : o) CHECK_NEAR(s.inputGain, 1.0, 1e-6);
}
TEST(Tail_attackRampsInputFromEachOnset) {
  ei::TailEnvelope e; e.prepare(kSr); e.setAttack(0.5f);
  auto o = runFull(e, 3.0, {{0.1, 1.0}, {2.0, 2.9}}, {0.11, 0.35, 0.7, 2.01, 2.25, 2.7});
  CHECK(o[0].inputGain < 0.05f); CHECK_NEAR(o[1].inputGain, 0.5, 0.05); CHECK_NEAR(o[2].inputGain, 1.0, 1e-3);
  CHECK(o[3].inputGain < 0.05f); CHECK_NEAR(o[4].inputGain, 0.5, 0.05); CHECK_NEAR(o[5].inputGain, 1.0, 1e-3);
}
TEST(Tail_neutralCurveKeepsReleaseRate) {
  ei::TailEnvelope e; e.prepare(kSr); e.setRelease(2.0f); e.setCurve(0);
  auto o = runFull(e, 4.0, {{0.0, 0.5}}, {0.2, 0.6, 1.5, 2.4, 3.5});
  for (auto& s : o) CHECK_NEAR(s.releaseScale, 1.0, 1e-3);
}
// curve +1: the decay barely moves at first and collapses at the end. curve -1: the opposite.
TEST(Tail_curveReshapesTheRelease) {
  ei::TailEnvelope up; up.prepare(kSr); up.setRelease(2.0f); up.setCurve(1.0f);
  auto a = runFull(up, 4.0, {{0.0, 0.5}}, {0.3, 1.0, 2.3, 3.0});       // source stops at 0.5 s: u = 0.25, 0.9, beyond the end
  CHECK_NEAR(a[0].releaseScale, 1.0, 1e-3);                              // still playing: plain release
  CHECK(a[1].releaseScale < 0.15f);
  CHECK(a[2].releaseScale > 2.0f);
  CHECK(a[3].releaseScale >= 1.0f);
  ei::TailEnvelope down; down.prepare(kSr); down.setRelease(2.0f); down.setCurve(-1.0f);
  auto b = runFull(down, 4.0, {{0.0, 0.5}}, {0.52, 2.3});
  CHECK(b[0].releaseScale > 3.0f);
  CHECK(b[1].releaseScale < 0.6f);
}
TEST(Tail_curveWaitsForTheSustainToEnd) {
  ei::TailEnvelope e; e.prepare(kSr); e.setSustain(1.0f); e.setRelease(2.0f); e.setCurve(1.0f);
  auto o = runFull(e, 3.0, {{0.0, 0.5}}, {1.0, 1.7});
  CHECK_NEAR(o[0].hold, 1.0, 1e-6);                                      // sustaining
  CHECK(o[1].hold < 0.01f); CHECK(o[1].releaseScale < 0.15f);            // early in the release: barely decaying
}
