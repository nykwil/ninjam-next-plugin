#include "PluginEditor.h"

namespace
{
constexpr int kPadding = 10;
constexpr int kRowHeight = 24;
constexpr int kStripHeight = 32;
constexpr int kVuBarWidth = 140;
constexpr int kButtonWidth = 28;
constexpr int kUserNameWidth = 120;
constexpr int kChannelNameWidth = 60;
constexpr float kGainMax = 2.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// VUGainBar
// ─────────────────────────────────────────────────────────────────────────────

VUGainBar::VUGainBar()
{
  setRepaintsOnMouseActivity(false);
}

void VUGainBar::setPeak(float p)
{
  peak = juce::jlimit(0.0f, 1.0f, p);
}

void VUGainBar::setGain(float g)
{
  gain = juce::jlimit(0.0f, kGainMax, g);
}

float VUGainBar::xToGain(float x) const
{
  const float w = static_cast<float>(getWidth());
  if (w <= 0.0f) return 1.0f;
  return juce::jlimit(0.0f, kGainMax, (x / w) * kGainMax);
}

void VUGainBar::paint(juce::Graphics& g)
{
  const auto bounds = getLocalBounds().toFloat();

  // Background
  g.setColour(juce::Colour::fromRGB(30, 32, 36));
  g.fillRoundedRectangle(bounds, 3.0f);

  // VU fill
  const float vuW = peak * bounds.getWidth();
  if (vuW > 0.5f)
  {
    juce::Colour vuColour;
    if (peak > 0.9f)
      vuColour = juce::Colours::red;
    else if (peak > 0.6f)
      vuColour = juce::Colours::yellow.darker(0.2f);
    else
      vuColour = juce::Colours::limegreen;

    g.setColour(vuColour.withAlpha(0.5f));
    g.fillRoundedRectangle(bounds.withWidth(vuW), 3.0f);
  }

  // Gain marker (vertical line)
  const float gainX = (gain / kGainMax) * bounds.getWidth();
  g.setColour(juce::Colours::white);
  g.drawLine(gainX, 1.0f, gainX, bounds.getHeight() - 1.0f, 2.0f);

  // Gain text
  g.setColour(juce::Colours::white.withAlpha(0.8f));
  g.setFont(juce::FontOptions(10.0f));
  const auto dbVal = (gain > 0.001f) ? 20.0f * std::log10(gain) : -60.0f;
  juce::String gainText;
  if (dbVal <= -59.0f)
    gainText = "-inf";
  else
    gainText = juce::String(dbVal, 1) + "dB";
  g.drawText(gainText, bounds.reduced(4.0f, 0.0f), juce::Justification::centredRight, false);
}

void VUGainBar::mouseDown(const juce::MouseEvent& e)
{
  gain = xToGain(static_cast<float>(e.x));
  if (onGainChanged) onGainChanged(gain);
  repaint();
}

void VUGainBar::mouseDrag(const juce::MouseEvent& e)
{
  gain = xToGain(static_cast<float>(e.x));
  if (onGainChanged) onGainChanged(gain);
  repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// UserStripComponent
// ─────────────────────────────────────────────────────────────────────────────

UserStripComponent::UserStripComponent(NinjamNextAudioProcessor& proc,
                                       const NinjamClientService::RemoteUser& user)
  : processor(proc), userIdx(user.userIndex), userName(user.name)
{
  nameLabel.setText(userName, juce::dontSendNotification);
  nameLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
  nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(nameLabel);

  rebuildChannels(user);
}

void UserStripComponent::rebuildChannels(const NinjamClientService::RemoteUser& user)
{
  channelStrips.clear();
  userIdx = user.userIndex;

  for (size_t i = 0; i < user.channels.size(); ++i)
  {
    const auto& ch = user.channels[i];
    auto* strip = channelStrips.add(new ChannelStrip());
    strip->channelIndex = ch.channelIndex;

    strip->nameLabel.setText(ch.name, juce::dontSendNotification);
    strip->nameLabel.setFont(juce::FontOptions(11.0f));
    strip->nameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(strip->nameLabel);

    strip->vuGain.setPeak(ch.peak);
    strip->vuGain.setGain(ch.volume);
    strip->vuGain.onGainChanged = [this, strip](float vol)
    {
      processor.setUserChannelVolume(userIdx, strip->channelIndex, vol);
    };
    addAndMakeVisible(strip->vuGain);

    strip->muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    strip->muteButton.setToggleable(true);
    strip->muteButton.setToggleState(ch.muted, juce::dontSendNotification);
    strip->muteButton.setClickingTogglesState(true);
    strip->muteButton.onClick = [this, strip]
    {
      processor.setUserChannelMute(userIdx, strip->channelIndex, strip->muteButton.getToggleState());
    };
    addAndMakeVisible(strip->muteButton);

    strip->soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow.darker(0.3f));
    strip->soloButton.setToggleable(true);
    strip->soloButton.setToggleState(ch.solo, juce::dontSendNotification);
    strip->soloButton.setClickingTogglesState(true);
    strip->soloButton.onClick = [this, strip]
    {
      processor.setUserChannelSolo(userIdx, strip->channelIndex, strip->soloButton.getToggleState());
    };
    addAndMakeVisible(strip->soloButton);
  }

  resized();
}

void UserStripComponent::update(const NinjamClientService::RemoteUser& user)
{
  userIdx = user.userIndex;
  userName = user.name;
  nameLabel.setText(userName, juce::dontSendNotification);

  if (static_cast<int>(user.channels.size()) != channelStrips.size())
  {
    rebuildChannels(user);
    return;
  }

  for (size_t i = 0; i < user.channels.size(); ++i)
  {
    auto* strip = channelStrips[static_cast<int>(i)];
    const auto& ch = user.channels[i];
    strip->channelIndex = ch.channelIndex;
    strip->nameLabel.setText(ch.name, juce::dontSendNotification);
    strip->vuGain.setPeak(ch.peak);
    strip->vuGain.setGain(ch.volume);
    strip->muteButton.setToggleState(ch.muted, juce::dontSendNotification);
    strip->soloButton.setToggleState(ch.solo, juce::dontSendNotification);
  }

  repaint();
}

void UserStripComponent::paint(juce::Graphics& g)
{
  g.setColour(juce::Colour::fromRGB(40, 44, 52));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
}

void UserStripComponent::resized()
{
  int x = 4;
  nameLabel.setBounds(x, 0, kUserNameWidth - 8, kStripHeight);
  x = kUserNameWidth;

  for (int i = 0; i < channelStrips.size(); ++i)
  {
    auto* strip = channelStrips[i];
    strip->nameLabel.setBounds(x, 0, kChannelNameWidth, kStripHeight);
    x += kChannelNameWidth;
    strip->vuGain.setBounds(x, 4, kVuBarWidth, kStripHeight - 8);
    x += kVuBarWidth + 4;
    strip->muteButton.setBounds(x, 4, kButtonWidth, kStripHeight - 8);
    x += kButtonWidth + 2;
    strip->soloButton.setBounds(x, 4, kButtonWidth, kStripHeight - 8);
    x += kButtonWidth + 8;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// SendStripComponent
// ─────────────────────────────────────────────────────────────────────────────

SendStripComponent::SendStripComponent(NinjamNextAudioProcessor& proc)
  : processor(proc)
{
  nameLabel.setText("Send", juce::dontSendNotification);
  nameLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
  nameLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
  addAndMakeVisible(nameLabel);

  vuGain.setGain(processor.getClientService().getLocalGain());
  vuGain.onGainChanged = [this](float vol)
  {
    processor.getClientService().setLocalGain(vol);
  };
  addAndMakeVisible(vuGain);

  // Mix: hear your input blended with remote audio (Monitor RX)
  mixButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::cyan.darker(0.3f));
  mixButton.setToggleable(true);
  mixButton.setClickingTogglesState(true);
  mixButton.setToggleState(processor.getMonitorIncomingAudio(), juce::dontSendNotification);
  mixButton.onClick = [this]
  {
    // Mix and Solo are mutually exclusive
    if (mixButton.getToggleState())
      soloButton.setToggleState(false, juce::dontSendNotification);
    processor.setMonitorTxAudio(false);
    processor.setMonitorIncomingAudio(mixButton.getToggleState());
  };
  addAndMakeVisible(mixButton);

  // Solo: hear only your input, replacing remote audio (Monitor TX)
  soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
  soloButton.setToggleable(true);
  soloButton.setClickingTogglesState(true);
  soloButton.setToggleState(processor.getMonitorTxAudio(), juce::dontSendNotification);
  soloButton.onClick = [this]
  {
    // Solo and Mix are mutually exclusive
    if (soloButton.getToggleState())
      mixButton.setToggleState(false, juce::dontSendNotification);
    processor.setMonitorIncomingAudio(false);
    processor.setMonitorTxAudio(soloButton.getToggleState());
  };
  addAndMakeVisible(soloButton);
}

void SendStripComponent::update(float sendPeak, float localGain, bool mixing, bool soloing)
{
  vuGain.setPeak(sendPeak);
  vuGain.setGain(localGain);
  mixButton.setToggleState(mixing, juce::dontSendNotification);
  soloButton.setToggleState(soloing, juce::dontSendNotification);
  repaint();
}

void SendStripComponent::paint(juce::Graphics& g)
{
  g.setColour(juce::Colour::fromRGB(30, 50, 55));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

  g.setColour(juce::Colours::cyan.withAlpha(0.3f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void SendStripComponent::resized()
{
  int x = 4;
  nameLabel.setBounds(x, 0, kUserNameWidth - 8, kStripHeight);
  x = kUserNameWidth;
  vuGain.setBounds(x, 4, kVuBarWidth + kChannelNameWidth, kStripHeight - 8);
  x += kVuBarWidth + kChannelNameWidth + 4;
  mixButton.setBounds(x, 4, 40, kStripHeight - 8);
  x += 44;
  soloButton.setBounds(x, 4, 44, kStripHeight - 8);
}

// ─────────────────────────────────────────────────────────────────────────────
// MixerContentComponent
// ─────────────────────────────────────────────────────────────────────────────

MixerContentComponent::MixerContentComponent(NinjamNextAudioProcessor& proc)
  : processor(proc), sendStrip(proc)
{
  addAndMakeVisible(sendStrip);
}

void MixerContentComponent::updateFromSnapshot(const NinjamClientService::Snapshot& snapshot)
{
  sendStrip.update(snapshot.sendMeter, snapshot.localGain, snapshot.monitorIncomingAudio, snapshot.monitorTxAudio);

  const auto& users = snapshot.remoteUsers;
  bool needsLayout = false;

  // Remove strips for users no longer present
  for (int i = userStrips.size() - 1; i >= 0; --i)
  {
    bool found = false;
    for (const auto& u : users)
    {
      if (userStrips[i]->getUserName() == u.name)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      userStrips.remove(i);
      needsLayout = true;
    }
  }

  // Update existing or add new strips
  for (const auto& user : users)
  {
    UserStripComponent* existing = nullptr;

    for (auto* strip : userStrips)
    {
      if (strip->getUserName() == user.name)
      {
        existing = strip;
        break;
      }
    }

    if (existing != nullptr)
    {
      existing->update(user);
    }
    else
    {
      auto* newStrip = userStrips.add(new UserStripComponent(processor, user));
      addAndMakeVisible(newStrip);
      needsLayout = true;
    }
  }

  if (needsLayout)
  {
    const int totalHeight = (1 + userStrips.size()) * (kStripHeight + 4) + 4;
    setSize(getWidth(), juce::jmax(10, totalHeight));
    resized();
  }
  else
  {
    repaint();
  }
}

void MixerContentComponent::resized()
{
  int y = 2;
  sendStrip.setBounds(0, y, getWidth(), kStripHeight);
  y += kStripHeight + 4;

  for (auto* strip : userStrips)
  {
    strip->setBounds(0, y, getWidth(), kStripHeight);
    y += kStripHeight + 4;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// NinjamNextAudioProcessorEditor
// ─────────────────────────────────────────────────────────────────────────────

NinjamNextAudioProcessorEditor::NinjamNextAudioProcessorEditor(NinjamNextAudioProcessor& p)
  : AudioProcessorEditor(&p),
    processor(p),
    mixerContent(p)
{
  setSize(920, 680);

  titleLabel.setText("NinjamNext", juce::dontSendNotification);
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

  metronomeToggle.setButtonText("Metronome");
  metronomeToggle.onClick = [this] { metronomeChanged(); };
  addAndMakeVisible(metronomeToggle);

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

  mixerViewport.setViewedComponent(&mixerContent, false);
  mixerViewport.setScrollBarsShown(true, false);
  addAndMakeVisible(mixerViewport);

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
  phaseOffsetSlider.setValue(snapshot.phaseOffsetMs, juce::dontSendNotification);

  ignoreToggleCallback = true;
  metronomeToggle.setToggleState(snapshot.metronomeEnabled, juce::dontSendNotification);
  ignoreToggleCallback = false;

  refreshFromService();
  startTimerHz(10);
}

NinjamNextAudioProcessorEditor::~NinjamNextAudioProcessorEditor()
{
  stopTimer();
}

void NinjamNextAudioProcessorEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour::fromRGB(24, 26, 30));

  g.setColour(juce::Colour::fromRGB(50, 55, 64));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 8.0f, 1.0f);
}

void NinjamNextAudioProcessorEditor::resized()
{
  auto area = getLocalBounds().reduced(kPadding);

  // Title
  titleLabel.setBounds(area.removeFromTop(28));
  area.removeFromTop(4);

  // Connection row 1: host/user/pass
  auto row1 = area.removeFromTop(kRowHeight);
  hostLabel.setBounds(row1.removeFromLeft(46));
  hostEditor.setBounds(row1.removeFromLeft(260));
  row1.removeFromLeft(10);
  userLabel.setBounds(row1.removeFromLeft(36));
  userEditor.setBounds(row1.removeFromLeft(200));
  row1.removeFromLeft(10);
  passwordLabel.setBounds(row1.removeFromLeft(72));
  passwordEditor.setBounds(row1.removeFromLeft(180));

  area.removeFromTop(6);

  // Connection row 2: connect/disconnect + status
  auto row2 = area.removeFromTop(kRowHeight);
  connectButton.setBounds(row2.removeFromLeft(110));
  row2.removeFromLeft(8);
  disconnectButton.setBounds(row2.removeFromLeft(110));
  row2.removeFromLeft(16);
  statusLabel.setBounds(row2);

  area.removeFromTop(6);

  // Info row: BPM + BPI + Interval + Metronome
  auto row3 = area.removeFromTop(kRowHeight);
  bpmLabel.setBounds(row3.removeFromLeft(260));
  bpiLabel.setBounds(row3.removeFromLeft(120));
  intervalLabel.setBounds(row3.removeFromLeft(200));
  metronomeToggle.setBounds(row3.removeFromLeft(130));

  area.removeFromTop(4);

  // Phase offset row
  auto row4 = area.removeFromTop(kRowHeight);
  phaseOffsetLabel.setBounds(row4.removeFromLeft(120));
  phaseOffsetSlider.setBounds(row4.removeFromLeft(300));

  area.removeFromTop(8);

  // Mixer panel (takes a portion of remaining space)
  const int mixerHeight = juce::jmax(72, area.getHeight() / 3);
  auto mixerArea = area.removeFromTop(mixerHeight);
  mixerViewport.setBounds(mixerArea);
  mixerContent.setSize(mixerArea.getWidth() - 16, mixerContent.getHeight());

  area.removeFromTop(8);

  // Log + command area
  auto logArea = area;
  auto commandArea = logArea.removeFromBottom(32);
  commandEditor.setBounds(commandArea.removeFromLeft(commandArea.getWidth() - 90));
  commandArea.removeFromLeft(8);
  sendButton.setBounds(commandArea);

  logArea.removeFromBottom(8);
  logEditor.setBounds(logArea);
}

void NinjamNextAudioProcessorEditor::timerCallback()
{
  refreshFromService();
}

void NinjamNextAudioProcessorEditor::refreshFromService()
{
  const auto snapshot = processor.getClientService().getSnapshot();

  statusLabel.setText("Status: " + snapshot.statusText + " | Sync: " + snapshot.syncStateText, juce::dontSendNotification);

  // Dual BPM display
  juce::String bpmText;
  bool bpmMismatch = false;
  if (snapshot.hostBpmValid && snapshot.hostBpm != snapshot.serverBpm)
  {
    bpmText = "BPM: " + juce::String(snapshot.hostBpm) + " (Server: " + juce::String(snapshot.serverBpm) + ")";
    bpmMismatch = true;
  }
  else
  {
    bpmText = "BPM: " + juce::String(snapshot.bpm) + " (Server: " + juce::String(snapshot.serverBpm) + ")";
  }
  bpmLabel.setText(bpmText, juce::dontSendNotification);
  bpmLabel.setColour(juce::Label::textColourId, bpmMismatch ? juce::Colours::red : juce::Colours::white);

  bpiLabel.setText("BPI: " + juce::String(snapshot.bpi), juce::dontSendNotification);
  intervalLabel.setText("Interval: " + juce::String(snapshot.intervalProgress * 100.0f, 1) + "%", juce::dontSendNotification);

  if (metronomeToggle.getToggleState() != snapshot.metronomeEnabled)
  {
    ignoreToggleCallback = true;
    metronomeToggle.setToggleState(snapshot.metronomeEnabled, juce::dontSendNotification);
    ignoreToggleCallback = false;
  }

  // Update mixer panel
  mixerContent.updateFromSnapshot(snapshot);

  const auto logText = snapshot.logLines.joinIntoString("\n");
  if (logText != lastRenderedLog)
  {
    lastRenderedLog = logText;
    logEditor.setText(logText, false);
    logEditor.moveCaretToEnd();
  }
}

void NinjamNextAudioProcessorEditor::connectPressed()
{
  processor.connectToServer(hostEditor.getText(), userEditor.getText(), passwordEditor.getText());
}

void NinjamNextAudioProcessorEditor::disconnectPressed()
{
  processor.disconnectFromServer();
}

void NinjamNextAudioProcessorEditor::sendCommandPressed()
{
  const auto text = commandEditor.getText();
  if (text.trim().isEmpty())
    return;

  processor.sendUserCommand(text);
  commandEditor.clear();
}

void NinjamNextAudioProcessorEditor::metronomeChanged()
{
  if (ignoreToggleCallback)
    return;

  processor.setMetronomeEnabled(metronomeToggle.getToggleState());
}
