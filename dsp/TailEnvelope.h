// Turns input activity into a "hold" signal: 1 while the source plays and for
// `sustain` seconds afterwards, then a 50 ms ramp down to 0.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

class TailEnvelope {
public:
  static constexpr float kThreshold = 0.00316228f;   // -50 dBFS
  static constexpr float kRampSeconds = 0.05f;

  void prepare(double sampleRate) { sr_ = sampleRate; rampStep_ = float(1.0 / (kRampSeconds * sampleRate)); reset(); }
  void reset() { remaining_ = 0; hold_ = 0; }
  void setSustain(float seconds) { sustain_ = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 60.0f) : 0.0f; }

  float process(float inputLevelLin) {
    if (sustain_ <= 0.0f) { remaining_ = 0; hold_ = std::max(0.0f, hold_ - rampStep_); return hold_; }
    if (inputLevelLin > kThreshold) remaining_ = double(sustain_) * sr_;
    else if (remaining_ > 0) remaining_ = std::min(remaining_ - 1.0, double(sustain_) * sr_);
    if (remaining_ > 0) hold_ = 1.0f;                               // instant on: the tail must not sag at the onset
    else hold_ = std::max(0.0f, hold_ - rampStep_);
    return hold_;
  }

private:
  double sr_ = 48000, remaining_ = 0;
  float sustain_ = 0, hold_ = 0, rampStep_ = 0;
};

}  // namespace ei
