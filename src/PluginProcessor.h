#pragma once

#include <JuceHeader.h>
#include "NinjamClientService.h"

class NinjamNextAudioProcessor final : public juce::AudioProcessor
{
public:
  NinjamNextAudioProcessor();
  ~NinjamNextAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  void connectToServer(const juce::String& host, const juce::String& user, const juce::String& password);
  void disconnectFromServer();
  void sendUserCommand(const juce::String& commandText);
  void setMonitorMode(NinjamClientService::MonitorMode mode);
  NinjamClientService::MonitorMode getMonitorMode() const;
  void setMetronomeEnabled(bool enabled);
  bool getMetronomeEnabled() const;

  void setUserChannelMute(int userIdx, int channelIdx, bool mute);
  void setUserChannelSolo(int userIdx, int channelIdx, bool solo);
  void setUserChannelVolume(int userIdx, int channelIdx, float volume);

  NinjamClientService& getClientService();
  const NinjamClientService& getClientService() const;

private:
  void initialiseSettings();
  void loadCredentialsFromSettings();
  void saveMonitorModeSetting(NinjamClientService::MonitorMode mode);
  void saveMetronomeSetting(bool enabled);
  NinjamClientService::TransportState buildTransportState(int numSamples);

  juce::ApplicationProperties appProperties;
  NinjamClientService clientService;
  double sampleRateHz = 48000.0;
  double lastHostTimeSeconds = -1.0;
  double lastHostPpq = 0.0;
  bool lastHostPpqValid = false;
  bool lastHostWasPlaying = false;
  bool autoConnectAttempted = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NinjamNextAudioProcessor)
};
