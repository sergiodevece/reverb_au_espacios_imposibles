// Follows the source and drives the shape of the reverb tail:
//
//   source starts  -> ATTACK : the gain of what enters the tank ramps 0 -> 1
//   source plays   -> the tank decays at the plain release rate
//   source stops   -> SUSTAIN: hold = 1 for `sustain` seconds (the tank is held, or bent by the Bender)
//                  -> RELEASE: the decay rate is scaled along the release time to draw the chosen curve
//
// Holding only after the source stops keeps the level from piling up while it plays.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

class TailEnvelope {
public:
  static constexpr float kThreshold = 0.00316228f;   // -50 dBFS
  static constexpr float kRiseSeconds = 0.005f, kFallSeconds = 0.05f;
  static constexpr float kMinScale = 0.05f, kMaxScale = 60.0f;

  void prepare(double sampleRate) {
    sr_ = sampleRate;
    riseStep_ = float(1.0 / (kRiseSeconds * sampleRate));
    fallStep_ = float(1.0 / (kFallSeconds * sampleRate));
    scaleCoef_ = float(std::exp(-1.0 / (0.01 * sampleRate)));
    reset();
  }
  void reset() {
    remaining_ = 0; hold_ = 0; wasActive_ = false;
    attackPos_ = 1e12; releasePos_ = 1e12;            // nothing has happened yet: no ramp, release long over
    inputGain_ = 1; scale_ = 1;
  }
  void setSustain(float seconds) { sustain_ = finite(seconds, 0, 0, 60); }
  void setAttack(float seconds) { attack_ = finite(seconds, 0, 0, 10); }
  void setRelease(float seconds, bool infinite = false) { release_ = finite(seconds, 4, 0.05f, 600); infinite_ = infinite; }
  // -1: falls away at once, then lingers.  0: plain exponential.  +1: holds on, then collapses.
  void setCurve(float curve) { curve_ = finite(curve, 0, -1, 1); power_ = std::pow(4.0f, curve_); }

  // activityLevelLin: a fast peak envelope of the source. Returns hold (0..1).
  float process(float activityLevelLin) {
    const bool active = activityLevelLin > kThreshold;
    if (active && !wasActive_) attackPos_ = 0;                                   // onset
    wasActive_ = active;

    // Attack.
    const double attackSamples = double(attack_) * sr_;
    inputGain_ = attackSamples < 1.0 ? 1.0f : float(std::min(1.0, attackPos_ / attackSamples));
    if (active) attackPos_ += 1.0;

    // Sustain.
    bool holding = false;
    if (active) { remaining_ = double(sustain_) * sr_; releasePos_ = -1; }       // every hit re-arms it
    else if (sustain_ > 0.0f && remaining_ > 0) { remaining_ = std::min(remaining_ - 1.0, double(sustain_) * sr_); holding = true; }
    else { remaining_ = 0; releasePos_ = releasePos_ < 0 ? 0.0 : releasePos_ + 1.0; }
    hold_ = holding ? std::min(1.0f, hold_ + riseStep_) : std::max(0.0f, hold_ - fallStep_);

    // Release curve: level(u) = -60 dB * u^power, so the decay rate is scaled by power * u^(power-1).
    float target = 1.0f;
    if (!active && !holding && !infinite_ && power_ != 1.0f) {
      const double u = releasePos_ / (double(release_) * sr_);
      target = u >= 1.0 ? std::max(1.0f, power_)
                        : std::clamp(power_ * float(std::pow(std::max(u, 1e-6), double(power_) - 1.0)), kMinScale, kMaxScale);
    }
    scale_ = target + scaleCoef_ * (scale_ - target);
    if (std::fabs(scale_ - target) < 1e-5f) scale_ = target;
    return hold_;
  }

  float inputGain() const { return inputGain_; }        // multiply what enters the reverb by this
  float releaseScale() const { return scale_; }         // multiply the release decay rate by this

private:
  static float finite(float v, float fallback, float lo, float hi) { return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback; }

  double sr_ = 48000, remaining_ = 0, attackPos_ = 1e12, releasePos_ = 1e12;
  float sustain_ = 0, attack_ = 0, release_ = 4, curve_ = 0, power_ = 1;
  float hold_ = 0, inputGain_ = 1, scale_ = 1, riseStep_ = 0, fallStep_ = 0, scaleCoef_ = 0;
  bool wasActive_ = false, infinite_ = false;
};

}  // namespace ei
