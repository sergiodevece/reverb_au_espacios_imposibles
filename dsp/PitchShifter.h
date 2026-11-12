// Granular pitch shifter: two delay taps half a grain apart, each swept by a
// sawtooth and windowed with sin^2 so the pair always sums to unity.
#pragma once
#include "DelayLine.h"
#include <algorithm>
#include <cmath>

namespace ei {

class PitchShifter {
public:
  static constexpr float kGrainMs = 60.0f;

  void prepare(double sampleRate) {
    sr_ = sampleRate;
    grain_ = float(kGrainMs * 0.001 * sampleRate);
    line_.prepare(int(grain_) + 16);
    bypassCoef_ = float(std::exp(-1.0 / (0.02 * sampleRate)));
    setSemitones(semis_);
    reset();
  }
  void reset() { line_.reset(); phase_ = 0.0f; bypass_ = semis_ == 0.0f ? 1.0f : 0.0f; }

  void setSemitones(float st) {
    if (!std::isfinite(st)) st = 0.0f;
    semis_ = std::clamp(st, -12.0f, 12.0f);
    float ratio = std::pow(2.0f, semis_ / 12.0f);
    inc_ = (1.0f - ratio) / grain_;          // delay shrinks when pitching up
  }
  // Delay of the dry path when the shift is 0 (half a grain).
  int delaySamples() const { return int(grain_ * 0.5f); }

  float process(float x) {
    line_.write(x);
    float target = semis_ == 0.0f ? 1.0f : 0.0f;
    bypass_ = target + bypassCoef_ * (bypass_ - target);
    float clean = line_.readInt(delaySamples());
    if (bypass_ > 0.9999f) return clean;

    phase_ += inc_;
    phase_ -= std::floor(phase_);
    float p2 = phase_ + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    float s1 = std::sin(kPi * phase_), s2 = std::sin(kPi * p2);
    float wet = s1 * s1 * line_.read(1.0f + phase_ * (grain_ - 2.0f))
              + s2 * s2 * line_.read(1.0f + p2 * (grain_ - 2.0f));
    return wet + bypass_ * (clean - wet);
  }

private:
  static constexpr float kPi = 3.14159265358979f;
  DelayLine line_;
  double sr_ = 48000;
  float grain_ = 2880, semis_ = 0, inc_ = 0, phase_ = 0, bypass_ = 1, bypassCoef_ = 0;
};

}  // namespace ei
