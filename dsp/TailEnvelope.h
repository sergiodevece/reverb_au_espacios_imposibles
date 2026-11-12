// Follows the source and drives the shape of the reverb tail.
//
//   source starts -> ATTACK: the gain of what enters the tank ramps 0 -> 1
//   source plays  -> the tank decays at the average slope of the drawn curve
//   source stops  -> the tank follows the drawn curve: five (time, level) points, joined by straight lines
//                    in dB, starting from (0 s, 0 dB). Each segment becomes a decay rate; a rising segment
//                    is a negative rate (the tail grows).
//
// The curve only starts once the source stops: holding or growing while it still plays would pile the
// level up without limit.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

struct TailPoint { float time; float levelDb; };       // seconds after the source stops, dB relative to full level

class TailEnvelope {
public:
  static constexpr int kPoints = 5;
  static constexpr float kThreshold = 0.00316228f;     // -50 dBFS
  static constexpr float kMinLevelDb = -60.0f, kMaxLevelDb = 12.0f, kMaxTime = 60.0f;
  static constexpr float kMinRate = -60.0f, kMaxRate = 600.0f;      // dB per second; negative = growing
  static constexpr float kRiseSeconds = 0.005f, kFallSeconds = 0.05f;

  // A plain exponential decay: 60 dB in `rt60` seconds.
  static void exponential(TailPoint* pts, float rt60) {
    for (int i = 0; i < kPoints; ++i) pts[i] = {rt60 * float(i + 1) / float(kPoints), kMinLevelDb * float(i + 1) / float(kPoints)};
  }

  TailEnvelope() { exponential(pts_, 4.0f); refresh(); }

  void prepare(double sampleRate) {
    sr_ = sampleRate;
    riseStep_ = float(1.0 / (kRiseSeconds * sampleRate));
    fallStep_ = float(1.0 / (kFallSeconds * sampleRate));
    rateCoef_ = float(std::exp(-1.0 / (0.01 * sampleRate)));
    reset();
  }
  void reset() {
    wasActive_ = false; hold_ = 0; inputGain_ = 1;
    attackPos_ = 1e12; tailPos_ = 1e12;               // nothing has happened yet: no ramp, curve long over
    rate_ = endRate_;
  }
  void setAttack(float seconds) { attack_ = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 10.0f) : 0.0f; }

  // Takes kPoints points. Anything out of range is clamped and times are forced into order,
  // so no automation move can produce an invalid curve.
  void setPoints(const TailPoint* pts) {
    float previous = 0.0f;
    for (int i = 0; i < kPoints; ++i) {
      float t = std::isfinite(pts[i].time) ? std::clamp(pts[i].time, 0.0f, kMaxTime) : previous;
      float l = std::isfinite(pts[i].levelDb) ? std::clamp(pts[i].levelDb, kMinLevelDb, kMaxLevelDb) : 0.0f;
      t = std::max(t, previous);
      pts_[i] = {t, l};
      previous = t;
    }
    refresh();
  }

  // activityLevelLin: a fast peak envelope of the source.
  void process(float activityLevelLin) {
    const bool active = activityLevelLin > kThreshold;
    if (active && !wasActive_) attackPos_ = 0;                                   // onset
    wasActive_ = active;

    const double attackSamples = double(attack_) * sr_;
    inputGain_ = attackSamples < 1.0 ? 1.0f : float(std::min(1.0, attackPos_ / attackSamples));
    if (active) attackPos_ += 1.0;

    float target;
    if (active) { tailPos_ = 0; target = activeRate_; }                          // every hit restarts the curve
    else { target = rateAt(float(tailPos_ / sr_)); tailPos_ += 1.0; }
    rate_ = target + rateCoef_ * (rate_ - target);
    if (std::fabs(rate_ - target) < 1e-3f) rate_ = target;

    // While the curve is flat or rising the tank is held: the engine also lifts the damping then.
    const bool holding = !active && target <= 0.01f;
    hold_ = holding ? std::min(1.0f, hold_ + riseStep_) : std::max(0.0f, hold_ - fallStep_);
  }

  float inputGain() const { return inputGain_; }     // multiply what enters the reverb by this
  float decayRate() const { return rate_; }          // dB per second the tank should lose right now
  float hold() const { return hold_; }               // 1 while held or growing

  // Seconds until the curve has run its course, or a negative value if it never dies.
  float lengthSeconds() const { return endsSilent_ ? pts_[kPoints - 1].time : -1.0f; }

private:
  void refresh() {
    const TailPoint& last = pts_[kPoints - 1];
    endsSilent_ = last.levelDb <= kMinLevelDb + 1.0f;
    activeRate_ = std::clamp(-last.levelDb / std::max(last.time, 0.05f), 1.0f, kMaxRate);
    endRate_ = endsSilent_ ? std::max(activeRate_, 20.0f) : 0.0f;                // ends above silence: stays there for good
  }
  float rateAt(float t) const {
    float t0 = 0.0f, l0 = 0.0f;
    for (int i = 0; i < kPoints; ++i) {
      if (t < pts_[i].time) return std::clamp(-(pts_[i].levelDb - l0) / std::max(pts_[i].time - t0, 0.005f), kMinRate, kMaxRate);
      t0 = pts_[i].time; l0 = pts_[i].levelDb;
    }
    return endRate_;
  }

  double sr_ = 48000, attackPos_ = 1e12, tailPos_ = 1e12;
  TailPoint pts_[kPoints];
  float attack_ = 0, activeRate_ = 15, endRate_ = 20, rate_ = 20;
  float hold_ = 0, inputGain_ = 1, riseStep_ = 0, fallStep_ = 0, rateCoef_ = 0;
  bool wasActive_ = false, endsSilent_ = true;
};

}  // namespace ei
