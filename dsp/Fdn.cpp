#include "Fdn.h"
#include <cmath>

namespace ei {
namespace {
constexpr float kSpeedOfSound = 343.0f;
constexpr float kTwoPi = 6.28318530717959f;

// Relative line lengths: irregular spacing so the echoes never line up.
constexpr float kRatios[Fdn::kLines] = {0.353f, 0.389f, 0.431f, 0.467f, 0.509f, 0.547f, 0.593f, 0.631f,
                                        0.677f, 0.719f, 0.761f, 0.809f, 0.853f, 0.907f, 0.953f, 1.000f};
// Output tap signs: even lines feed the left channel, odd lines the right.
constexpr float kTapSign[Fdn::kLines] = {1, 1, -1, -1, 1, 1, -1, -1, -1, 1, 1, -1, 1, -1, -1, 1};
constexpr float kDiffuserMs[4] = {4.7f, 3.6f, 12.7f, 9.3f};
// Extra diffusers for large rooms: milliseconds per unit of (size / reference - 1), so they start from nothing.
constexpr float kExtraDiffuserMs[4] = {5.3f, 8.9f, 14.1f, 19.7f};
constexpr float kMaxExtraDiffuserMs = 300.0f;
// Where along each line the output is picked up, as shares of its length: four taps per line, 64 in all,
// each one a reflection of its own. A few sit almost at the start of their line, so the reverb answers at once
// whatever the room size (the pre-delay alone decides when it begins); the rest are spread irregularly, so even
// a room hundreds of metres wide returns a steady stream of reflections instead of a few distant echoes.
constexpr int kTaps = 4;
constexpr float kTapShare[Fdn::kLines][kTaps] = {
    {0.002f, 0.283f, 0.561f, 0.847f}, {0.003f, 0.317f, 0.609f, 0.883f}, {0.071f, 0.359f, 0.631f, 0.917f}, {0.103f, 0.397f, 0.673f, 0.953f},
    {0.037f, 0.241f, 0.523f, 0.787f}, {0.053f, 0.271f, 0.541f, 0.811f}, {0.139f, 0.433f, 0.709f, 0.971f}, {0.167f, 0.461f, 0.739f, 0.991f},
    {0.019f, 0.223f, 0.487f, 0.761f}, {0.029f, 0.199f, 0.503f, 0.773f}, {0.008f, 0.337f, 0.593f, 0.863f}, {0.011f, 0.373f, 0.647f, 0.907f},
    {0.089f, 0.307f, 0.577f, 0.829f}, {0.113f, 0.349f, 0.619f, 0.877f}, {0.151f, 0.409f, 0.683f, 0.937f}, {0.181f, 0.443f, 0.727f, 0.967f}};
constexpr float kTapPolarity[kTaps] = {1.0f, -1.0f, -1.0f, 1.0f};
// In-loop diffuser length as a share of its line. The share is carved out of the line, so the loop time is unchanged.
constexpr float kApShare[Fdn::kLines] = {0.31f, 0.23f, 0.37f, 0.27f, 0.21f, 0.35f, 0.25f, 0.39f,
                                         0.29f, 0.19f, 0.33f, 0.24f, 0.38f, 0.22f, 0.34f, 0.28f};
constexpr float kApMaxGain = 0.6f;
constexpr float kPi = 3.14159265358979f;
constexpr float kInputGain = 0.5f;
constexpr float kOutputGain = 0.175f;      // four taps per line: half the gain keeps the level where it was

// Linear below |x| = 1 (so a held tail is lossless), smoothly limited to +-2 above.
inline float softClip(float x) {
  float a = std::fabs(x);
  if (a <= 1.0f) return a < 1e-20f ? 0.0f : x;      // also flushes denormals
  float y = 1.0f + std::tanh(a - 1.0f);
  return x < 0 ? -y : y;
}

inline void hadamard16(float* v) {
  for (int h = 1; h < 16; h <<= 1)
    for (int i = 0; i < 16; i += h << 1)
      for (int j = i; j < i + h; ++j) {
        float a = v[j], b = v[j + h];
        v[j] = a + b; v[j + h] = a - b;
      }
  for (int i = 0; i < 16; ++i) v[i] *= 0.25f;
}

// y = (M4 (x) M4) v, with v indexed as 4*a + b.
inline void mix16(float* v, const float m[4][4]) {
  for (int a = 0; a < 16; a += 4) {
    const float x0 = v[a], x1 = v[a + 1], x2 = v[a + 2], x3 = v[a + 3];
    for (int r = 0; r < 4; ++r) v[a + r] = m[r][0] * x0 + m[r][1] * x1 + m[r][2] * x2 + m[r][3] * x3;
  }
  for (int b = 0; b < 4; ++b) {
    const float x0 = v[b], x1 = v[4 + b], x2 = v[8 + b], x3 = v[12 + b];
    for (int r = 0; r < 4; ++r) v[4 * r + b] = m[r][0] * x0 + m[r][1] * x1 + m[r][2] * x2 + m[r][3] * x3;
  }
}
}  // namespace

// The mixing matrix slides from the identity (angle 0: sixteen separate echo lines) to the Hadamard matrix
// (angle pi: full mixing) and stays orthogonal all the way, so no energy is gained or lost on the way.
// The 4x4 Hadamard is Q * diag(1,-1,-1,1) * Q^T with Q = R (x) R, R a rotation by pi/8; turning the middle
// 2x2 block from a 0 to a pi rotation gives every matrix in between.
void Fdn::updateMixMatrix() {
  plainMix_ = mixAngle_ >= kPi - 1e-4f;
  if (plainMix_) return;
  const float c = std::cos(kPi / 8.0f), s = std::sin(kPi / 8.0f);
  const float R[2][2] = {{c, -s}, {s, c}};
  float Q[4][4];
  for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) Q[i][j] = R[i >> 1][j >> 1] * R[i & 1][j & 1];
  const float ct = std::cos(mixAngle_), st = std::sin(mixAngle_);
  const float D[4][4] = {{1, 0, 0, 0}, {0, ct, -st, 0}, {0, st, ct, 0}, {0, 0, 0, 1}};
  float T[4][4];
  for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) { T[i][j] = 0; for (int k = 0; k < 4; ++k) T[i][j] += Q[i][k] * D[k][j]; }
  for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) { mix4_[i][j] = 0; for (int k = 0; k < 4; ++k) mix4_[i][j] += T[i][k] * Q[j][k]; }
}

void Fdn::prepare(double sampleRate) {
  sr_ = sampleRate;
  maxDelay_ = float(kMaxLineSeconds * sampleRate);
  for (auto& l : lines_) l.prepare(int(maxDelay_) + 8);
  for (auto& l : apLines_) l.prepare(int(maxDelay_ * 0.4f) + 8);
  apGainCoef_ = float(std::exp(-1.0 / (0.05 * sampleRate)));
  for (auto& s : shifters_) s.prepare(sampleRate);
  for (int ch = 0; ch < 2; ++ch)
    for (int k = 0; k < kDiffusers; ++k) {
      const float stereo = ch == 0 ? 1.0f : 1.07f;
      if (k < 4) {
        const int d = std::max(2, int(kDiffuserMs[k] * stereo * 0.001 * sampleRate));
        diffuserBase_[ch][k] = float(d);
        diffusers_[ch][k].line.prepare(d + 8);
      } else {
        diffuserBase_[ch][k] = float(kExtraDiffuserMs[k - 4] * stereo * 0.001 * sampleRate);
        diffusers_[ch][k].line.prepare(int(kMaxExtraDiffuserMs * 0.001 * sampleRate) + 8);
      }
    }
  sizeCoef_ = float(std::exp(-double(kControlInterval) / (0.15 * sampleRate)));
  modCoef_ = float(std::exp(-1.0 / (0.003 * sampleRate)));
  freezeCoef_ = float(std::exp(-1.0 / (0.005 * sampleRate)));
  shelfCoef_ = float(std::exp(-kTwoPi * 2000.0 / sampleRate));
  setDamping(damping_);
  reset();
}

void Fdn::reset() {
  for (auto& l : lines_) l.reset();
  for (auto& l : apLines_) l.reset();
  for (auto& s : shifters_) s.reset();
  for (auto& ch : diffusers_) for (auto& a : ch) a.line.reset();
  lp_.fill(0); shelf_.fill(0);
  mixAngle_ = kPi * std::min(1.0f, densityTarget_ * 2.0f); apGain_ = kApMaxGain * std::max(0.0f, densityTarget_ * 2.0f - 1.0f);
  updateMixMatrix();
  size_ = sizeTarget_; mod_ = modTarget_; pitchMix_ = pitchMixTarget_; freeze_ = freezeTarget_;
  tick_ = 0; settled_ = false;
  for (int i = 0; i < kLines; ++i) {
    base_[i] = std::clamp(size_ / kSpeedOfSound * kRatios[i] * float(sr_), 4.0f, maxDelay_);
    baseInc_[i] = 0;
    apBase_[i] = base_[i] * kApShare[i]; apInc_[i] = 0;
  }
  controlTick();
  for (auto& ch : diffusers_) for (auto& a : ch) { a.delay += a.inc * float(kControlInterval); a.inc = 0; }
  for (int i = 0; i < kLines; ++i) {             // start settled: land on the first targets at once
    base_[i] += baseInc_[i] * float(kControlInterval); apBase_[i] += apInc_[i] * float(kControlInterval);
    baseInc_[i] = 0; apInc_[i] = 0;
  }
}

void Fdn::setSize(float m) { if (std::isfinite(m)) sizeTarget_ = std::clamp(m, 1.0f, 1000.0f); }
void Fdn::setSizeMod(float mod) { if (std::isfinite(mod)) modTarget_ = std::clamp(mod, -0.5f, 0.5f); }
void Fdn::setDensity(float a) { if (std::isfinite(a)) densityTarget_ = std::clamp(a, 0.0f, 1.0f); }
void Fdn::setDiffusion(float a) { if (std::isfinite(a)) diffusion_ = std::clamp(a, 0.0f, 1.0f); }
void Fdn::setWidth(float w) { if (std::isfinite(w)) width_ = std::clamp(w, 0.0f, 2.0f); }
void Fdn::setHold(float h) { if (std::isfinite(h)) hold_ = std::clamp(h, 0.0f, 1.0f); }
void Fdn::setHoldRate(float r) { if (std::isfinite(r)) holdRate_ = std::clamp(r, -60.0f, 240.0f); }
void Fdn::setReleaseScale(float s) { if (std::isfinite(s)) releaseScale_ = std::clamp(s, 0.01f, 100.0f); }
void Fdn::setFreeze(bool on) { freezeTarget_ = on ? 1.0f : 0.0f; }

void Fdn::setRelease(float rt60, bool infinite) {
  if (std::isfinite(rt60)) rt60_ = std::clamp(rt60, 0.05f, 600.0f);
  infinite_ = infinite;
}

void Fdn::setPitch(float semitones, float mix) {
  for (auto& s : shifters_) s.setSemitones(semitones);
  if (std::isfinite(mix)) pitchMixTarget_ = std::clamp(mix, 0.0f, 1.0f);
}

void Fdn::setDamping(float d) {
  if (!std::isfinite(d)) return;
  damping_ = std::clamp(d, -1.0f, 1.0f);
  float dark = std::max(0.0f, -damping_);
  float fc = 20000.0f * std::pow(500.0f / 20000.0f, dark);        // 20 kHz .. 500 Hz
  fc = std::min(fc, 0.45f * float(sr_));
  lpCoef_ = std::exp(-kTwoPi * fc / float(sr_));
  lpBlend_ = std::min(1.0f, dark * 10.0f);                         // exact bypass at 0
  shelfGain_ = std::max(0.0f, damping_) * 0.41f;                   // highs end up to 3 dB above lows per pass
}

// Runs every kControlInterval samples: smooths Size and refreshes the per-line feedback gains.
void Fdn::controlTick() {
  size_ = sizeTarget_ + sizeCoef_ * (size_ - sizeTarget_);
  bool sizeSettled = std::fabs(size_ - sizeTarget_) < 1e-4f * sizeTarget_;
  if (sizeSettled) size_ = sizeTarget_;

  // Input diffusion grows with the room above the reference size, so a short sound in a huge space
  // comes back as a cloud rather than as a string of separate echoes.
  const float scale = std::clamp(size_ / kReferenceSize, 1.0f, kMaxDiffusionScale);
  const float extra = std::min(1.0f, (scale - 1.0f) * 2.0f);
  if (extra > 0.0f && extraDiffusion_ == 0.0f)                       // switching in: drop whatever they held last time
    for (auto& ch : diffusers_) for (int k = 4; k < kDiffusers; ++k) ch[size_t(k)].line.reset();
  extraDiffusion_ = extra;
  const float maxExtra = kMaxExtraDiffuserMs * 0.001f * float(sr_);
  for (int ch = 0; ch < 2; ++ch)
    for (int k = 0; k < kDiffusers; ++k) {
      // The first four keep their short, fixed lengths (fine grain); the extra four stretch with the room (coarse spread).
      float target = k < 4 ? diffuserBase_[ch][k] : std::clamp(diffuserBase_[ch][k] * (scale - 1.0f), 1.0f, maxExtra);
      if (sizeSettled) target = std::round(target);
      Allpass& a = diffusers_[size_t(ch)][size_t(k)];
      if (std::fabs(target - a.delay) < 1e-3f) { a.delay = target; a.inc = 0.0f; }
      else a.inc = (target - a.delay) / float(kControlInterval);
    }

  // Density, lower half: how much the lines mix.
  const float angleTarget = kPi * std::min(1.0f, densityTarget_ * 2.0f);
  if (mixAngle_ != angleTarget) {
    mixAngle_ = angleTarget + sizeCoef_ * (mixAngle_ - angleTarget);
    if (std::fabs(mixAngle_ - angleTarget) < 1e-3f) mixAngle_ = angleTarget;
    updateMixMatrix();
  }
  for (int i = 0; i < kLines; ++i) {
    float target = std::clamp(size_ / kSpeedOfSound * kRatios[i] * float(sr_), 4.0f, maxDelay_);
    if (sizeSettled) { target = std::round(target); }
    baseInc_[i] = (target - base_[i]) / float(kControlInterval);
    float apTarget = target * kApShare[i];
    if (sizeSettled) apTarget = std::round(apTarget);
    apInc_[i] = (apTarget - apBase_[i]) / float(kControlInterval);
    float seconds = target / float(sr_);
    gain_[i] = infinite_ ? 1.0f : std::pow(10.0f, -3.0f * seconds / rt60_ * releaseScale_);
    holdGain_[i] = holdRate_ == 0.0f ? 1.0f : std::pow(10.0f, -holdRate_ * seconds / 20.0f);
  }
  settled_ = sizeSettled;
}

void Fdn::process(float inL, float inR, float& outL, float& outR) {
  if (tick_ == 0) controlTick();
  tick_ = (tick_ + 1) % kControlInterval;

  mod_ = modTarget_ + modCoef_ * (mod_ - modTarget_);
  if (std::fabs(mod_ - modTarget_) < 1e-7f) mod_ = modTarget_;
  freeze_ = freezeTarget_ + freezeCoef_ * (freeze_ - freezeTarget_);
  if (std::fabs(freeze_ - freezeTarget_) < 1e-4f) freeze_ = freezeTarget_;
  pitchMix_ += 0.001f * (pitchMixTarget_ - pitchMix_);
  if (std::fabs(pitchMix_ - pitchMixTarget_) < 1e-5f) pitchMix_ = pitchMixTarget_;

  // Input diffusion.
  const float g = 0.75f * diffusion_;
  float dl = inL, dr = inR;
  for (int k = 0; k < 4; ++k) { dl = diffusers_[0][k].process(dl, g); dr = diffusers_[1][k].process(dr, g); }
  if (extraDiffusion_ > 0.0f) {
    const float ge = g * extraDiffusion_;
    for (int k = 4; k < kDiffusers; ++k) { dl = diffusers_[0][k].process(dl, ge); dr = diffusers_[1][k].process(dr, ge); }
  }
  const float inGain = kInputGain * (1.0f - freeze_);
  dl *= inGain; dr *= inGain;

  // hold: 1 while sustaining or frozen. It takes over the loop gain and removes the damping.
  const float hold = std::max(hold_, freeze_);
  const float lpBlend = lpBlend_ * (1.0f - hold);
  const float shelfGain = shelfGain_ * (1.0f - hold);
  const float shelfNorm = 1.0f / (1.0f + shelfGain);
  const bool integerDelays = settled_ && mod_ == 0.0f;

  // Density, upper half: gain of the diffusers inside the loop.
  const float apTarget = kApMaxGain * std::max(0.0f, densityTarget_ * 2.0f - 1.0f);
  apGain_ = apTarget + apGainCoef_ * (apGain_ - apTarget);
  if (std::fabs(apGain_ - apTarget) < 1e-4f) apGain_ = apTarget;
  const float apGain = apGain_;
  const float scale = 1.0f + mod_;

  float v[kLines];
  float sumL = 0, sumR = 0;
  for (int i = 0; i < kLines; ++i) {
    base_[i] += baseInc_[i]; apBase_[i] += apInc_[i];
    // Each line is split in two: a plain delay followed by a diffuser (a Schroeder all-pass) whose own delay
    // is carved out of the line. With the diffuser gain at 0 the two add up to the plain line again.
    float y, tap, apOut, out;                     // `out`: what this line contributes to the output
    if (integerDelays) {
      const int total = int(base_[i] + 0.5f), ap = std::clamp(int(apBase_[i] + 0.5f), 1, std::max(1, total - 3));
      tap = lines_[i].readInt(total - ap - 1);
      apOut = apLines_[i].readInt(ap - 1);
      y = apGain == 0.0f ? lines_[i].readInt(total - 1) : apOut - apGain * tap;
      out = 0.0f;
      for (int k = 0; k < kTaps; ++k) out += kTapPolarity[(k + i) & 3] * lines_[i].readInt(int(float(total) * kTapShare[i][k]));
    } else {
      const float total = std::clamp(base_[i] * scale, 4.0f, maxDelay_);
      const float ap = std::clamp(apBase_[i] * scale, 2.0f, std::max(2.0f, total - 3.0f));
      tap = lines_[i].read(total - ap - 1.0f);
      apOut = apLines_[i].read(ap - 1.0f);
      y = apGain == 0.0f ? lines_[i].read(total - 1.0f) : apOut - apGain * tap;
      out = 0.0f;
      for (int k = 0; k < kTaps; ++k) out += kTapPolarity[(k + i) & 3] * lines_[i].readLinear(total * kTapShare[i][k]);
    }
    apLines_[i].write(apGain == 0.0f ? tap : tap + apGain * y);    // kept warm even when unused, so switching it in is seamless
    if (i & 1) sumR += kTapSign[i] * out; else sumL += kTapSign[i] * out;

    // Damping.
    lp_[i] = y + lpCoef_ * (lp_[i] - y);
    if (std::fabs(lp_[i]) < 1e-20f) lp_[i] = 0.0f;
    shelf_[i] = y + shelfCoef_ * (shelf_[i] - y);
    if (std::fabs(shelf_[i]) < 1e-20f) shelf_[i] = 0.0f;
    y += lpBlend * (lp_[i] - y);
    // Brightening: highs keep the full loop gain, lows are turned down. Normalised so no frequency
    // ever exceeds the release gain, otherwise the loop would ring forever.
    y = (y + shelfGain * (y - shelf_[i])) * shelfNorm;

    // Pitch in the loop: every pass shifts the tail a little further.
    if ((i & 3) == 0) {
      float shifted = shifters_[size_t(i >> 2)].process(y);
      y += pitchMix_ * (shifted - y);
    }
    float g = gain_[i] + hold_ * (holdGain_[i] - gain_[i]);     // sustain: held, sinking or growing
    g += freeze_ * (1.0f - g);                                    // freeze: exactly lossless
    v[i] = y * g;
  }

  if (plainMix_) hadamard16(v); else mix16(v, mix4_);
  for (int i = 0; i < kLines; ++i) lines_[i].write(softClip(v[i]) + ((i & 1) ? dr : dl) * kTapSign[(i + 5) & 15]);

  float l = sumL * kOutputGain, r = sumR * kOutputGain;
  float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width_;
  outL = mid + side; outR = mid - side;
}

}  // namespace ei
