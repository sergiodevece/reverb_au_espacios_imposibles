// LFO with a manual mode and an "auto" mode that syncs to tempo and backs off
// while the source is playing, so the movement shows up in the tails.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ei {

class Lfo {
public:
  enum class Mode { Auto, Manual };
  enum class Shape { Sine, Triangle, Saw, Square, Random };
  static constexpr int kDivisions = 9;

  // 8 bars, 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32 — in quarter-note beats.
  static float divisionToBeats(int index) {
    static constexpr float t[kDivisions] = {32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f};
    return t[std::clamp(index, 0, kDivisions - 1)];
  }

  void prepare(double sampleRate) {
    sr_ = sampleRate;
    duckCoef_ = float(std::exp(-1.0 / (0.1 * sampleRate)));
    reset();
  }
  void reset() { phase_ = 0; duck_ = 1; rndPrev_ = rndNext_ = 0; seed_ = 0x1234567u; nextRandom(); nextRandom(); }

  void setMode(Mode m) { mode_ = m; }
  void setShape(Shape s) { shape_ = s; }
  void setRateHz(float hz) { if (std::isfinite(hz)) rateHz_ = std::clamp(hz, 0.01f, 20.0f); }
  void setSync(bool on, float beats) { sync_ = on; if (std::isfinite(beats)) beats_ = std::clamp(beats, 0.125f, 32.0f); }
  void setDepth(float d) { if (std::isfinite(d)) depth_ = std::clamp(d, 0.0f, 1.0f); }
  void setPhase(float degrees) { if (std::isfinite(degrees)) offset_ = degrees / 360.0f - std::floor(degrees / 360.0f); }
  void setTempo(double bpm) { bpm_ = (std::isfinite(bpm) && bpm > 0.0) ? std::clamp(bpm, 20.0, 999.0) : 120.0; }

  // inputLevel01: 0 = silence, 1 = full-scale source. Returns a value in [-depth, +depth].
  float process(float inputLevel01) {
    const bool autoMode = mode_ == Mode::Auto;
    const double hz = autoMode ? bpm_ / 60.0 / 4.0
                               : (sync_ ? bpm_ / 60.0 / double(beats_) : double(rateHz_));
    phase_ += hz / sr_;
    if (phase_ >= 1.0) { phase_ -= std::floor(phase_); nextRandom(); }

    float p = float(phase_) + (autoMode ? 0.0f : offset_);
    if (p >= 1.0f) p -= 1.0f;
    float v = 0;
    switch (autoMode ? Shape::Sine : shape_) {
      case Shape::Sine:     v = std::sin(6.28318530717959f * p); break;
      case Shape::Triangle: v = p < 0.25f ? 4 * p : (p < 0.75f ? 2 - 4 * p : 4 * p - 4); break;
      case Shape::Saw:      v = 2 * p - 1; break;
      case Shape::Square:   v = p < 0.5f ? 1.0f : -1.0f; break;
      case Shape::Random:   v = rndPrev_ + (rndNext_ - rndPrev_) * float(phase_); break;
    }
    const float duckTarget = autoMode ? 1.0f - std::clamp(inputLevel01, 0.0f, 1.0f) : 1.0f;
    duck_ = duckTarget + duckCoef_ * (duck_ - duckTarget);
    return v * depth_ * duck_;
  }

  float phase01() const { return float(phase_); }

private:
  void nextRandom() {
    seed_ = seed_ * 1664525u + 1013904223u;
    rndPrev_ = rndNext_;
    rndNext_ = float(seed_ >> 8) / float(1u << 23) - 1.0f;
    rndNext_ = std::clamp(rndNext_, -1.0f, 1.0f);
  }

  double sr_ = 48000, phase_ = 0, bpm_ = 120;
  Mode mode_ = Mode::Auto; Shape shape_ = Shape::Sine;
  float rateHz_ = 0.5f, beats_ = 4, depth_ = 0.2f, offset_ = 0, duck_ = 1, duckCoef_ = 0;
  bool sync_ = false;
  float rndPrev_ = 0, rndNext_ = 0; uint32_t seed_ = 0x1234567u;
};

}  // namespace ei
