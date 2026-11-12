#pragma once
#include "PluginProcessor.h"
#include "ui/EnvelopeDisplay.h"
#include "ui/EqDisplay.h"
#include "ui/LfoView.h"
#include "ui/Widgets.h"

class ImpossibleSpacesEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
  static constexpr int kWidth = 1000, kHeight = 640;

  explicit ImpossibleSpacesEditor(ImpossibleSpacesProcessor&);
  ~ImpossibleSpacesEditor() override = default;
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;
  float value(const char* paramId) const;

  juce::SharedResourcePointer<ui::Fonts> fonts_;      // first member: outlives every component that draws text
  ImpossibleSpacesProcessor& processor_;
  juce::AudioProcessorValueTreeState& state_;

  ui::EnvelopeDisplay envelope_;
  ui::EqDisplay eq_;
  ui::Chips eqBand6Mode_;

  // Space
  ui::Knob size_, diffusion_, damping_, width_, pitch_, pitchMix_;
  ui::Toggle freeze_;
  // LFO
  ui::Toggle lfoOn_;
  ui::Chips lfoMode_, lfoShape_, lfoTarget_;
  ui::LfoView lfoView_;
  ui::Knob lfoRate_, lfoDivision_, lfoDepth_, lfoPhase_;
  ui::Toggle lfoSync_;
  // Reverse
  ui::Toggle revOn_;
  ui::Knob revWindow_, revDivision_;
  ui::Chips revSync_;
  // Gate
  ui::Toggle gateOn_;
  ui::Knob gateThreshold_, gateRange_, gateAttack_, gateHold_, gateRelease_;
  ui::Chips gateSource_;
  // Output
  ui::Knob mix_, output_;

  float heldIn_ = 0, heldWet_ = 0, lastGate_ = 1, lastLfo_ = 0;
  bool lastRevOn_ = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImpossibleSpacesEditor)
};
