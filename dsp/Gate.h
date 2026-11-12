// Noise gate driven by an external detector level. Returns the gain to apply.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

class Gate {
public:
  void prepare(double sampleRate) { sr_ = sampleRate; setParams(thrDb_, attMs_, holdMs_, relMs_, rangeDb_); reset(); }
  void reset() { open_ = false; holdLeft_ = 0; gain_ = range_; }

  void setParams(float thresholdDb, float attackMs, float holdMs, float releaseMs, float rangeDb) {
    thrDb_ = fin(thresholdDb, -30); attMs_ = fin(attackMs, 1); holdMs_ = fin(holdMs, 100); relMs_ = fin(releaseMs, 100); rangeDb_ = fin(rangeDb, -80);
    openThr_ = dbToLin(std::clamp(thrDb_, -100.0f, 0.0f));
    closeThr_ = openThr_ * 0.70794578f;                                 // 3 dB of hysteresis
    att_ = coef(std::clamp(attMs_, 0.05f, 1000.0f));
    rel_ = coef(std::clamp(relMs_, 1.0f, 10000.0f));
    holdSamples_ = int(std::clamp(holdMs_, 0.0f, 10000.0f) * 0.001 * sr_);
    range_ = dbToLin(std::clamp(rangeDb_, -100.0f, 0.0f));
  }

  float process(float detectorLevelLin) {
    if (detectorLevelLin > openThr_) { open_ = true; holdLeft_ = holdSamples_; }
    else if (open_ && detectorLevelLin < closeThr_) { if (holdLeft_ > 0) --holdLeft_; else open_ = false; }

    if (open_) {
      gain_ = 1.0f + att_ * (gain_ - 1.0f);                             // linear rise
    } else if (gain_ > range_ * 1.0001f) {
      float ratio = std::max(gain_, 1e-9f) / range_;                    // exponential fall (linear in dB)
      gain_ = range_ * std::exp(std::log(ratio) * rel_);
    } else {
      gain_ = range_;
    }
    return gain_;
  }
  bool isOpen() const { return open_; }
  float gain() const { return gain_; }

private:
  static float fin(float v, float fallback) { return std::isfinite(v) ? v : fallback; }
  static float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
  float coef(float ms) const { return float(std::exp(-1.0 / (0.001 * ms * sr_))); }

  double sr_ = 48000;
  float thrDb_ = -30, attMs_ = 1, holdMs_ = 100, relMs_ = 100, rangeDb_ = -80;
  float openThr_ = 0.0316f, closeThr_ = 0.0224f, att_ = 0, rel_ = 0, range_ = 0.0001f, gain_ = 0.0001f;
  int holdSamples_ = 0, holdLeft_ = 0;
  bool open_ = false;
};

}  // namespace ei
