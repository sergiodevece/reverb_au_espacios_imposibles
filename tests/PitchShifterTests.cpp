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
// A granular shifter splices grains, and at two octaves it does so every 20 ms: a pure tone comes out as a
// cluster of lines around the target pitch rather than one clean line, and zero crossings over-read it.
// So measure where the energy sits: total energy within +-60 Hz of a frequency, over a second of output.
static double energyAround(float semis, double inFreq, double centre) {
  ei::PitchShifter p; p.prepare(kSr); p.setSemitones(semis);
  const int warm = 24000, n = 48000;
  std::vector<float> y(n);
  for (int i = 0; i < warm + n; ++i) { float v = p.process(0.5f * float(std::sin(2 * M_PI * inFreq * i / kSr))); if (i >= warm) y[size_t(i - warm)] = v; }
  double total = 0;
  for (int k = -60; k <= 60; ++k) {                       // 1 Hz steps: one second of signal resolves exactly that
    double re = 0, im = 0; const double w = 2 * M_PI * (centre + k) / kSr;
    for (int i = 0; i < n; ++i) { re += y[size_t(i)] * std::cos(w * i); im += y[size_t(i)] * std::sin(w * i); }
    total += (re * re + im * im) / (double(n) * n);
  }
  return total;
}
TEST(Pitch_twoOctavesUp) {
  const double target = energyAround(24, 220, 880);
  CHECK(target > 0.02);                                    // most of the tone (a clean 0.5 sine carries 0.0625)
  for (double other : {220.0, 440.0, 1760.0}) CHECK(target > 4.0 * energyAround(24, 220, other));
  double f; float o, i; runSine(24, 220, f, o, i);
  CHECK_NEAR(o, i, 3.0);
}
TEST(Pitch_twoOctavesDown) {
  const double target = energyAround(-24, 880, 220);
  CHECK(target > 0.02);
  for (double other : {880.0, 440.0, 110.0}) CHECK(target > 4.0 * energyAround(-24, 880, other));
  double f; float o, i; runSine(-24, 880, f, o, i);
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
