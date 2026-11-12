// The whole signal chain:
//   in -> pre-delay -> FDN -> reverser -> EQ -> gate -> mix -> out
// Plain C++; the plugin wrapper only talks to this class.
#pragma once
#include "DelayLine.h"
#include "EnvelopeFollower.h"
#include "Eq6.h"
#include "Fdn.h"
#include "Gate.h"
#include "Lfo.h"
#include "Reverser.h"
#include "TailEnvelope.h"
#include <atomic>

namespace ei {

struct EngineParams {
  // Space
  float size = 30, diffusion = 0.7f, damping = -0.3f, pitch = 0, pitchMix = 0, width = 1;
  bool freeze = false;
  // Tail envelope
  float preDelayMs = 20, attackSec = 0, sustainSec = 0, releaseSec = 4;
  float sustainBendDb = 0;      // level change across the sustain: -24 .. +12 dB
  float releaseCurve = 0;       // -1 falls away then lingers .. +1 holds on then collapses
  bool releaseInfinite = false;
  // EQ
  EqBand eq[6] = {{false, 40, 0, 0.707f}, {true, 150, 0, 0.707f}, {true, 400, 0, 1.0f},
                  {true, 1500, 0, 1.0f}, {true, 5000, 0, 1.0f}, {true, 10000, 0, 0.707f}};
  bool eqBand6HighCut = false;
  // LFO
  bool lfoOn = true;
  Lfo::Mode lfoMode = Lfo::Mode::Auto;
  Lfo::Shape lfoShape = Lfo::Shape::Sine;
  float lfoRateHz = 0.5f; bool lfoSync = false; int lfoDivision = 3;
  float lfoDepth = 0.2f, lfoPhaseDeg = 0;
  enum class LfoTarget { Size, Pitch, Damping, Pan } lfoTarget = LfoTarget::Size;
  // Reverse
  bool reverseOn = false; float reverseWindowMs = 500; bool reverseSync = false; int reverseDivision = 5;
  // Gate
  bool gateOn = false;
  float gateThresholdDb = -30, gateAttackMs = 1, gateHoldMs = 100, gateReleaseMs = 100, gateRangeDb = -80;
  enum class GateSource { Dry, Wet } gateSource = GateSource::Dry;
  // Output
  float mix = 0.3f, outputDb = 0;
};

struct Meters { float inputLevel = 0, wetLevel = 0, gateGain = 1, lfoValue = 0, hold = 0; };

class ReverbEngine {
public:
  static constexpr float kMaxPreDelayMs = 500.0f;
  static constexpr float kMinWindowMs = 50.0f, kMaxWindowMs = 2000.0f;
  // LFO reach at full depth.
  static constexpr float kLfoSizeRange = 0.5f, kLfoPitchRange = 2.0f, kLfoDampingRange = 0.5f, kLfoPanRange = 1.0f;

  void prepare(double sampleRate, int maxBlockSize);   // allocates
  void reset();                                         // immediate, for use while no audio is running
  // Asks for the tail to be dropped. Safe from any thread and click-free: the next process() call fades
  // the wet signal out over a few milliseconds, forgets everything, then fades back in.
  void requestFlush() { flushPending_.store(true, std::memory_order_release); }
  void setParams(const EngineParams& p);               // once per block
  void setTempo(double bpm);                           // <= 0 falls back to 120
  void process(float* left, float* right, int numSamples);   // in place

  int latencySamples() const { return params_.reverseOn ? reverser_.latencySamples() : 0; }
  Meters meters() const { return meters_; }             // peaks over the last block
  const Eq6& eq() const { return eq_; }

private:
  void applyParams();
  void clearState();
  void snapSmoothers();
  void controlTick(float lfoValue);

  static constexpr int kControlInterval = 32;
  double sr_ = 48000, bpm_ = 120;
  EngineParams params_;
  bool snap_ = true;
  int tick_ = 0;

  DelayLine preDelay_[2], dryDelay_[2];
  Fdn fdn_;
  TailEnvelope tail_;
  Lfo lfo_;
  Reverser reverser_;
  Eq6 eq_;
  Gate gate_;
  EnvelopeFollower inputFollower_, activityFollower_, gateDryFollower_, wetFollower_;

  float preDelayTarget_ = 0, preDelay_s_ = 0, preDelayCoef_ = 0;
  float fadeCoef_ = 0;
  float mixDry_ = 1, mixWet_ = 0, mixDryTarget_ = 1, mixWetTarget_ = 0;
  float outGain_ = 1, outGainTarget_ = 1;
  float revMix_ = 0, gateMix_ = 0, pan_ = 0, panTarget_ = 0;
  float lfoMix_ = 1, lfoFadeCoef_ = 0;
  std::atomic<bool> flushPending_{false};
  enum class Flush { Idle, FadingOut, FadingIn } flush_ = Flush::Idle;
  int flushPos_ = 0, flushLength_ = 240;
  Meters meters_;
};

}  // namespace ei
