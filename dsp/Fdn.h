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
  // Rooms up to this size keep the original, fixed input diffusion; above it the diffusion grows with the room.
  static constexpr float kReferenceSize = 30.0f;
  static constexpr float kMaxDiffusionScale = 20.0f;

  void prepare(double sampleRate);               // allocates
  void reset();

  void setSize(float meters);                    // 1..1000, smoothed (~150 ms)
  void setSizeMod(float mod);                    // -0.5..+0.5, scales every delay by (1 + mod), smoothed (~3 ms)
  void setDiffusion(float amount01);
  // 0 = separate echoes (the lines stop mixing), 0.5 = the plain network, 1 = extra-dense (diffusers inside the loop).
  void setDensity(float amount01);
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
  void updateMixMatrix();

  struct Allpass {
    DelayLine line; float delay = 1, inc = 0;      // delay in samples; `inc` glides it when Size moves
    float process(float x, float g) {
      delay += inc;
      const int whole = int(delay);
      float d = float(whole) == delay ? line.readInt(whole - 1) : line.read(delay - 1.0f);
      float y = d - g * x;
      line.write(x + g * y);
      return y;
    }
  };
  static constexpr int kDiffusers = 8;             // 4 always on + 4 that only large rooms use

  static constexpr int kControlInterval = 16;
  double sr_ = 48000;
  std::array<DelayLine, kLines> lines_;
  std::array<PitchShifter, kPitchLines> shifters_;
  std::array<std::array<Allpass, kDiffusers>, 2> diffusers_;
  float diffuserBase_[2][kDiffusers] = {};        // reference lengths in samples
  float extraDiffusion_ = 0;                      // 0 up to the reference room size, 1 well above it
  std::array<float, kLines> base_{}, baseInc_{}, gain_{}, holdGain_{}, lp_{}, shelf_{};
  std::array<DelayLine, kLines> apLines_;          // in-loop diffusers, one per line
  std::array<float, kLines> apBase_{}, apInc_{};
  float mix4_[4][4] = {};                          // 4x4 block of the mixing matrix; the 16x16 is its Kronecker square
  bool plainMix_ = true;                           // true = exact Hadamard
  float densityTarget_ = 0.5f, mixAngle_ = 3.14159265f, apGain_ = 0, apGainCoef_ = 0;
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
