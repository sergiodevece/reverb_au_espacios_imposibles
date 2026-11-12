// Main screen: the tail envelope drawn over a live history of the input and reverb levels.
// PRE and ATT set how the reverb comes in; five free points (time, level) draw the tail itself.
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
  void mouseExit(const juce::MouseEvent&) override { hover_ = kNone; repaint(); }

private:
  static constexpr int kPoints = 5;
  static constexpr int kNone = -1, kPre = 0, kAttack = 1, kFirstPoint = 2, kNumHandles = kFirstPoint + kPoints;
  static constexpr int kHistory = 150;
  static constexpr float kMinTime = 0.001f, kMaxTime = 80.0f, kTopDb = 12.0f, kBottomDb = -60.0f;

  struct Shape {                                  // screen geometry of the envelope, worked out once per event
    float xPre, xAttack, yZero, bottom;
    juce::Point<float> pt[kPoints];
    float time[kPoints], level[kPoints];
    bool neverDies;
  };
  Shape shape() const;
  juce::Rectangle<float> plot() const;
  float timeToX(float seconds) const;
  float xToTime(float x) const;
  float levelToY(float db) const;
  float yToLevel(float y) const;
  juce::Point<float> handlePos(int handle, const Shape&) const;
  int handleAt(juce::Point<float>) const;
  float value(const juce::String& paramId) const;
  static float defaultOf(juce::ParameterAttachment&, juce::RangedAudioParameter&);

  juce::AudioProcessorValueTreeState& state_;
  juce::ParameterAttachment pre_, attack_;
  std::unique_ptr<juce::ParameterAttachment> time_[kPoints], level_[kPoints];
  std::array<float, kHistory> inDb_{}, wetDb_{};
  int head_ = 0;
  float gateGain_ = 1.0f, lfo_ = 0.0f, drift_ = 0.0f;
  int hover_ = kNone, drag_ = kNone;
};

}  // namespace ui
