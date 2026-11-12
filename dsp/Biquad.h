// Biquad (transposed direct form II) with RBJ cookbook designs.
#pragma once
#include <algorithm>
#include <cmath>

namespace ei {

struct BiquadCoeffs {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

  enum class Type { LowCut, LowShelf, Bell, HighShelf, HighCut };

  static BiquadCoeffs design(Type type, double sampleRate, double freq, double gainDb, double q) {
    const double pi = 3.14159265358979323846;
    freq = std::clamp(freq, 20.0, 0.45 * sampleRate);
    q = std::clamp(q, 0.1, 10.0);
    gainDb = std::clamp(gainDb, -18.0, 18.0);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w = 2.0 * pi * freq / sampleRate, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type) {
      case Type::LowCut:
        b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case Type::HighCut:
        b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
      case Type::Bell:
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
      case Type::LowShelf: {
        const double s = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
        a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; break;
      }
      case Type::HighShelf: {
        const double s = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
        a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; break;
      }
    }
    BiquadCoeffs c;
    c.b0 = float(b0 / a0); c.b1 = float(b1 / a0); c.b2 = float(b2 / a0); c.a1 = float(a1 / a0); c.a2 = float(a2 / a0);
    return c;
  }

  // Magnitude in dB at `freq`.
  double magnitudeDb(double sampleRate, double freq) const {
    const double w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
    const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const double dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    return 10.0 * std::log10((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
  }
};

struct BiquadState {
  float z1 = 0, z2 = 0;
  float process(float x, const BiquadCoeffs& c) {
    float y = c.b0 * x + z1;
    z1 = c.b1 * x - c.a1 * y + z2;
    z2 = c.b2 * x - c.a2 * y;
    if (std::fabs(z1) < 1e-20f) z1 = 0;
    if (std::fabs(z2) < 1e-20f) z2 = 0;
    return y;
  }
  void reset() { z1 = z2 = 0; }
};

}  // namespace ei
