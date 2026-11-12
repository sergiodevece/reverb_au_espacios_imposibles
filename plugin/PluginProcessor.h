#pragma once
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

class ImpossibleSpacesProcessor : public juce::AudioProcessor, private juce::Timer {
public:
  // One entry per audio block, consumed by the editor.
  struct MeterFrame { float in = 0, wet = 0, gateGain = 1, lfo = 0, hold = 0; };

  ImpossibleSpacesProcessor();
  ~ImpossibleSpacesProcessor() override { stopTimer(); }

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void reset() override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "Impossible Spaces"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override;

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  bool popMeterFrame(MeterFrame& frame);                 // UI thread
  const ei::Eq6& eq() const { return engine_.eq(); }
  double currentBpm() const { return bpm_.load(); }

  juce::AudioProcessorValueTreeState apvts;

private:
  void timerCallback() override;

  ei::ReverbEngine engine_;
  ei::ParamReader params_;
  static constexpr int kMeterFifoSize = 512;
  juce::AbstractFifo meterFifo_{kMeterFifoSize};
  std::array<MeterFrame, kMeterFifoSize> meterFrames_{};
  std::atomic<double> bpm_{120.0};
  std::atomic<int> wantedLatency_{0};     // written by the audio thread
  int lastSeenLatency_ = 0;               // message thread only

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImpossibleSpacesProcessor)
};
