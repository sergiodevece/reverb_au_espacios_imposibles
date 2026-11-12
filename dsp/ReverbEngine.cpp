#include "ReverbEngine.h"
#include <cmath>

namespace ei {
namespace {
inline float finiteOr(float v, float fallback) { return std::isfinite(v) ? v : fallback; }

// Transparent below |x| = 2, eases into a ceiling of 4. Only a runaway tail ever reaches it.
inline float wetLimiter(float x) {
  float a = std::fabs(x);
  if (a <= 2.0f) return x;
  float y = 2.0f + 2.0f * std::tanh((a - 2.0f) * 0.5f);
  return x < 0 ? -y : y;
}
inline void approach(float& v, float target, float coef) {
  v = target + coef * (v - target);
  if (std::fabs(v - target) < 1e-6f) v = target;
}
}  // namespace

void ReverbEngine::prepare(double sampleRate, int /*maxBlockSize*/) {
  sr_ = sampleRate;
  const int preMax = int(kMaxPreDelayMs * 0.001 * sampleRate) + 8;
  const int dryMax = int(kMaxWindowMs * 0.001 * sampleRate) + 8;
  for (auto& d : preDelay_) d.prepare(preMax);
  for (auto& d : dryDelay_) d.prepare(dryMax);
  fdn_.prepare(sampleRate);
  tail_.prepare(sampleRate);
  lfo_.prepare(sampleRate);
  reverser_.prepare(sampleRate, kMaxWindowMs * 0.001f);
  eq_.prepare(sampleRate);
  gate_.prepare(sampleRate);
  inputFollower_.prepare(sampleRate); inputFollower_.setTimes(1.0f, 50.0f);
  activityFollower_.prepare(sampleRate); activityFollower_.setTimes(0.1f, 10.0f);
  gateDryFollower_.prepare(sampleRate); gateDryFollower_.setTimes(0.1f, 10.0f);
  wetFollower_.prepare(sampleRate); wetFollower_.setTimes(0.1f, 10.0f);
  preDelayCoef_ = float(std::exp(-1.0 / (0.05 * sampleRate)));
  fadeCoef_ = float(std::exp(-1.0 / (0.005 * sampleRate)));      // ~20 ms to settle
  flushLength_ = std::max(16, int(0.005 * sampleRate));
  lfoFadeCoef_ = float(std::exp(-1.0 / (0.05 * sampleRate)));    // slow: releasing a Size offset too fast is a pitch swoop
  applyParams();
  reset();
}

void ReverbEngine::reset() {
  clearState();
  flushPending_.store(false, std::memory_order_release);
  flush_ = Flush::Idle; flushPos_ = 0;
  snap_ = true; meters_ = Meters{};
}

// Forgets all audio. Cheap enough for the audio thread: the delay lines reset in constant time.
void ReverbEngine::clearState() {
  for (auto& d : preDelay_) d.reset();
  for (auto& d : dryDelay_) d.reset();
  applyParams();
  fdn_.reset(); tail_.reset(); lfo_.reset(); reverser_.reset(); eq_.reset(); gate_.reset();
  inputFollower_.reset(); activityFollower_.reset(); gateDryFollower_.reset(); wetFollower_.reset();
  tick_ = 0;
}

// Jumps every smoothed value to its target (nothing is sounding, so nothing can click).
void ReverbEngine::snapSmoothers() {
  preDelay_s_ = preDelayTarget_; mixDry_ = mixDryTarget_; mixWet_ = mixWetTarget_; outGain_ = outGainTarget_;
  revMix_ = params_.reverseOn ? 1.0f : 0.0f; gateMix_ = params_.gateOn ? 1.0f : 0.0f; lfoMix_ = params_.lfoOn ? 1.0f : 0.0f;
  controlTick(0.0f); pan_ = panTarget_;
}

void ReverbEngine::setTempo(double bpm) {
  bpm_ = (std::isfinite(bpm) && bpm > 0.0) ? bpm : 120.0;
  lfo_.setTempo(bpm_);
  applyParams();                                   // synced window lengths depend on tempo
}

void ReverbEngine::setParams(const EngineParams& p) { params_ = p; applyParams(); }

void ReverbEngine::applyParams() {
  const EngineParams& p = params_;
  fdn_.setSize(p.size); fdn_.setDiffusion(p.diffusion); fdn_.setDensity(p.density); fdn_.setWidth(p.width); fdn_.setFreeze(p.freeze);
  tail_.setAttack(p.attackSec); tail_.setPoints(p.tail);
  fdn_.setDecayRate(tail_.decayRate());

  preDelayTarget_ = std::clamp(finiteOr(p.preDelayMs, 0.0f), 0.0f, kMaxPreDelayMs) * 0.001f * float(sr_);

  for (int i = 0; i < 6; ++i) eq_.setBand(i, p.eq[i]);
  eq_.setBand6HighCut(p.eqBand6HighCut);

  lfo_.setMode(p.lfoMode); lfo_.setShape(p.lfoShape); lfo_.setRateHz(p.lfoRateHz);
  lfo_.setSync(p.lfoSync, Lfo::divisionToBeats(p.lfoDivision));
  lfo_.setDepth(p.lfoDepth); lfo_.setPhase(p.lfoPhaseDeg); lfo_.setTempo(bpm_);

  double windowMs = p.reverseSync ? double(Lfo::divisionToBeats(p.reverseDivision)) * 60000.0 / bpm_
                                  : double(finiteOr(p.reverseWindowMs, 500.0f));
  windowMs = std::clamp(windowMs, double(kMinWindowMs), double(kMaxWindowMs));
  reverser_.setWindowSamples(int(std::lround(windowMs * 0.001 * sr_)));

  gate_.setParams(p.gateThresholdDb, p.gateAttackMs, p.gateHoldMs, p.gateReleaseMs, p.gateRangeDb);

  const float mix = std::clamp(finiteOr(p.mix, 0.3f), 0.0f, 1.0f);
  mixDryTarget_ = mix <= 0.0f ? 1.0f : std::cos(mix * 1.57079632679f);
  mixWetTarget_ = mix <= 0.0f ? 0.0f : std::sin(mix * 1.57079632679f);
  if (mix >= 1.0f) mixDryTarget_ = 0.0f;
  outGainTarget_ = std::pow(10.0f, std::clamp(finiteOr(p.outputDb, 0.0f), -24.0f, 12.0f) / 20.0f);
}

// LFO routing, every kControlInterval samples.
void ReverbEngine::controlTick(float lfo) {
  const EngineParams& p = params_;
  using T = EngineParams::LfoTarget;
  fdn_.setSizeMod(p.lfoTarget == T::Size ? lfo * kLfoSizeRange : 0.0f);
  fdn_.setPitch(p.pitch + (p.lfoTarget == T::Pitch ? lfo * kLfoPitchRange : 0.0f), p.pitchMix);
  fdn_.setDamping(std::clamp(finiteOr(p.damping, 0.0f) + (p.lfoTarget == T::Damping ? lfo * kLfoDampingRange : 0.0f), -1.0f, 1.0f));
  panTarget_ = p.lfoTarget == T::Pan ? lfo * kLfoPanRange : 0.0f;
}

void ReverbEngine::process(float* left, float* right, int numSamples) {
  const EngineParams& p = params_;
  const float revTarget = p.reverseOn ? 1.0f : 0.0f;
  const float gateTarget = p.gateOn ? 1.0f : 0.0f;
  const float lfoTarget = p.lfoOn ? 1.0f : 0.0f;
  if (snap_) { snapSmoothers(); snap_ = false; }
  if (flushPending_.exchange(false, std::memory_order_acq_rel) && flush_ != Flush::FadingOut) {
    flushPos_ = flush_ == Flush::FadingIn ? flushLength_ - flushPos_ : 0;     // start from the current gain
    flush_ = Flush::FadingOut;
  }
  Meters m; m.gateGain = 1.0f;

  for (int n = 0; n < numSamples; ++n) {
    const float inL = finiteOr(left[n], 0.0f), inR = finiteOr(right[n], 0.0f);

    // Analysis of the source.
    const float inPeak = std::max(std::fabs(inL), std::fabs(inR));
    const float level = inputFollower_.process(inPeak);
    tail_.process(activityFollower_.process(inPeak));       // fast detector: the curve starts right as the source stops
    const float hold = tail_.hold();
    fdn_.setHold(hold);
    fdn_.setDecayRate(tail_.decayRate());
    const float attackGain = tail_.inputGain();
    approach(lfoMix_, lfoTarget, lfoFadeCoef_);
    const float lfo = lfo_.process(level * 4.0f) * lfoMix_;  // the auto LFO is fully ducked from -12 dBFS up
    if (tick_ == 0) controlTick(lfo);
    tick_ = (tick_ + 1) % kControlInterval;

    // Pre-delay.
    approach(preDelay_s_, preDelayTarget_, preDelayCoef_);
    preDelay_[0].write(inL * attackGain); preDelay_[1].write(inR * attackGain);
    float pl, pr;
    if (preDelay_s_ == preDelayTarget_) {
      const int d = int(preDelayTarget_ + 0.5f);
      pl = preDelay_[0].readInt(d); pr = preDelay_[1].readInt(d);
    } else {
      pl = preDelay_[0].read(preDelay_s_); pr = preDelay_[1].read(preDelay_s_);
    }

    // Reverb.
    float wl, wr;
    fdn_.process(pl, pr, wl, wr);

    // Reverse, with the dry path delayed by the same latency.
    approach(revMix_, revTarget, fadeCoef_);
    dryDelay_[0].write(inL); dryDelay_[1].write(inR);
    float dryL = inL, dryR = inR;
    float rl = wl, rr = wr;
    reverser_.process(rl, rr);                      // always running, so switching it on never replays stale audio
    if (revMix_ > 0.0f) {
      const int latency = reverser_.activeWindowSamples();   // stays aligned with the grains actually playing
      wl += revMix_ * (rl - wl); wr += revMix_ * (rr - wr);
      dryL += revMix_ * (dryDelay_[0].readInt(latency) - inL);
      dryR += revMix_ * (dryDelay_[1].readInt(latency) - inR);
    }

    eq_.process(wl, wr);

    // Gate: keyed from the (latency-aligned) source for classic gated reverb, or from the tail itself.
    const float dryKey = gateDryFollower_.process(std::max(std::fabs(dryL), std::fabs(dryR)));
    const float wetKey = wetFollower_.process(std::max(std::fabs(wl), std::fabs(wr)));
    approach(gateMix_, gateTarget, fadeCoef_);
    float gateGain = 1.0f;
    if (gateMix_ > 0.0f || gateTarget > 0.0f) {
      const float g = gate_.process(p.gateSource == EngineParams::GateSource::Dry ? dryKey : wetKey);
      gateGain = 1.0f + gateMix_ * (g - 1.0f);
      wl *= gateGain; wr *= gateGain;
    }

    // Pan (LFO target only).
    approach(pan_, panTarget_, fadeCoef_);
    if (pan_ > 0.0f) wl *= 1.0f - pan_; else if (pan_ < 0.0f) wr *= 1.0f + pan_;

    wl = wetLimiter(wl); wr = wetLimiter(wr);

    // Flush: raised-cosine fade out, forget everything, fade back in.
    if (flush_ != Flush::Idle) {
      const float x = float(flushPos_) / float(flushLength_);
      const float g = 0.5f + 0.5f * std::cos(3.14159265f * (flush_ == Flush::FadingOut ? x : 1.0f - x));
      wl *= g; wr *= g;
      if (++flushPos_ >= flushLength_) {
        flushPos_ = 0;
        if (flush_ == Flush::FadingOut) { clearState(); snapSmoothers(); flush_ = Flush::FadingIn; }
        else flush_ = Flush::Idle;
      }
    }

    approach(mixDry_, mixDryTarget_, fadeCoef_); approach(mixWet_, mixWetTarget_, fadeCoef_);
    approach(outGain_, outGainTarget_, fadeCoef_);
    left[n] = std::clamp((dryL * mixDry_ + wl * mixWet_) * outGain_, -8.0f, 8.0f);
    right[n] = std::clamp((dryR * mixDry_ + wr * mixWet_) * outGain_, -8.0f, 8.0f);

    m.inputLevel = std::max(m.inputLevel, inPeak);
    m.wetLevel = std::max(m.wetLevel, std::max(std::fabs(wl), std::fabs(wr)));
    m.gateGain = std::min(m.gateGain, gateGain);
    m.lfoValue = lfo; m.hold = hold;
  }
  if (numSamples > 0) meters_ = m;
}

}  // namespace ei
