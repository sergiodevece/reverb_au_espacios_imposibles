#include "TestHarness.h"
#include "dsp/Lfo.h"

namespace {
const double kSr = 48000;
// Seconds per cycle, from rising zero crossings.
double period(ei::Lfo& l, double seconds) {
  int first = -1, last = -1, count = 0; float prev = l.process(0);
  for (int i = 1; i < int(seconds * kSr); ++i) {
    float v = l.process(0);
    if (prev <= 0 && v > 0) { if (first < 0) first = i; last = i; ++count; }
    prev = v;
  }
  return count > 1 ? (last - first) / kSr / (count - 1) : 0.0;
}
}

TEST(Lfo_sineFrequency) {
  ei::Lfo l; l.prepare(kSr); l.setMode(ei::Lfo::Mode::Manual); l.setShape(ei::Lfo::Shape::Sine); l.setRateHz(2.0f); l.setDepth(1.0f);
  CHECK_NEAR(period(l, 10.0), 0.5, 0.005);
}
TEST(Lfo_shapesRange) {
  using S = ei::Lfo::Shape;
  for (S s : {S::Sine, S::Triangle, S::Saw, S::Square, S::Random}) {
    ei::Lfo l; l.prepare(kSr); l.setMode(ei::Lfo::Mode::Manual); l.setShape(s); l.setRateHz(5.0f); l.setDepth(0.6f);
    float lo = 0, hi = 0;
    for (int i = 0; i < int(60 * kSr); ++i) { float v = l.process(0); lo = std::fmin(lo, v); hi = std::fmax(hi, v); }
    CHECK(hi <= 0.6f + 1e-4f); CHECK(lo >= -0.6f - 1e-4f);
    CHECK(hi >= 0.95f * 0.6f); CHECK(lo <= -0.95f * 0.6f);
  }
}
TEST(Lfo_syncFollowsTempo) {
  ei::Lfo l; l.prepare(kSr); l.setMode(ei::Lfo::Mode::Manual); l.setDepth(1.0f); l.setSync(true, 4.0f); l.setTempo(120.0);
  CHECK_NEAR(period(l, 12.0), 2.0, 0.02);
  l.setTempo(60.0);
  CHECK_NEAR(period(l, 20.0), 4.0, 0.04);
}
TEST(Lfo_autoDucksWithInput) {
  ei::Lfo l; l.prepare(kSr); l.setMode(ei::Lfo::Mode::Auto); l.setDepth(0.8f); l.setTempo(120.0);
  float peak = 0;
  for (int i = 0; i < int(1 * kSr); ++i) l.process(1.0f);
  for (int i = 0; i < int(4 * kSr); ++i) peak = std::fmax(peak, std::fabs(l.process(1.0f)));
  CHECK(peak < 0.05f * 0.8f);
  for (int i = 0; i < int(1 * kSr); ++i) l.process(0.0f);
  peak = 0;
  for (int i = 0; i < int(4 * kSr); ++i) peak = std::fmax(peak, std::fabs(l.process(0.0f)));
  CHECK(peak >= 0.95f * 0.8f);
}
TEST(Lfo_zeroTempoFallsBack) {
  for (double bpm : {0.0, -1.0, 1e9}) {
    ei::Lfo l; l.prepare(kSr); l.setMode(ei::Lfo::Mode::Manual); l.setDepth(1.0f); l.setSync(true, 4.0f); l.setTempo(bpm);
    double p = period(l, 12.0);
    CHECK(std::isfinite(p));
    if (bpm <= 0) CHECK_NEAR(p, 2.0, 0.02);
  }
}
TEST(Lfo_divisionTable) {
  CHECK_NEAR(ei::Lfo::divisionToBeats(0), 32.0, 1e-6);
  CHECK_NEAR(ei::Lfo::divisionToBeats(3), 4.0, 1e-6);
  CHECK_NEAR(ei::Lfo::divisionToBeats(5), 1.0, 1e-6);
  CHECK_NEAR(ei::Lfo::divisionToBeats(8), 0.125, 1e-6);
  CHECK_NEAR(ei::Lfo::divisionToBeats(-4), 32.0, 1e-6);
  CHECK_NEAR(ei::Lfo::divisionToBeats(99), 0.125, 1e-6);
}
