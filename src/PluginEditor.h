#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Combined VU meter + gain slider control.
// Paints VU fill as background, gain marker as vertical line overlay.
// Mouse drag adjusts gain (-inf dB to +10 dB).
class VUGainBar : public juce::Component
{
public:
  VUGainBar();

  void setPeak(float p);
  void setGain(float g);
  float getGain() const { return gain; }

  std::function<void(float)> onGainChanged;

  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;

private:
  float peak = 0.0f;
  float gain = 1.0f;

  float xToGain(float x) const;
  float gainToX(float g) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUGainBar)
};

// Strip for a remote user (one row per user, channels inline).
class UserStripComponent : public juce::Component
{
public:
  UserStripComponent(NinjamNextAudioProcessor& proc,
                     const NinjamClientService::RemoteUser& user);

  void update(const NinjamClientService::RemoteUser& user);
  void paint(juce::Graphics& g) override;
  void resized() override;

  int getUserIndex() const { return userIdx; }

private:
  NinjamNextAudioProcessor& processor;
  int userIdx = 0;
  juce::String userName;

  juce::Label nameLabel;

  struct ChannelStrip
  {
    int channelIndex = 0;
    VUGainBar vuGain;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
  };

  juce::OwnedArray<ChannelStrip> channelStrips;

  void rebuildChannels(const NinjamClientService::RemoteUser& user);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserStripComponent)
};

// Strip for local send channel (visually distinct from remote strips).
class SendStripComponent : public juce::Component
{
public:
  SendStripComponent(NinjamNextAudioProcessor& proc);

  void update(float sendPeak, float localGain, NinjamClientService::MonitorMode mode);
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  NinjamNextAudioProcessor& processor;

  juce::Label nameLabel;
  VUGainBar vuGain;
  juce::TextButton mixButton { "A" };
  juce::TextButton soloButton { "L" };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SendStripComponent)
};

// Container for send strip + remote user strips, hosted in a Viewport.
class MixerContentComponent : public juce::Component
{
public:
  MixerContentComponent(NinjamNextAudioProcessor& proc);

  void updateFromSnapshot(const NinjamClientService::Snapshot& snapshot);
  void resized() override;

private:
  NinjamNextAudioProcessor& processor;
  SendStripComponent sendStrip;
  juce::OwnedArray<UserStripComponent> userStrips;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerContentComponent)
};

struct ServerEntry
{
  juce::String name;
  juce::String host;
  juce::String port;
  int bpm = 0;
  int bpi = 0;
  int userCount = 0;
  int userLimit = 0;
  juce::String hostPort() const { return host + ":" + port; }
};

class ServerBrowserComponent : public juce::Component,
                               public juce::TableListBoxModel,
                               private juce::Timer
{
public:
  std::function<void(const juce::String&)> onServerSelected;

  ServerBrowserComponent();
  ~ServerBrowserComponent() override;

  void paint(juce::Graphics&) override;
  void resized() override;

  int getNumRows() override;
  void paintRowBackground(juce::Graphics&, int row, int width, int height, bool selected) override;
  void paintCell(juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
  void cellDoubleClicked(int row, int columnId, const juce::MouseEvent&) override;

  void onFetchComplete(const juce::String& json);

private:
  class FetchThread : public juce::Thread
  {
  public:
    explicit FetchThread(juce::WeakReference<ServerBrowserComponent> weakSelf);
    ~FetchThread() override;
    void run() override;
  private:
    juce::WeakReference<ServerBrowserComponent> weakSelf;
  };

  void timerCallback() override;
  void fetchServers();
  void selectCurrentRow();

  juce::TableListBox table { {}, this };
  juce::Label statusLabel;
  juce::TextButton refreshButton { "Refresh" };
  juce::TextButton selectButton { "Use Server" };

  juce::Array<ServerEntry> servers;
  std::unique_ptr<FetchThread> fetchThread;
  bool fetching = false;
  int ticksUntilRefresh = 30;

  JUCE_DECLARE_WEAK_REFERENCEABLE(ServerBrowserComponent)
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ServerBrowserComponent)
};

class NinjamNextAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
  explicit NinjamNextAudioProcessorEditor(NinjamNextAudioProcessor&);
  ~NinjamNextAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;

  void refreshFromService();
  void browsePressed();
  void connectTogglePressed();
  void licenseOkPressed();
  void sendCommandPressed();
  void phaseOffsetEdited();
  void metronomeChanged();
  void flashField(juce::TextEditor& editor);

  NinjamNextAudioProcessor& processor;

  juce::Label titleLabel;

  juce::Label hostLabel;
  juce::TextEditor hostEditor;
  juce::TextButton browseButton;

  juce::Label userLabel;
  juce::TextEditor userEditor;

  juce::Label passwordLabel;
  juce::TextEditor passwordEditor;

  juce::TextButton connectToggleButton;

  juce::Label statusLabel;
  juce::Label bpmLabel;
  juce::Label bpiLabel;
  juce::Label intervalLabel;

  juce::ToggleButton metronomeToggle;

  juce::Label phaseOffsetLabel;
  juce::TextEditor phaseOffsetEditor;

  juce::Viewport mixerViewport;
  MixerContentComponent mixerContent;

  juce::TextEditor logEditor;
  juce::TextEditor commandEditor;
  juce::TextButton sendButton;
  juce::TextButton licenseOkButton;

  juce::String lastRenderedLog;
  bool ignoreToggleCallback = false;
  bool wasConnected = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NinjamNextAudioProcessorEditor)
};
