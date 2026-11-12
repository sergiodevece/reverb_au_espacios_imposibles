#include "TestHarness.h"
#include "dsp/Gate.h"

namespace {
const double kSr = 48000;
float db(float d) { return std::pow(10.0f, d / 20.0f); }
float run(ei::Gate& g, float level, double ms) { float v = 1; for (int i = 0; i < int(ms * 0.001 * kSr); ++i) v = g.process(level); return v; }
}

TEST(Gate_opensAboveThreshold) {
  ei::Gate g; g.prepare(kSr); g.setParams(-30, 2, 0, 50, -60);
  run(g, 0.0f, 500);
  CHECK(!g.isOpen());
  CHECK(run(g, db(-20), 10) >= 0.99f);
  CHECK(g.isOpen());
}
TEST(Gate_closesToRange) {
  ei::Gate g; g.prepare(kSr); g.setParams(-30, 2, 100, 50, -60);
  run(g, db(-20), 50);
  CHECK(run(g, 0.0f, 100 + 5 * 50) <= 0.0011f);
  CHECK(!g.isOpen());
}
TEST(Gate_holdKeepsOpen) {
  ei::Gate g; g.prepare(kSr); g.setParams(-30, 2, 200, 50, -60);
  run(g, db(-20), 50);
  CHECK(run(g, 0.0f, 150) >= 0.99f);
}
TEST(Gate_hysteresis) {
  ei::Gate open; open.prepare(kSr); open.setParams(-30, 2, 0, 50, -60);
  run(open, db(-20), 50);
  CHECK(run(open, db(-32), 500) >= 0.99f);
  ei::Gate closed; closed.prepare(kSr); closed.setParams(-30, 2, 0, 50, -60);
  run(closed, 0.0f, 500);
  CHECK(run(closed, db(-32), 500) <= 0.0011f);
}
TEST(Gate_rangeZeroIsTransparent) {
  ei::Gate g; g.prepare(kSr); g.setParams(-30, 2, 0, 50, 0);
  CHECK_NEAR(run(g, 0.0f, 500), 1.0, 1e-4);
}
