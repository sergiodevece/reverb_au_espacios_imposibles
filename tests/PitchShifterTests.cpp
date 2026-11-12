#include "TestHarness.h"
#include "dsp/PitchShifter.h"

namespace {
const double kSr = 48000;
// Runs a sine through the shifter; returns estimated output frequency (zero crossings) and RMS in dB.
void runSine(float semis, double freq, double& outFreq, float& outRmsDb, float& inRmsDb) {
  ei::PitchShifter p; p.prepare(kSr); p.setSemitones(semis);
  const int warm = 24000, n = 48000;
  std::vector<float> in(warm + n), out(warm + n);
  for (int i = 0; i < warm + n; ++i) { in[i] = 0.5f * float(std::sin(2 * M_PI * freq * i / kSr)); out[i] = p.process(in[i]); }
  int crossings = 0;
  for (int i = warm + 1; i < warm + n; ++i) if (out[i - 1] <= 0 && out[i] > 0) ++crossings;
  outFreq = crossings * kSr / n;
  outRmsDb = th::rmsDb(out, warm, n); inRmsDb = th::rmsDb(in, warm, n);
}
}

TEST(Pitch_octaveUp) {
  double f; float o, i; runSine(12, 440, f, o, i);
  CHECK_NEAR(f, 880.0, 880.0 * 0.03);
}
TEST(Pitch_fifthDown) {
  double f; float o, i; runSine(-7, 440, f, o, i);
  CHECK_NEAR(f, 293.66, 293.66 * 0.03);
}
TEST(Pitch_twoOctavesUp) {
  double f; float o, i; runSine(24, 220, f, o, i);
  CHECK_NEAR(f, 880.0, 880.0 * 0.03);
  CHECK_NEAR(o, i, 3.0);
}
TEST(Pitch_twoOctavesDown) {
  double f; float o, i; runSine(-24, 880, f, o, i);
  CHECK_NEAR(f, 220.0, 220.0 * 0.03);
  CHECK_NEAR(o, i, 3.0);
}
TEST(Pitch_unityGain) {
  for (float st : {12.0f, -7.0f, 3.0f, -12.0f}) {
    double f; float o, i; runSine(st, 440, f, o, i);
    CHECK_NEAR(o, i, 3.0);
  }
}
TEST(Pitch_zeroIsClean) {
  ei::PitchShifter p; p.prepare(kSr); p.setSemitones(0);
  const int d = p.delaySamples();
  CHECK(d > 0);
  std::vector<float> in(20000);
  double maxErr = 0;
  for (int i = 0; i < 20000; ++i) {
    in[i] = 0.5f * float(std::sin(2 * M_PI * 440 * i / kSr));
    float y = p.process(in[i]);
    if (i > d + 4800) maxErr = std::fmax(maxErr, std::fabs(y - in[i - d]));
  }
  CHECK(maxErr < 1e-4);
}
TEST(Pitch_clampsRange) {
  ei::PitchShifter p; p.prepare(kSr); p.setSemitones(1000.0f);
  for (int i = 0; i < 48000; ++i) CHECK(std::isfinite(p.process(float(i % 100) * 0.01f)));
}
