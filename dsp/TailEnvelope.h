// Turns source activity into a "hold" signal for the reverb tank.
// While the source plays the tank decays normally; once it stops, hold goes to 1
// for `sustain` seconds (the tail stays at full level), then ramps back to 0.
// Holding only after the source stops keeps the level from piling up.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

class TailEnvelope {
public:
  static constexpr float kThreshold = 0.00316228f;   // -50 dBFS
  static constexpr float kRiseSeconds = 0.005f, kFallSeconds = 0.05f;

  void prepare(double sampleRate) {
    sr_ = sampleRate;
    riseStep_ = float(1.0 / (kRiseSeconds * sampleRate));
    fallStep_ = float(1.0 / (kFallSeconds * sampleRate));
    reset();
  }
  void reset() { remaining_ = 0; hold_ = 0; }
  void setSustain(float seconds) { sustain_ = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 60.0f) : 0.0f; }

  // activityLevelLin: a fast peak envelope of the source.
  float process(float activityLevelLin) {
    bool holding = false;
    if (sustain_ <= 0.0f) remaining_ = 0;
    else if (activityLevelLin > kThreshold) remaining_ = double(sustain_) * sr_;          // re-armed by every hit
    else if (remaining_ > 0) { remaining_ = std::min(remaining_ - 1.0, double(sustain_) * sr_); holding = true; }
    hold_ = holding ? std::min(1.0f, hold_ + riseStep_) : std::max(0.0f, hold_ - fallStep_);
    return hold_;
  }

private:
  double sr_ = 48000, remaining_ = 0;
  float sustain_ = 0, hold_ = 0, riseStep_ = 0, fallStep_ = 0;
};

}  // namespace ei
