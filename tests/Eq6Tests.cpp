#include "TestHarness.h"
#include "dsp/Eq6.h"

namespace {
const double kSr = 48000;
float sineGainDb(ei::Eq6& eq, double freq) {
  const int n = 48000, warm = 24000; std::vector<float> in(n), out(n);
  for (int i = 0; i < n; ++i) { float x = 0.25f * float(std::sin(2 * M_PI * freq * i / kSr)); float l = x, r = x; eq.process(l, r); in[i] = x; out[i] = l; }
  return th::rmsDb(out, warm, n - warm) - th::rmsDb(in, warm, n - warm);
}
ei::EqBand band(bool on, float f, float g, float q = 0.707f) { ei::EqBand b; b.on = on; b.freq = f; b.gainDb = g; b.q = q; return b; }
}

TEST(Eq_flatByDefault) {
  ei::Eq6 eq; eq.prepare(kSr);
  for (float f : {50.0f, 200.0f, 1000.0f, 5000.0f, 15000.0f}) CHECK_NEAR(eq.magnitudeDb(f), 0.0, 0.1);
  CHECK_NEAR(sineGainDb(eq, 1000), 0.0, 0.05);
}
TEST(Eq_bellGainAtCenter) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand(3, band(true, 1500, 12, 1.0f));
  float g = sineGainDb(eq, 1500);
  CHECK_NEAR(g, 12.0, 0.5); CHECK_NEAR(eq.magnitudeDb(1500), g, 0.2);
}
TEST(Eq_lowCutSlope) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand(0, band(true, 200, 0));
  CHECK_NEAR(sineGainDb(eq, 100), -12.0, 2.0);
  CHECK_NEAR(sineGainDb(eq, 2000), 0.0, 0.3);
}
TEST(Eq_lowShelf) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand(1, band(true, 150, -9));
  CHECK_NEAR(sineGainDb(eq, 30), -9.0, 0.8);
}
TEST(Eq_highShelf) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand(5, band(true, 10000, 6));
  CHECK_NEAR(sineGainDb(eq, 18000), 6.0, 0.7);
}
TEST(Eq_highCutMode) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand6HighCut(true); eq.setBand(5, band(true, 5000, 6));
  CHECK(sineGainDb(eq, 10000) < -10.0f);
  CHECK(eq.magnitudeDb(10000) < -10.0f);
}
TEST(Eq_bandOffIsBypass) {
  ei::Eq6 eq; eq.prepare(kSr); eq.setBand(2, band(false, 400, 18, 2.0f));
  CHECK_NEAR(sineGainDb(eq, 400), 0.0, 0.05); CHECK_NEAR(eq.magnitudeDb(400), 0.0, 0.05);
}
TEST(Eq_clampsBadValues) {
  ei::Eq6 eq; eq.prepare(kSr);
  eq.setBand(2, band(true, 1e9f, 500.0f, 0.0f)); eq.setBand(3, band(true, -5.0f, -500.0f, 1e9f)); eq.setBand(9, band(true, 100, 1));
  CHECK(std::isfinite(sineGainDb(eq, 1000)));
  CHECK(eq.magnitudeDb(1000) <= 18.5f * 2);
}
