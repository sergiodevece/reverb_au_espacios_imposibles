// Main screen: the tail envelope (pre-delay / sustain / release) drawn over a
// live history of the input and reverb levels.
#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui {

class EnvelopeDisplay : public juce::Component {
public:
  explicit EnvelopeDisplay(juce::AudioProcessorValueTreeState& state);

  // One call per UI frame. Levels are linear peaks; gateGain is the gain the gate applied.
  void pushFrame(float inputPeak, float wetPeak, float gateGain, float lfoValue);

  void paint(juce::Graphics&) override;
  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseExit(const juce::MouseEvent&) override { hover_ = None; repaint(); }

private:
  enum Handle { None = -1, Pre, Sus, Rel };
  static constexpr int kHistory = 150;
  static constexpr float kMinTime = 0.001f, kMaxTime = 80.0f, kFloorDb = -72.0f;

  juce::Rectangle<float> plot() const;
  float timeToX(float seconds) const;
  float xToTime(float x) const;
  float dbToY(float db) const;
  juce::Point<float> handlePos(Handle h) const;
  Handle handleAt(juce::Point<float> p) const;
  float value(const char* paramId) const;

  juce::AudioProcessorValueTreeState& state_;
  juce::ParameterAttachment pre_, sus_, rel_, inf_;
  std::array<float, kHistory> inDb_{}, wetDb_{};
  int head_ = 0;
  float gateGain_ = 1.0f, lfo_ = 0.0f, drift_ = 0.0f;
  Handle hover_ = None, drag_ = None;
};

}  // namespace ui
