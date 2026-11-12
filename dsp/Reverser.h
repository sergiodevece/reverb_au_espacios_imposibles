// Real-time reverse: two overlapping grains each replay the last `window`
// samples backwards under a sin^2 window. Latency equals the window length.
#pragma once
#include "DelayLine.h"
#include <algorithm>
#include <cmath>

namespace ei {

class Reverser {
public:
  void prepare(double sampleRate, float maxWindowSeconds = 2.0f) {
    maxWindow_ = std::max(64, int(double(maxWindowSeconds) * sampleRate));
    for (auto& l : lines_) l.prepare(2 * maxWindow_ + 8);
    setWindowSamples(window_);
    reset();
  }
  void reset() {
    for (auto& l : lines_) l.reset();
    grains_[0] = Grain{0, window_, true};
    grains_[1] = Grain{0, window_, false};
    clock_ = 0; period_ = window_;
  }
  void setWindowSamples(int n) { window_ = std::clamp(n, 64, maxWindow_); }
  int latencySamples() const { return window_; }

  void process(float& l, float& r) {
    lines_[0].write(l); lines_[1].write(r);

    // Grain 0 sets the pace and restarts as soon as it ends, picking up any new window length.
    // Grain 1 starts half a period later; if it is still busy (the window just shrank) it skips a turn.
    if (!grains_[0].active) { grains_[0] = Grain{0, window_, true}; clock_ = 0; period_ = window_; }
    if (!grains_[1].active && clock_ == period_ / 2) grains_[1] = Grain{0, period_, true};
    ++clock_;

    float outL = 0, outR = 0;
    for (auto& g : grains_) {
      if (!g.active) continue;
      float s = std::sin(3.14159265358979f * float(g.pos) / float(g.length));
      float w = s * s;
      outL += w * lines_[0].readInt(2 * g.pos);
      outR += w * lines_[1].readInt(2 * g.pos);
      if (++g.pos >= g.length) g.active = false;
    }
    l = outL * kMakeup; r = outR * kMakeup;
  }

private:
  // Two sin^2 grains reading different moments sum to 0.75 in power; bring that back to unity.
  static constexpr float kMakeup = 1.1547f;
  struct Grain { int pos = 0, length = 64; bool active = false; };
  DelayLine lines_[2];
  Grain grains_[2];
  int window_ = 24000, maxWindow_ = 96000, clock_ = 0, period_ = 24000;
};

}  // namespace ei
