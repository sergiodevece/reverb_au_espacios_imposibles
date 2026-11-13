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

// reset() must be instant (the host may call it on the audio thread) yet leave nothing audible behind.
TEST(DelayLine_resetForgetsOldAudioWithoutClearing) {
  ei::DelayLine d; d.prepare(1 << 16);
  for (int i = 0; i < 70000; ++i) d.write(1.0f);
  d.reset();
  for (int delay : {0, 1, 100, 5000, 65000}) { CHECK_NEAR(d.readInt(delay), 0.0, 0.0); CHECK_NEAR(d.read(float(delay) + 0.5f), 0.0, 0.0); }
  d.write(0.5f);
  CHECK_NEAR(d.readInt(0), 0.5, 1e-9);
  CHECK_NEAR(d.readInt(1), 0.0, 0.0);
  for (int i = 0; i < 200; ++i) d.write(0.25f);
  CHECK_NEAR(d.read(100.5f), 0.25, 1e-6);
  CHECK_NEAR(d.readInt(5000), 0.0, 0.0);
}

TEST(DelayLine_linearReadInterpolates) {
  ei::DelayLine d; d.prepare(256);
  for (int i = 0; i < 100; ++i) d.write(float(i));                 // a ramp: linear interpolation is exact on it
  CHECK_NEAR(d.readLinear(0.0f), 99.0, 1e-5);
  CHECK_NEAR(d.readLinear(10.25f), 88.75, 1e-4);
  CHECK_NEAR(d.readLinear(-3.0f), 99.0, 1e-5);
  CHECK(std::isfinite(d.readLinear(1e9f)));
  d.reset();
  CHECK_NEAR(d.readLinear(5.5f), 0.0, 0.0);
}
