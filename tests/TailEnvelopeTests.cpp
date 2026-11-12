#include "TestHarness.h"
#include "dsp/TailEnvelope.h"

namespace {
const double kSr = 48000;
struct Probe { float rate, hold, inputGain; };
// Source plays at level 0.5 during each [start, end) interval; state is sampled at the probe times.
std::vector<Probe> run(ei::TailEnvelope& e, double seconds, const std::vector<std::pair<double, double>>& hits, const std::vector<double>& probes) {
  std::vector<Probe> out; size_t p = 0;
  for (int i = 0; i < int(seconds * kSr); ++i) {
    double t = i / kSr; float lvl = 0;
    for (auto& hit : hits) if (t >= hit.first && t < hit.second) lvl = 0.5f;
    e.process(lvl);
    if (p < probes.size() && t >= probes[p]) { out.push_back({e.decayRate(), e.hold(), e.inputGain()}); ++p; }
  }
  return out;
}
void setPoints(ei::TailEnvelope& e, std::initializer_list<ei::TailPoint> pts) { std::vector<ei::TailPoint> v(pts); e.setPoints(v.data()); }
}

TEST(Tail_defaultIsAPlainFourSecondDecay) {
  ei::TailEnvelope e; e.prepare(kSr);
  auto o = run(e, 6.0, {{0.5, 1.0}}, {0.2, 0.7, 1.5, 3.0, 4.5});
  CHECK_NEAR(o[1].rate, 15.0, 0.1); CHECK_NEAR(o[2].rate, 15.0, 0.1); CHECK_NEAR(o[3].rate, 15.0, 0.1);
  CHECK(o[0].rate >= 15.0f); CHECK(o[4].rate >= 15.0f);          // before anything played and after the curve ended: still decaying
  for (auto& s : o) CHECK_NEAR(s.hold, 0.0, 1e-6);
}

// Hold, fall 30 dB, hold, rise 12 dB, fall to silence.
TEST(Tail_eachSegmentSetsItsOwnRate) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{1.0f, 0.0f}, {2.0f, -30.0f}, {3.0f, -30.0f}, {3.5f, -18.0f}, {5.0f, -60.0f}});
  auto o = run(e, 7.5, {{0.0, 0.5}}, {1.0, 2.0, 3.0, 3.75, 4.5, 6.5});     // the source stops at 0.5 s
  CHECK_NEAR(o[0].rate, 0.0, 0.05);    CHECK_NEAR(o[0].hold, 1.0, 1e-6);
  CHECK_NEAR(o[1].rate, 30.0, 0.3);    CHECK_NEAR(o[1].hold, 0.0, 1e-6);
  CHECK_NEAR(o[2].rate, 0.0, 0.05);    CHECK_NEAR(o[2].hold, 1.0, 1e-6);
  CHECK_NEAR(o[3].rate, -24.0, 0.3);   CHECK_NEAR(o[3].hold, 1.0, 1e-6);    // growing
  CHECK_NEAR(o[4].rate, 28.0, 0.3);
  CHECK(o[5].rate >= 20.0f);                                                // past the last point: keeps dying
}

// Holding while the source still plays would pile energy up without limit.
TEST(Tail_curveOnlyStartsWhenTheSourceStops) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{2.0f, 0.0f}, {3.0f, -15.0f}, {4.0f, -30.0f}, {5.0f, -45.0f}, {6.0f, -60.0f}});
  auto o = run(e, 2.0, {{0.0, 1.0}}, {0.5, 0.95, 1.2});
  CHECK_NEAR(o[0].rate, 10.0, 0.1); CHECK_NEAR(o[0].hold, 0.0, 1e-6);       // playing: the curve's average slope, 60 dB in 6 s
  CHECK_NEAR(o[1].hold, 0.0, 1e-6);
  CHECK_NEAR(o[2].rate, 0.0, 0.05); CHECK_NEAR(o[2].hold, 1.0, 1e-6);       // stopped: now it holds
}

TEST(Tail_everyHitRestartsTheCurve) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{1.0f, 0.0f}, {2.0f, -15.0f}, {3.0f, -30.0f}, {4.0f, -45.0f}, {5.0f, -60.0f}});
  auto o = run(e, 3.0, {{0.0, 0.1}, {1.5, 1.6}}, {1.4, 2.4});
  CHECK_NEAR(o[0].rate, 15.0, 0.2);        // 1.3 s after the first hit: already falling
  CHECK_NEAR(o[1].rate, 0.0, 0.05);        // 0.8 s after the second: holding again
}

TEST(Tail_endingAboveSilenceHoldsForever) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{0.5f, -5.0f}, {1.0f, -10.0f}, {1.5f, -15.0f}, {2.0f, -18.0f}, {2.5f, -20.0f}});
  auto o = run(e, 12.0, {{0.0, 0.5}}, {11.0});
  CHECK_NEAR(o[0].rate, 0.0, 0.01); CHECK_NEAR(o[0].hold, 1.0, 1e-6);
}

TEST(Tail_ignoresNoiseFloor) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{2.0f, 0.0f}, {3.0f, -15.0f}, {4.0f, -30.0f}, {5.0f, -45.0f}, {6.0f, -60.0f}});
  for (int i = 0; i < 48000; ++i) e.process(0.001f);                         // -60 dBFS, below the -50 dB threshold
  CHECK_NEAR(e.hold(), 0.0, 1e-6);
}

TEST(Tail_toleratesImpossiblePoints) {
  ei::TailEnvelope e; e.prepare(kSr);
  setPoints(e, {{60.0f, 500.0f}, {30.0f, -500.0f}, {-4.0f, 12.0f}, {1.0f, NAN}, {1.0f, -60.0f}});   // out of order, out of range, vertical
  auto o = run(e, 5.0, {{0.0, 0.5}}, {0.6, 1.0, 1.6, 3.0, 4.9});
  for (auto& s : o) { CHECK(std::isfinite(s.rate)); CHECK(s.rate >= -60.0f); CHECK(s.rate <= 600.0f); }
}

TEST(Tail_noAttackMeansFullInput) {
  ei::TailEnvelope e; e.prepare(kSr); e.setAttack(0);
  auto o = run(e, 1.0, {{0.1, 0.9}}, {0.05, 0.1001, 0.5});
  for (auto& s : o) CHECK_NEAR(s.inputGain, 1.0, 1e-6);
}
TEST(Tail_attackRampsInputFromEachOnset) {
  ei::TailEnvelope e; e.prepare(kSr); e.setAttack(0.5f);
  auto o = run(e, 3.0, {{0.1, 1.0}, {2.0, 2.9}}, {0.11, 0.35, 0.7, 2.01, 2.25, 2.7});
  CHECK(o[0].inputGain < 0.05f); CHECK_NEAR(o[1].inputGain, 0.5, 0.05); CHECK_NEAR(o[2].inputGain, 1.0, 1e-3);
  CHECK(o[3].inputGain < 0.05f); CHECK_NEAR(o[4].inputGain, 0.5, 0.05); CHECK_NEAR(o[5].inputGain, 1.0, 1e-3);
}
