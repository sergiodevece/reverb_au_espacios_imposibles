// Shows the LFO waveform and where it currently is.
#pragma once
#include "Theme.h"

namespace ui {

class LfoView : public juce::Component {
public:
  // shape: 0 sine, 1 triangle, 2 saw, 3 square, 4 random. cyclesPerSecond drives the moving dot.
  void update(int shape, float depth01, float phaseOffset01, float cyclesPerSecond, float dtSeconds);
  void paint(juce::Graphics&) override;

private:
  float valueAt(float phase) const;
  int shape_ = 0;
  float depth_ = 0.2f, offset_ = 0.0f, phase_ = 0.0f;
};

}  // namespace ui
