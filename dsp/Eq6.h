// Six-band stereo EQ: low-cut, low-shelf, three bells, high-shelf / high-cut.
#pragma once
#include "Biquad.h"
#include <array>
#include <atomic>

namespace ei {

struct EqBand { bool on = true; float freq = 1000; float gainDb = 0; float q = 0.707f; };

class Eq6 {
public:
  using Type = BiquadCoeffs::Type;
  static constexpr int kBands = 6;
  static constexpr float kDefaultFreq[kBands] = {40.0f, 150.0f, 400.0f, 1500.0f, 5000.0f, 10000.0f};

  Eq6() {
    for (int i = 0; i < kBands; ++i) { bands_[i].freq = kDefaultFreq[i]; bands_[i].on = i != 0; }
    for (auto& s : shared_) for (auto& v : s) v.store(0.0f);
    for (auto& s : shared_) s[0].store(1.0f);
  }

  static Type typeOf(int index, bool band6HighCut) {
    switch (index) {
      case 0: return Type::LowCut;
      case 1: return Type::LowShelf;
      case 5: return band6HighCut ? Type::HighCut : Type::HighShelf;
      default: return Type::Bell;
    }
  }

  void prepare(double sampleRate) {
    sr_ = sampleRate; sharedSr_.store(float(sampleRate));
    for (int i = 0; i < kBands; ++i) update(i);
    reset();
  }
  void reset() { for (auto& ch : state_) for (auto& s : ch) s.reset(); }

  void setBand(int index, const EqBand& b) {
    if (index < 0 || index >= kBands) return;
    EqBand& cur = bands_[index];
    if (cur.on == b.on && cur.freq == b.freq && cur.gainDb == b.gainDb && cur.q == b.q) return;
    cur = b;
    if (!std::isfinite(cur.freq)) cur.freq = kDefaultFreq[index];
    if (!std::isfinite(cur.gainDb)) cur.gainDb = 0;
    if (!std::isfinite(cur.q)) cur.q = 0.707f;
    update(index);
  }
  void setBand6HighCut(bool highCut) { if (highCut != highCut_) { highCut_ = highCut; update(5); } }

  void process(float& l, float& r) {
    for (int i = 0; i < kBands; ++i) {
      if (!active_[i]) continue;
      l = state_[0][i].process(l, coeffs_[i]);
      r = state_[1][i].process(r, coeffs_[i]);
    }
  }

  // Total response. Reads atomics only, so the UI thread may call it while audio runs.
  float magnitudeDb(float freqHz) const {
    const double sr = sharedSr_.load();
    double db = 0;
    for (int i = 0; i < kBands; ++i) {
      BiquadCoeffs c;
      c.b0 = shared_[i][0].load(); c.b1 = shared_[i][1].load(); c.b2 = shared_[i][2].load();
      c.a1 = shared_[i][3].load(); c.a2 = shared_[i][4].load();
      db += c.magnitudeDb(sr, freqHz);
    }
    return float(db);
  }

private:
  void update(int i) {
    const EqBand& b = bands_[i];
    const Type t = typeOf(i, highCut_);
    const bool isCut = t == Type::LowCut || t == Type::HighCut;
    active_[i] = b.on && (isCut || b.gainDb != 0.0f);
    coeffs_[i] = active_[i] ? BiquadCoeffs::design(t, sr_, b.freq, b.gainDb, b.q) : BiquadCoeffs{};
    const BiquadCoeffs& c = coeffs_[i];
    shared_[i][0].store(c.b0); shared_[i][1].store(c.b1); shared_[i][2].store(c.b2);
    shared_[i][3].store(c.a1); shared_[i][4].store(c.a2);
  }

  double sr_ = 48000;
  std::array<EqBand, kBands> bands_{};
  std::array<BiquadCoeffs, kBands> coeffs_{};
  std::array<bool, kBands> active_{};
  std::array<std::array<BiquadState, kBands>, 2> state_{};
  std::array<std::array<std::atomic<float>, 5>, kBands> shared_;
  std::atomic<float> sharedSr_{48000.0f};
  bool highCut_ = false;
};

}  // namespace ei
