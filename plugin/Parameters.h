// Parameter IDs, layout and the bridge from host parameters to the DSP engine.
// The ID strings are part of saved projects: never rename them.
#pragma once
#include "dsp/ReverbEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ei {

namespace id {
#define EI_ID(name) inline constexpr const char* name = #name
EI_ID(size); EI_ID(diffusion); EI_ID(damping); EI_ID(pitch); EI_ID(pitchMix); EI_ID(width); EI_ID(freeze);
EI_ID(preDelay); EI_ID(sustain); EI_ID(release); EI_ID(releaseInf);
EI_ID(eq6HighCut);
EI_ID(lfoMode); EI_ID(lfoShape); EI_ID(lfoRate); EI_ID(lfoSync); EI_ID(lfoDivision); EI_ID(lfoDepth); EI_ID(lfoPhase); EI_ID(lfoTarget);
EI_ID(revOn); EI_ID(revWindow); EI_ID(revSync); EI_ID(revDivision);
EI_ID(gateOn); EI_ID(gateThreshold); EI_ID(gateAttack); EI_ID(gateHold); EI_ID(gateRelease); EI_ID(gateRange); EI_ID(gateSource);
EI_ID(mix); EI_ID(output);
#undef EI_ID
// Per-band IDs: eq1On, eq1Freq, eq1Gain, eq1Q ... eq6Q (band is 1-based).
inline juce::String eq(int band, const char* what) { return "eq" + juce::String(band) + what; }
}  // namespace id

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Raw parameter pointers resolved once, so the audio thread never looks up strings.
class ParamReader {
public:
  explicit ParamReader(juce::AudioProcessorValueTreeState& state);
  EngineParams read() const;

private:
  std::atomic<float>* get(const juce::String& paramId) const;
  juce::AudioProcessorValueTreeState& state_;
  std::atomic<float> *size, *diffusion, *damping, *pitch, *pitchMix, *width, *freeze;
  std::atomic<float> *preDelay, *sustain, *release, *releaseInf;
  std::atomic<float> *eqOn[6], *eqFreq[6], *eqGain[6], *eqQ[6], *eq6HighCut;
  std::atomic<float> *lfoMode, *lfoShape, *lfoRate, *lfoSync, *lfoDivision, *lfoDepth, *lfoPhase, *lfoTarget;
  std::atomic<float> *revOn, *revWindow, *revSync, *revDivision;
  std::atomic<float> *gateOn, *gateThreshold, *gateAttack, *gateHold, *gateRelease, *gateRange, *gateSource;
  std::atomic<float> *mix, *output;
};

}  // namespace ei
