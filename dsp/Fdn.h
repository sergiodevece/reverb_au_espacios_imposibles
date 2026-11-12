// 16-line feedback delay network: the reverb core.
#pragma once
#include "DelayLine.h"
#include "PitchShifter.h"
#include <array>

namespace ei {

class Fdn {
public:
  static constexpr int kLines = 16;
  static constexpr int kPitchLines = 4;          // lines 0, 4, 8, 12 carry a pitch shifter
  static constexpr float kMaxLineSeconds = 3.0f;

  void prepare(double sampleRate);               // allocates
  void reset();

  void setSize(float meters);                    // 1..1000, smoothed (~150 ms)
  void setSizeMod(float mod);                    // -0.5..+0.5, scales every delay by (1 + mod), smoothed (~3 ms)
  void setDiffusion(float amount01);
  void setDamping(float bipolar);                // -1: highs die first (darkens). +1: lows die first (brightens)
  void setPitch(float semitones, float mix01);
  void setRelease(float rt60Seconds, bool infinite);
  void setHold(float hold01);                    // 1 = sustaining: feedback follows the hold rate, damping is off
  void setHoldRate(float dbPerSecond);           // level change while held: 0 = frozen, + sinks, - grows (Bender)
  void setReleaseScale(float scale);             // multiplies the release decay rate (release curve)
  void setFreeze(bool on);
  void setWidth(float width);                    // 0 mono .. 2 extra wide

  void process(float inL, float inR, float& outL, float& outR);

private:
  void controlTick();

  struct Allpass {
    DelayLine line; int delay = 1;
    float process(float x, float g) {
      float d = line.readInt(delay - 1);
      float y = d - g * x;
      line.write(x + g * y);
      return y;
    }
  };

  static constexpr int kControlInterval = 16;
  double sr_ = 48000;
  std::array<DelayLine, kLines> lines_;
  std::array<PitchShifter, kPitchLines> shifters_;
  std::array<std::array<Allpass, 4>, 2> diffusers_;
  std::array<float, kLines> base_{}, baseInc_{}, gain_{}, holdGain_{}, lp_{}, shelf_{};
  int tick_ = 0;
  bool settled_ = false;

  float sizeTarget_ = 30, size_ = 30, sizeCoef_ = 0;
  float modTarget_ = 0, mod_ = 0, modCoef_ = 0;
  float diffusion_ = 0.7f, damping_ = 0, width_ = 1;
  float lpCoef_ = 0, lpBlend_ = 0, shelfCoef_ = 0, shelfGain_ = 0;
  float pitchMixTarget_ = 0, pitchMix_ = 0;
  float rt60_ = 4; bool infinite_ = false;
  float hold_ = 0, holdRate_ = 0, releaseScale_ = 1;
  float freezeTarget_ = 0, freeze_ = 0, freezeCoef_ = 0;
  float maxDelay_ = 0;
};

}  // namespace ei
