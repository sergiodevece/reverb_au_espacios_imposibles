// Peak envelope follower with separate attack and release times.
#pragma once
#include <cmath>

namespace ei {

class EnvelopeFollower {
public:
  void prepare(double sampleRate) { sr_ = sampleRate; setTimes(attMs_, relMs_); env_ = 0; }
  void reset() { env_ = 0; }
  void setTimes(float attackMs, float releaseMs) {
    attMs_ = attackMs; relMs_ = releaseMs;
    att_ = coef(attackMs); rel_ = coef(releaseMs);
  }
  float process(float x) {
    float a = std::fabs(x);
    float c = a > env_ ? att_ : rel_;
    env_ = a + c * (env_ - a);
    if (env_ < 1e-12f) env_ = 0.0f;
    return env_;
  }
  float value() const { return env_; }

private:
  float coef(float ms) const { return ms <= 0.0f ? 0.0f : float(std::exp(-1.0 / (0.001 * ms * sr_))); }
  double sr_ = 48000;
  float attMs_ = 1, relMs_ = 100, att_ = 0, rel_ = 0, env_ = 0;
};

}  // namespace ei
