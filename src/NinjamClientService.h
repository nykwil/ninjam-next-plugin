#pragma once

#include <JuceHeader.h>
#include "njclient.h"

class NinjamClientService : private juce::Timer
{
public:
  struct TransportState
  {
    bool isPlaying = true;
    bool isSeek = false;
    double hostTimeSeconds = -1.0;
    double hostBpm = 0.0;
    double hostPpqPosition = 0.0;
    bool hostBpmValid = false;
    bool hostPpqValid = false;
  };

  struct Snapshot
  {
    bool connected = false;
    juce::String statusText = "Disconnected";
    juce::String host;
    juce::String user;
    juce::String password;
    int bpm = 120;
    int bpi = 16;
    float intervalProgress = 0.0f;
    float localMeter = 0.0f;
    float remoteMeter = 0.0f;
    float localGain = 1.0f;
    float remoteGain = 1.0f;
    float phaseOffsetMs = 0.0f;
    bool monitorIncomingAudio = false;
    bool monitorTxAudio = false;
    bool metronomeEnabled = true;
    juce::String syncStateText = "Classic";
    juce::StringArray logLines;
  };

  NinjamClientService();
  ~NinjamClientService() override;

  void setCredentials(const juce::String& host, const juce::String& user, const juce::String& password);
  void connect();
  void disconnect();

  void sendCommand(const juce::String& text);
  void processAudioBlock(juce::AudioBuffer<float>& buffer, const TransportState& transportState);
  void setSampleRate(int sampleRateHz);

  void setMonitorIncomingAudio(bool enabled);
  bool getMonitorIncomingAudio() const;
  void setMonitorTxAudio(bool enabled);
  bool getMonitorTxAudio() const;
  void setMetronomeEnabled(bool enabled);
  bool getMetronomeEnabled() const;

  void setLocalGain(float value);
  void setRemoteGain(float value);
  void setPhaseOffsetMs(float ms);

  float getLocalGain() const;
  float getRemoteGain() const;
  float getPhaseOffsetMs() const;

  Snapshot getSnapshot() const;

  void addLogLine(const juce::String& message);

private:
  void timerCallback() override;
  void ensureAllRemoteChannelsSubscribed();
  void warnIfDuplicateUsername();
  void updateMetersFromBuffer(const juce::AudioBuffer<float>& buffer);
  void refreshStatusFromCore();
  void configureCorePaths();

  void appendLogLineUnlocked(const juce::String& line);
  void handleChatMessage(const char** parms, int nparms);
  static void chatMessageCallback(void* userData, NJClient* inst, const char** parms, int nparms);
  int onLicenseAgreement(const char* licenseText);
  static int licenseAgreementCallback(void* userData, const char* licenseText);

  static juce::String statusCodeToText(int statusCode);
  void applySessionChannelModeToCore();
  void renderMetronome(float** outBuffers, int numChannels, int blockSize,
                       double bpm, int bpi, double phaseBeats, int sampleRateHz);

  static void ringCopy(juce::AudioBuffer<float>& dst, int dstPos,
                       const juce::AudioBuffer<float>& src, int srcPos,
                       int numChannels, int numSamples, int ringLen);
  static float clampMeter(float value);

  mutable juce::CriticalSection lock;
  Snapshot state;
  NJClient client;
  int sampleRate = 48000;
  int lastStatusCode = NJClient::NJC_STATUS_DISCONNECTED;
  double lastHostPpq = 0.0;
  bool lastHostPpqValid = false;
  double lastHostBpm = 0.0;
  bool lastHostBpmValid = false;
  bool hostLockedActive = false;
  int lastSyncMode = -1;
  bool savedLocalMonitorMute = false;
  bool haveSavedLocalMonitorMute = false;
  bool duplicateNameWarned = false;
  bool forceSeekPending = false;
  int lastServerBpm = 0;
  int lastServerBpi = 0;
  bool hostPhaseAccumulatorValid = false;
  double hostPhaseAccumulatorBeats = 0.0;
  double lastHostPhaseBeat = 0.0;

  juce::AudioBuffer<float> inputScratch;
  juce::AudioBuffer<float> txMonitorScratch;
  juce::AudioBuffer<float> outputScratch;

  juce::AudioBuffer<float> phaseRingBuffer;
  int phaseRingIntervalLen = 0;

  juce::AudioBuffer<float> inputRingBuffer;
  int inputRingIntervalLen = 0;
  double phaseRingBeatOffset = 0.0;
  bool phaseRingOffsetValid = false;
  int metronomeClickState = 0;
  bool metronomeClickAccent = false;
};
