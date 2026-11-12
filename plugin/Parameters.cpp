#include "Parameters.h"

namespace ei {
namespace {
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Range = juce::NormalisableRange<float>;

Range skewed(float lo, float hi, float centre, float step = 0.0f) {
  Range r(lo, hi, step);
  r.setSkewForCentre(centre);
  return r;
}

void addFloat(Layout& l, const juce::String& pid, const juce::String& name, Range range, float def,
              const juce::String& unit, int decimals = 1) {
  auto attrs = juce::AudioParameterFloatAttributes()
                   .withLabel(unit)
                   .withStringFromValueFunction([decimals](float v, int) { return juce::String(v, decimals); });
  l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{pid, 1}, name, range, def, attrs));
}
void addBool(Layout& l, const juce::String& pid, const juce::String& name, bool def) {
  l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{pid, 1}, name, def));
}
void addChoice(Layout& l, const juce::String& pid, const juce::String& name, const juce::StringArray& choices, int def) {
  l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{pid, 1}, name, choices, def));
}

const juce::StringArray kDivisions{"8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32"};
}  // namespace

Layout createParameterLayout() {
  Layout l;
  const EngineParams d;   // single source of truth for the defaults

  // Space
  addFloat(l, id::size, "Size", skewed(1.0f, 1000.0f, 30.0f), d.size, "m");
  addFloat(l, id::diffusion, "Diffusion", Range(0.0f, 100.0f), d.diffusion * 100.0f, "%", 0);
  addFloat(l, id::damping, "Damping", Range(-100.0f, 100.0f), d.damping * 100.0f, "%", 0);
  addFloat(l, id::pitch, "Pitch", Range(-12.0f, 12.0f, 0.01f), d.pitch, "st", 2);
  addFloat(l, id::pitchMix, "Pitch Mix", Range(0.0f, 100.0f), d.pitchMix * 100.0f, "%", 0);
  addFloat(l, id::width, "Width", Range(0.0f, 200.0f), d.width * 100.0f, "%", 0);
  addBool(l, id::freeze, "Freeze", d.freeze);

  // Tail envelope
  addFloat(l, id::preDelay, "Pre-Delay", skewed(0.0f, 500.0f, 50.0f), d.preDelayMs, "ms");
  addFloat(l, id::sustain, "Sustain", skewed(0.0f, 10.0f, 1.5f), d.sustainSec, "s", 2);
  addFloat(l, id::release, "Release", skewed(0.1f, 60.0f, 4.0f), d.releaseSec, "s", 2);
  addBool(l, id::releaseInf, "Release Infinite", d.releaseInfinite);

  // EQ
  for (int b = 1; b <= 6; ++b) {
    const EqBand& band = d.eq[b - 1];
    const juce::String n = "EQ " + juce::String(b) + " ";
    addBool(l, id::eq(b, "On"), n + "On", band.on);
    addFloat(l, id::eq(b, "Freq"), n + "Freq", skewed(20.0f, 20000.0f, 632.0f), band.freq, "Hz", 0);
    addFloat(l, id::eq(b, "Gain"), n + "Gain", Range(-18.0f, 18.0f), band.gainDb, "dB");
    addFloat(l, id::eq(b, "Q"), n + "Q", skewed(0.1f, 10.0f, 1.0f), band.q, "", 2);
  }
  addBool(l, id::eq6HighCut, "EQ 6 High-Cut", d.eqBand6HighCut);

  // LFO
  addChoice(l, id::lfoMode, "LFO Mode", {"Auto", "Manual"}, int(d.lfoMode));
  addChoice(l, id::lfoShape, "LFO Shape", {"Sine", "Triangle", "Saw", "Square", "Random"}, int(d.lfoShape));
  addFloat(l, id::lfoRate, "LFO Rate", skewed(0.01f, 20.0f, 1.0f), d.lfoRateHz, "Hz", 2);
  addBool(l, id::lfoSync, "LFO Sync", d.lfoSync);
  addChoice(l, id::lfoDivision, "LFO Division", kDivisions, d.lfoDivision);
  addFloat(l, id::lfoDepth, "LFO Depth", Range(0.0f, 100.0f), d.lfoDepth * 100.0f, "%", 0);
  addFloat(l, id::lfoPhase, "LFO Phase", Range(0.0f, 360.0f), d.lfoPhaseDeg, juce::String::fromUTF8("°"), 0);
  addChoice(l, id::lfoTarget, "LFO Target", {"Size", "Pitch", "Damping", "Pan"}, int(d.lfoTarget));

  // Reverse
  addBool(l, id::revOn, "Reverse", d.reverseOn);
  addFloat(l, id::revWindow, "Reverse Window", skewed(50.0f, 2000.0f, 400.0f), d.reverseWindowMs, "ms", 0);
  addBool(l, id::revSync, "Reverse Sync", d.reverseSync);
  addChoice(l, id::revDivision, "Reverse Division", kDivisions, d.reverseDivision);

  // Gate
  addBool(l, id::gateOn, "Gate", d.gateOn);
  addFloat(l, id::gateThreshold, "Gate Threshold", Range(-80.0f, 0.0f), d.gateThresholdDb, "dB");
  addFloat(l, id::gateAttack, "Gate Attack", skewed(0.1f, 100.0f, 5.0f), d.gateAttackMs, "ms");
  addFloat(l, id::gateHold, "Gate Hold", skewed(0.0f, 2000.0f, 200.0f), d.gateHoldMs, "ms", 0);
  addFloat(l, id::gateRelease, "Gate Release", skewed(5.0f, 2000.0f, 150.0f), d.gateReleaseMs, "ms", 0);
  addFloat(l, id::gateRange, "Gate Range", Range(-80.0f, 0.0f), d.gateRangeDb, "dB");
  addChoice(l, id::gateSource, "Gate Source", {"Dry", "Wet"}, int(d.gateSource));

  // Output
  addFloat(l, id::mix, "Mix", Range(0.0f, 100.0f), d.mix * 100.0f, "%", 0);
  addFloat(l, id::output, "Output", Range(-24.0f, 12.0f), d.outputDb, "dB");
  return l;
}

std::atomic<float>* ParamReader::get(const juce::String& paramId) const {
  auto* p = state_.getRawParameterValue(paramId);
  jassert(p != nullptr);
  return p;
}

ParamReader::ParamReader(juce::AudioProcessorValueTreeState& state) : state_(state) {
#define EI_GET(name) name = get(id::name)
  EI_GET(size); EI_GET(diffusion); EI_GET(damping); EI_GET(pitch); EI_GET(pitchMix); EI_GET(width); EI_GET(freeze);
  EI_GET(preDelay); EI_GET(sustain); EI_GET(release); EI_GET(releaseInf); EI_GET(eq6HighCut);
  EI_GET(lfoMode); EI_GET(lfoShape); EI_GET(lfoRate); EI_GET(lfoSync); EI_GET(lfoDivision); EI_GET(lfoDepth); EI_GET(lfoPhase); EI_GET(lfoTarget);
  EI_GET(revOn); EI_GET(revWindow); EI_GET(revSync); EI_GET(revDivision);
  EI_GET(gateOn); EI_GET(gateThreshold); EI_GET(gateAttack); EI_GET(gateHold); EI_GET(gateRelease); EI_GET(gateRange); EI_GET(gateSource);
  EI_GET(mix); EI_GET(output);
#undef EI_GET
  for (int b = 0; b < 6; ++b) {
    eqOn[b] = get(id::eq(b + 1, "On")); eqFreq[b] = get(id::eq(b + 1, "Freq"));
    eqGain[b] = get(id::eq(b + 1, "Gain")); eqQ[b] = get(id::eq(b + 1, "Q"));
  }
}

EngineParams ParamReader::read() const {
  auto f = [](const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); };
  auto on = [&](const std::atomic<float>* p) { return f(p) > 0.5f; };
  auto idx = [&](const std::atomic<float>* p) { return int(f(p) + 0.5f); };

  EngineParams e;
  e.size = f(size); e.diffusion = f(diffusion) * 0.01f; e.damping = f(damping) * 0.01f;
  e.pitch = f(pitch); e.pitchMix = f(pitchMix) * 0.01f; e.width = f(width) * 0.01f; e.freeze = on(freeze);
  e.preDelayMs = f(preDelay); e.sustainSec = f(sustain); e.releaseSec = f(release); e.releaseInfinite = on(releaseInf);
  for (int b = 0; b < 6; ++b) { e.eq[b].on = on(eqOn[b]); e.eq[b].freq = f(eqFreq[b]); e.eq[b].gainDb = f(eqGain[b]); e.eq[b].q = f(eqQ[b]); }
  e.eqBand6HighCut = on(eq6HighCut);
  e.lfoMode = Lfo::Mode(idx(lfoMode)); e.lfoShape = Lfo::Shape(idx(lfoShape)); e.lfoRateHz = f(lfoRate); e.lfoSync = on(lfoSync);
  e.lfoDivision = idx(lfoDivision); e.lfoDepth = f(lfoDepth) * 0.01f; e.lfoPhaseDeg = f(lfoPhase);
  e.lfoTarget = EngineParams::LfoTarget(idx(lfoTarget));
  e.reverseOn = on(revOn); e.reverseWindowMs = f(revWindow); e.reverseSync = on(revSync); e.reverseDivision = idx(revDivision);
  e.gateOn = on(gateOn); e.gateThresholdDb = f(gateThreshold); e.gateAttackMs = f(gateAttack); e.gateHoldMs = f(gateHold);
  e.gateReleaseMs = f(gateRelease); e.gateRangeDb = f(gateRange); e.gateSource = EngineParams::GateSource(idx(gateSource));
  e.mix = f(mix) * 0.01f; e.outputDb = f(output);
  return e;
}

}  // namespace ei
