#include "PluginProcessor.h"
#include "PluginEditor.h"

ImpossibleSpacesProcessor::ImpossibleSpacesProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ImpossibleSpaces", ei::createParameterLayout()),
      params_(apvts) {
  startTimerHz(10);
}

bool ImpossibleSpacesProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::stereo()) return false;
  return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void ImpossibleSpacesProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  engine_.prepare(sampleRate, samplesPerBlock);
  engine_.setParams(params_.read());
  engine_.reset();
  const int latency = engine_.latencySamples();
  wantedLatency_.store(latency); lastSeenLatency_ = latency;
  setLatencySamples(latency);
}

void ImpossibleSpacesProcessor::reset() { engine_.reset(); }

void ImpossibleSpacesProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  if (buffer.getNumChannels() < 2 || n == 0) return;

  // Mono in, stereo out: the host hands us two channels with only the first one filled.
  if (getTotalNumInputChannels() < 2) buffer.copyFrom(1, 0, buffer, 0, 0, n);
  for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

  double bpm = 0.0;
  if (auto* head = getPlayHead())
    if (auto pos = head->getPosition())
      if (auto b = pos->getBpm()) bpm = *b;
  bpm_.store(bpm > 0.0 ? bpm : 120.0);

  engine_.setTempo(bpm);                         // <= 0 falls back to 120 inside the engine
  engine_.setParams(params_.read());

  wantedLatency_.store(engine_.latencySamples(), std::memory_order_relaxed);   // the host is told from timerCallback()

  engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n);

  const ei::Meters m = engine_.meters();
  const auto scope = meterFifo_.write(1);
  if (scope.blockSize1 > 0) meterFrames_[size_t(scope.startIndex1)] = {m.inputLevel, m.wetLevel, m.gateGain, m.lfoValue, m.hold};
}

// Message thread. setLatencySamples() takes locks and makes the host rebuild its delay compensation,
// so it is kept off the audio thread and only sent once the value has held still for a tick
// (dragging Window or a tempo ramp would otherwise flood the host).
void ImpossibleSpacesProcessor::timerCallback() {
  const int wanted = wantedLatency_.load(std::memory_order_relaxed);
  if (wanted == lastSeenLatency_ && wanted != getLatencySamples()) setLatencySamples(wanted);
  lastSeenLatency_ = wanted;
}

bool ImpossibleSpacesProcessor::popMeterFrame(MeterFrame& frame) {
  const auto scope = meterFifo_.read(1);
  if (scope.blockSize1 == 0) return false;
  frame = meterFrames_[size_t(scope.startIndex1)];
  return true;
}

double ImpossibleSpacesProcessor::getTailLengthSeconds() const {
  const ei::EngineParams p = params_.read();
  if (p.releaseInfinite || p.freeze) return 60.0;
  return double(p.releaseSec + p.sustainSec + p.preDelayMs * 0.001f) + (p.reverseOn ? 2.0 : 0.0);
}

void ImpossibleSpacesProcessor::getStateInformation(juce::MemoryBlock& destData) {
  if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void ImpossibleSpacesProcessor::setStateInformation(const void* data, int sizeInBytes) {
  if (auto xml = getXmlFromBinary(data, sizeInBytes))
    if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* ImpossibleSpacesProcessor::createEditor() { return new ImpossibleSpacesEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ImpossibleSpacesProcessor(); }
