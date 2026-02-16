#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NinjamVST3AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
  explicit NinjamVST3AudioProcessorEditor(NinjamVST3AudioProcessor&);
  ~NinjamVST3AudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;

  void refreshFromService();
  void connectPressed();
  void disconnectPressed();
  void sendCommandPressed();
  void monitorIncomingChanged();
  void monitorTxChanged();
  void metronomeChanged();

  NinjamVST3AudioProcessor& processor;

  juce::Label titleLabel;

  juce::Label hostLabel;
  juce::TextEditor hostEditor;

  juce::Label userLabel;
  juce::TextEditor userEditor;

  juce::Label passwordLabel;
  juce::TextEditor passwordEditor;

  juce::TextButton connectButton;
  juce::TextButton disconnectButton;

  juce::Label statusLabel;
  juce::Label bpmLabel;
  juce::Label bpiLabel;
  juce::Label intervalLabel;

  juce::Label syncHintLabel;
  juce::ToggleButton monitorIncomingToggle;
  juce::ToggleButton monitorTxToggle;
  juce::ToggleButton metronomeToggle;

  juce::Label localGainLabel;
  juce::Slider localGainSlider;

  juce::Label remoteGainLabel;
  juce::Slider remoteGainSlider;

  juce::Label phaseOffsetLabel;
  juce::Slider phaseOffsetSlider;

  juce::TextEditor logEditor;
  juce::TextEditor commandEditor;
  juce::TextButton sendButton;

  juce::String lastRenderedLog;
  bool ignoreMonitorToggleCallback = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NinjamVST3AudioProcessorEditor)
};
