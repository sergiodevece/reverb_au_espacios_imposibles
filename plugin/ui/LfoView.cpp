#include "LfoView.h"

namespace ui {

void LfoView::update(int shape, float depth01, float phaseOffset01, float cyclesPerSecond, float dt) {
  shape_ = shape; depth_ = depth01; offset_ = phaseOffset01;
  phase_ += cyclesPerSecond * dt;
  phase_ -= std::floor(phase_);
  repaint();
}

float LfoView::valueAt(float phase) const {
  float p = phase + offset_; p -= std::floor(p);
  switch (shape_) {
    case 1: return p < 0.25f ? 4 * p : (p < 0.75f ? 2 - 4 * p : 4 * p - 4);
    case 2: return 2 * p - 1;
    case 3: return p < 0.5f ? 1.0f : -1.0f;
    case 4: {   // fixed pseudo-random steps, just to convey the idea
      static constexpr float steps[8] = {0.3f, -0.7f, 0.9f, -0.2f, 0.6f, -0.9f, 0.1f, -0.5f};
      const float x = p * 4.0f; const int i = int(x) & 7;
      return steps[i] + (steps[(i + 1) & 7] - steps[i]) * (x - std::floor(x));
    }
    default: return std::sin(juce::MathConstants<float>::twoPi * p);
  }
}

void LfoView::paint(juce::Graphics& g) {
  const auto r = getLocalBounds().toFloat();
  g.setColour(col::voidBlack); g.fillRect(r);
  g.setColour(col::line); g.drawRect(r, 1.0f);
  const float dashes[] = {1.0f, 4.0f};
  g.drawDashedLine({r.getX(), r.getCentreY(), r.getRight(), r.getCentreY()}, dashes, 2, 1.0f);

  const float amp = r.getHeight() * 0.38f * (0.15f + 0.85f * depth_);
  const float cycles = 2.0f;
  juce::Path wave;
  for (int i = 0; i <= 200; ++i) {
    const float u = float(i) / 200.0f;
    const float y = r.getCentreY() - valueAt(u * cycles) * amp;
    if (i == 0) wave.startNewSubPath(r.getX() + 1, y); else wave.lineTo(r.getX() + 1 + (r.getWidth() - 2) * u, y);
  }
  glowStroke(g, wave, col::green, 1.6f);
  const float u = phase_ / cycles;
  glowDot(g, {r.getX() + 1 + (r.getWidth() - 2) * u, r.getCentreY() - valueAt(phase_) * amp}, 3.5f, col::mint);
}

}  // namespace ui
