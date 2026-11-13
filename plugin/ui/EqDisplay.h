// EQ curve with six draggable nodes. Drag = frequency/gain, wheel = Q, double-click = on/off.
#pragma once
#include "Theme.h"
#include "dsp/Eq6.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui {

class EqDisplay : public juce::Component {
public:
  explicit EqDisplay(juce::AudioProcessorValueTreeState& state);
  void refresh();                                   // re-reads the parameters; call once per UI frame

  void paint(juce::Graphics&) override;
  void mouseMove(const juce::MouseEvent&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseDoubleClick(const juce::MouseEvent&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
  struct Band {
    std::unique_ptr<juce::ParameterAttachment> on, freq, gain, q;
    ei::EqBand value;
  };
  juce::Rectangle<float> plot() const;
  float freqToX(float hz) const;
  float xToFreq(float x) const;
  float dbToY(float db) const;
  float yToDb(float y) const;
  bool isCut(int band) const;
  juce::Point<float> nodePos(int band) const;
  int bandAt(juce::Point<float> p) const;

  juce::AudioProcessorValueTreeState& state_;
  std::array<Band, 6> bands_;
  ei::Eq6 model_;                                   // UI-side copy, so the curve is right even when the host is stopped
  bool highCut_ = false;
  int selected_ = 4, drag_ = -1, hover_ = -1;
  juce::uint32 signature_ = 0;
};

}  // namespace ui
