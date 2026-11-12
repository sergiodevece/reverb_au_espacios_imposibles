// Circular delay line with integer and 4-point Hermite interpolated reads.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace ei {

class DelayLine {
public:
  // Allocates. Call from prepare(), never from the audio callback.
  void prepare(int maxSamples) {
    max_ = std::max(8, maxSamples);
    int size = 1;
    while (size < max_ + 4) size <<= 1;
    buf_.assign(size_t(size), 0.0f);
    mask_ = size - 1;
    w_ = 0;
  }
  void reset() { std::fill(buf_.begin(), buf_.end(), 0.0f); w_ = 0; }
  int maxDelay() const { return max_; }

  void write(float x) {
    buf_[size_t(w_)] = x;
    w_ = (w_ + 1) & mask_;
  }
  // delay 0 = the sample just written.
  float readInt(int delaySamples) const {
    int d = std::clamp(delaySamples, 0, max_);
    return buf_[size_t((w_ - 1 - d) & mask_)];
  }
  float read(float delaySamples) const {
    float d = delaySamples;
    if (!(d >= 1.0f)) d = 1.0f;                       // also catches NaN
    d = std::min(d, float(max_ - 3));
    int n = int(d);
    float f = d - float(n);
    int base = w_ - 1 - n;
    float ym = buf_[size_t((base + 1) & mask_)];
    float y0 = buf_[size_t(base & mask_)];
    float y1 = buf_[size_t((base - 1) & mask_)];
    float y2 = buf_[size_t((base - 2) & mask_)];
    float c1 = 0.5f * (y1 - ym);
    float c2 = ym - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
  }

private:
  std::vector<float> buf_;
  int mask_ = 0, w_ = 0, max_ = 8;
};

}  // namespace ei
