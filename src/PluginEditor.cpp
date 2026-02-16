#include "PluginEditor.h"

namespace
{
constexpr int kPadding = 10;
constexpr int kRowHeight = 24;
}

NinjamVST3AudioProcessorEditor::NinjamVST3AudioProcessorEditor(NinjamVST3AudioProcessor& p)
  : AudioProcessorEditor(&p),
    processor(p)
{
  setSize(920, 640);

  titleLabel.setText("NINJAM VST3", juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
  addAndMakeVisible(titleLabel);

  hostLabel.setText("Host", juce::dontSendNotification);
  addAndMakeVisible(hostLabel);
  addAndMakeVisible(hostEditor);

  userLabel.setText("User", juce::dontSendNotification);
  addAndMakeVisible(userLabel);
  addAndMakeVisible(userEditor);

  passwordLabel.setText("Password", juce::dontSendNotification);
  addAndMakeVisible(passwordLabel);
  passwordEditor.setPasswordCharacter('*');
  addAndMakeVisible(passwordEditor);

  connectButton.setButtonText("Connect");
  connectButton.onClick = [this] { connectPressed(); };
  addAndMakeVisible(connectButton);

  disconnectButton.setButtonText("Disconnect");
  disconnectButton.onClick = [this] { disconnectPressed(); };
  addAndMakeVisible(disconnectButton);

  statusLabel.setText("Status: Disconnected", juce::dontSendNotification);
  addAndMakeVisible(statusLabel);

  bpmLabel.setText("BPM: --", juce::dontSendNotification);
  addAndMakeVisible(bpmLabel);

  bpiLabel.setText("BPI: --", juce::dontSendNotification);
  addAndMakeVisible(bpiLabel);

  intervalLabel.setText("Interval: --", juce::dontSendNotification);
  addAndMakeVisible(intervalLabel);

  syncHintLabel.setText("Session sync is automatic; classic fallback is used when host transport is unavailable.", juce::dontSendNotification);
  syncHintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(syncHintLabel);

  monitorIncomingToggle.setButtonText("Monitor RX (incoming network audio)");
  monitorIncomingToggle.onClick = [this] { monitorIncomingChanged(); };
  addAndMakeVisible(monitorIncomingToggle);

  monitorTxToggle.setButtonText("Monitor TX (local send)");
  monitorTxToggle.onClick = [this] { monitorTxChanged(); };
  addAndMakeVisible(monitorTxToggle);

  metronomeToggle.setButtonText("Metronome");
  metronomeToggle.onClick = [this] { metronomeChanged(); };
  addAndMakeVisible(metronomeToggle);

  localGainLabel.setText("Local Gain", juce::dontSendNotification);
  addAndMakeVisible(localGainLabel);
  localGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  localGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
  localGainSlider.setRange(0.0, 2.0, 0.01);
  localGainSlider.onValueChange = [this]
  {
    processor.getClientService().setLocalGain(static_cast<float>(localGainSlider.getValue()));
  };
  addAndMakeVisible(localGainSlider);

  remoteGainLabel.setText("Remote Gain", juce::dontSendNotification);
  addAndMakeVisible(remoteGainLabel);
  remoteGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  remoteGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
  remoteGainSlider.setRange(0.0, 2.0, 0.01);
  remoteGainSlider.onValueChange = [this]
  {
    processor.getClientService().setRemoteGain(static_cast<float>(remoteGainSlider.getValue()));
  };
  addAndMakeVisible(remoteGainSlider);

  phaseOffsetLabel.setText("Phase Offset (ms)", juce::dontSendNotification);
  addAndMakeVisible(phaseOffsetLabel);
  phaseOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  phaseOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
  phaseOffsetSlider.setRange(-500.0, 500.0, 1.0);
  phaseOffsetSlider.onValueChange = [this]
  {
    processor.getClientService().setPhaseOffsetMs(static_cast<float>(phaseOffsetSlider.getValue()));
  };
  addAndMakeVisible(phaseOffsetSlider);

  logEditor.setMultiLine(true);
  logEditor.setReadOnly(true);
  logEditor.setScrollbarsShown(true);
  logEditor.setCaretVisible(false);
  addAndMakeVisible(logEditor);

  commandEditor.setMultiLine(false);
  commandEditor.setReturnKeyStartsNewLine(false);
  commandEditor.setTextToShowWhenEmpty("Enter /bpm 120, /bpi 16, !vote bpm 120, or regular message", juce::Colours::grey);
  commandEditor.onReturnKey = [this] { sendCommandPressed(); };
  addAndMakeVisible(commandEditor);

  sendButton.setButtonText("Send");
  sendButton.onClick = [this] { sendCommandPressed(); };
  addAndMakeVisible(sendButton);

  const auto snapshot = processor.getClientService().getSnapshot();
  hostEditor.setText(snapshot.host, juce::dontSendNotification);
  userEditor.setText(snapshot.user, juce::dontSendNotification);
  passwordEditor.setText(snapshot.password, juce::dontSendNotification);
  localGainSlider.setValue(snapshot.localGain, juce::dontSendNotification);
  remoteGainSlider.setValue(snapshot.remoteGain, juce::dontSendNotification);
  phaseOffsetSlider.setValue(snapshot.phaseOffsetMs, juce::dontSendNotification);

  ignoreMonitorToggleCallback = true;
  monitorIncomingToggle.setToggleState(snapshot.monitorIncomingAudio, juce::dontSendNotification);
  monitorTxToggle.setToggleState(snapshot.monitorTxAudio, juce::dontSendNotification);
  metronomeToggle.setToggleState(snapshot.metronomeEnabled, juce::dontSendNotification);
  ignoreMonitorToggleCallback = false;

  refreshFromService();
  startTimerHz(10);
}

NinjamVST3AudioProcessorEditor::~NinjamVST3AudioProcessorEditor()
{
  stopTimer();
}

void NinjamVST3AudioProcessorEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour::fromRGB(24, 26, 30));

  g.setColour(juce::Colour::fromRGB(50, 55, 64));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 8.0f, 1.0f);

  g.setColour(juce::Colour::fromRGB(35, 38, 44));
  g.fillRoundedRectangle(juce::Rectangle<float>(8.0f, 44.0f, static_cast<float>(getWidth() - 16), 272.0f), 6.0f);
  g.fillRoundedRectangle(juce::Rectangle<float>(8.0f, 322.0f, static_cast<float>(getWidth() - 16), static_cast<float>(getHeight() - 330)), 6.0f);
}

void NinjamVST3AudioProcessorEditor::resized()
{
  auto area = getLocalBounds().reduced(kPadding);

  titleLabel.setBounds(area.removeFromTop(28));
  area.removeFromTop(6);

  auto connection = area.removeFromTop(270);
  auto row1 = connection.removeFromTop(kRowHeight);
  hostLabel.setBounds(row1.removeFromLeft(46));
  hostEditor.setBounds(row1.removeFromLeft(260));
  row1.removeFromLeft(10);
  userLabel.setBounds(row1.removeFromLeft(36));
  userEditor.setBounds(row1.removeFromLeft(200));
  row1.removeFromLeft(10);
  passwordLabel.setBounds(row1.removeFromLeft(72));
  passwordEditor.setBounds(row1.removeFromLeft(180));

  connection.removeFromTop(8);
  auto row2 = connection.removeFromTop(kRowHeight);
  connectButton.setBounds(row2.removeFromLeft(110));
  row2.removeFromLeft(8);
  disconnectButton.setBounds(row2.removeFromLeft(110));
  row2.removeFromLeft(16);
  statusLabel.setBounds(row2);

  connection.removeFromTop(8);
  auto row3 = connection.removeFromTop(kRowHeight);
  bpmLabel.setBounds(row3.removeFromLeft(180));
  bpiLabel.setBounds(row3.removeFromLeft(180));
  intervalLabel.setBounds(row3.removeFromLeft(260));

  connection.removeFromTop(8);
  auto row4 = connection.removeFromTop(kRowHeight);
  syncHintLabel.setBounds(row4);

  connection.removeFromTop(8);
  auto row5 = connection.removeFromTop(kRowHeight);
  monitorIncomingToggle.setBounds(row5.removeFromLeft(250));
  monitorTxToggle.setBounds(row5.removeFromLeft(220));
  metronomeToggle.setBounds(row5.removeFromLeft(130));

  connection.removeFromTop(8);
  auto row6 = connection.removeFromTop(kRowHeight);
  localGainLabel.setBounds(row6.removeFromLeft(80));
  localGainSlider.setBounds(row6.removeFromLeft(290));
  row6.removeFromLeft(24);
  remoteGainLabel.setBounds(row6.removeFromLeft(90));
  remoteGainSlider.setBounds(row6.removeFromLeft(290));

  connection.removeFromTop(8);
  auto row7 = connection.removeFromTop(kRowHeight);
  phaseOffsetLabel.setBounds(row7.removeFromLeft(120));
  phaseOffsetSlider.setBounds(row7.removeFromLeft(300));

  area.removeFromTop(8);
  auto logArea = area;
  auto commandArea = logArea.removeFromBottom(32);
  commandEditor.setBounds(commandArea.removeFromLeft(commandArea.getWidth() - 90));
  commandArea.removeFromLeft(8);
  sendButton.setBounds(commandArea);

  logArea.removeFromBottom(8);
  logEditor.setBounds(logArea);
}

void NinjamVST3AudioProcessorEditor::timerCallback()
{
  refreshFromService();
}

void NinjamVST3AudioProcessorEditor::refreshFromService()
{
  const auto snapshot = processor.getClientService().getSnapshot();

  statusLabel.setText("Status: " + snapshot.statusText + " | Sync: " + snapshot.syncStateText, juce::dontSendNotification);
  bpmLabel.setText("BPM: " + juce::String(snapshot.bpm), juce::dontSendNotification);
  bpiLabel.setText("BPI: " + juce::String(snapshot.bpi), juce::dontSendNotification);
  intervalLabel.setText("Interval: " + juce::String(snapshot.intervalProgress * 100.0f, 1) + "%", juce::dontSendNotification);

  if (monitorIncomingToggle.getToggleState() != snapshot.monitorIncomingAudio ||
      monitorTxToggle.getToggleState() != snapshot.monitorTxAudio ||
      metronomeToggle.getToggleState() != snapshot.metronomeEnabled)
  {
    ignoreMonitorToggleCallback = true;
    monitorIncomingToggle.setToggleState(snapshot.monitorIncomingAudio, juce::dontSendNotification);
    monitorTxToggle.setToggleState(snapshot.monitorTxAudio, juce::dontSendNotification);
    metronomeToggle.setToggleState(snapshot.metronomeEnabled, juce::dontSendNotification);
    ignoreMonitorToggleCallback = false;
  }

  const auto logText = snapshot.logLines.joinIntoString("\n");
  if (logText != lastRenderedLog)
  {
    lastRenderedLog = logText;
    logEditor.setText(logText, false);
    logEditor.moveCaretToEnd();
  }
}

void NinjamVST3AudioProcessorEditor::connectPressed()
{
  processor.connectToServer(hostEditor.getText(), userEditor.getText(), passwordEditor.getText());
}

void NinjamVST3AudioProcessorEditor::disconnectPressed()
{
  processor.disconnectFromServer();
}

void NinjamVST3AudioProcessorEditor::sendCommandPressed()
{
  const auto text = commandEditor.getText();
  if (text.trim().isEmpty())
  {
    return;
  }

  processor.sendUserCommand(text);
  commandEditor.clear();
}

void NinjamVST3AudioProcessorEditor::monitorIncomingChanged()
{
  if (ignoreMonitorToggleCallback)
  {
    return;
  }

  processor.setMonitorIncomingAudio(monitorIncomingToggle.getToggleState());
}

void NinjamVST3AudioProcessorEditor::monitorTxChanged()
{
  if (ignoreMonitorToggleCallback)
  {
    return;
  }

  processor.setMonitorTxAudio(monitorTxToggle.getToggleState());
}

void NinjamVST3AudioProcessorEditor::metronomeChanged()
{
  if (ignoreMonitorToggleCallback)
  {
    return;
  }

  processor.setMetronomeEnabled(metronomeToggle.getToggleState());
}
