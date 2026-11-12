#include "TestHarness.h"
#include "dsp/DelayLine.h"
#include "dsp/EnvelopeFollower.h"

TEST(DelayLine_integerDelay) {
  ei::DelayLine d; d.prepare(1024);
  for (int i = 0; i < 300; ++i) {
    d.write(i == 0 ? 1.0f : 0.0f);
    float y = d.readInt(100);
    if (i == 100) CHECK_NEAR(y, 1.0, 1e-6); else CHECK_NEAR(y, 0.0, 1e-6);
  }
}

TEST(DelayLine_fractionalDelay) {
  const double sr = 48000, f = 1000, w = 2 * M_PI * f / sr;
  ei::DelayLine d; d.prepare(1024);
  double maxErr = 0;
  for (int i = 0; i < 2000; ++i) {
    d.write(float(std::sin(w * i)));
    float y = d.read(10.5f);
    if (i > 100) maxErr = std::fmax(maxErr, std::fabs(y - std::sin(w * (i - 10.5))));
  }
  CHECK(maxErr < 1e-3);
}

TEST(DelayLine_clampsOutOfRange) {
  ei::DelayLine d; d.prepare(256);
  for (int i = 0; i < 1000; ++i) {
    d.write(float(i % 7) * 0.1f);
    CHECK(std::isfinite(d.read(-5.0f)));
    CHECK(std::isfinite(d.read(1e9f)));
    CHECK(std::isfinite(d.readInt(-3)));
    CHECK(std::isfinite(d.readInt(1 << 30)));
  }
}

TEST(EnvelopeFollower_tracksLevel) {
  const double sr = 48000;
  ei::EnvelopeFollower e; e.prepare(sr); e.setTimes(1.0f, 50.0f);
  float lvl = 0;
  for (int i = 0; i < 24000; ++i) lvl = e.process(0.5f * float(std::sin(2 * M_PI * 1000 * i / sr)));
  CHECK_NEAR(lvl, 0.5, 0.05);
  for (int i = 0; i < 24000; ++i) lvl = e.process(0.0f);
  CHECK(lvl < 0.01f);
}
