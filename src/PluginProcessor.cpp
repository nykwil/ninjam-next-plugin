#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

NinjamVST3AudioProcessor::NinjamVST3AudioProcessor()
  : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
  initialiseSettings();
  loadCredentialsFromSettings();
}

NinjamVST3AudioProcessor::~NinjamVST3AudioProcessor()
{
  clientService.disconnect();
  appProperties.closeFiles();
}

void NinjamVST3AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  juce::ignoreUnused(samplesPerBlock);
  sampleRateHz = sampleRate > 1.0 ? sampleRate : 48000.0;
  lastHostTimeSeconds = -1.0;
  lastHostPpq = 0.0;
  lastHostPpqValid = false;
  lastHostWasPlaying = false;
  clientService.setSampleRate(juce::roundToInt(sampleRate));

  if (!autoConnectAttempted)
  {
    autoConnectAttempted = true;
    const auto snapshot = clientService.getSnapshot();
    if (!snapshot.connected && snapshot.host.isNotEmpty() && snapshot.user.isNotEmpty())
    {
      clientService.addLogLine("Auto-connecting using saved credentials");
      clientService.connect();
    }
  }
}

void NinjamVST3AudioProcessor::releaseResources()
{
  lastHostTimeSeconds = -1.0;
  lastHostPpq = 0.0;
  lastHostPpqValid = false;
  lastHostWasPlaying = false;
}

bool NinjamVST3AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  const auto input = layouts.getMainInputChannelSet();
  const auto output = layouts.getMainOutputChannelSet();
  return input == output && (output == juce::AudioChannelSet::stereo() || output == juce::AudioChannelSet::mono());
}

void NinjamVST3AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
  juce::ignoreUnused(midiMessages);

  const auto totalInputChannels = getTotalNumInputChannels();
  const auto totalOutputChannels = getTotalNumOutputChannels();

  for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
  {
    buffer.clear(channel, 0, buffer.getNumSamples());
  }

  const auto transportState = buildTransportState(buffer.getNumSamples());
  clientService.processAudioBlock(buffer, transportState);
}

NinjamClientService::TransportState NinjamVST3AudioProcessor::buildTransportState(int numSamples)
{
  NinjamClientService::TransportState state;

  juce::AudioPlayHead::CurrentPositionInfo positionInfo;
  auto* currentPlayHead = getPlayHead();
  const bool hasPosition = currentPlayHead != nullptr && currentPlayHead->getCurrentPosition(positionInfo);

  if (!hasPosition)
  {
    state.isPlaying = lastHostWasPlaying;
    state.isSeek = false;
    if (lastHostTimeSeconds >= 0.0)
    {
      state.hostTimeSeconds = state.isPlaying ? (lastHostTimeSeconds + static_cast<double>(numSamples) / sampleRateHz)
                                              : lastHostTimeSeconds;
      lastHostTimeSeconds = state.hostTimeSeconds;
    }
    else
    {
      state.hostTimeSeconds = -1.0;
    }
    state.hostBpm = 0.0;
    state.hostPpqPosition = 0.0;
    state.hostBpmValid = false;
    state.hostPpqValid = false;
    return state;
  }

  state.isPlaying = positionInfo.isPlaying || positionInfo.isRecording;
  state.hostBpm = positionInfo.bpm;
  state.hostPpqPosition = positionInfo.ppqPosition;
  state.hostBpmValid = std::isfinite(state.hostBpm) && state.hostBpm > 1.0;
  state.hostPpqValid = std::isfinite(state.hostPpqPosition);
  if (state.hostPpqValid && state.hostBpmValid)
  {
    state.hostTimeSeconds = state.hostPpqPosition * 60.0 / state.hostBpm;
  }
  else if (std::isfinite(positionInfo.timeInSeconds) && positionInfo.timeInSeconds >= 0.0)
  {
    state.hostTimeSeconds = positionInfo.timeInSeconds;
  }
  else if (state.isPlaying && lastHostTimeSeconds >= 0.0)
  {
    state.hostTimeSeconds = lastHostTimeSeconds + static_cast<double>(numSamples) / sampleRateHz;
  }

  if (state.hostTimeSeconds < 0.0)
  {
    lastHostTimeSeconds = -1.0;
    lastHostPpq = 0.0;
    lastHostPpqValid = false;
    lastHostWasPlaying = state.isPlaying;
    return state;
  }

  if (state.isPlaying)
  {
    if (!lastHostWasPlaying)
    {
      state.isSeek = true;
    }
    else if (state.hostPpqValid && lastHostPpqValid)
    {
      const auto ppqDelta = state.hostPpqPosition - lastHostPpq;
      if (ppqDelta < -64.0 || ppqDelta > 256.0)
        state.isSeek = true;
    }
    else if (lastHostTimeSeconds >= 0.0)
    {
      const auto secDelta = state.hostTimeSeconds - lastHostTimeSeconds;
      if (secDelta < -2.0 || secDelta > 30.0)
        state.isSeek = true;
    }
  }

  lastHostTimeSeconds = state.hostTimeSeconds;
  lastHostPpq = state.hostPpqPosition;
  lastHostPpqValid = state.hostPpqValid;
  lastHostWasPlaying = state.isPlaying;
  return state;
}

juce::AudioProcessorEditor* NinjamVST3AudioProcessor::createEditor()
{
  return new NinjamVST3AudioProcessorEditor(*this);
}

bool NinjamVST3AudioProcessor::hasEditor() const
{
  return true;
}

const juce::String NinjamVST3AudioProcessor::getName() const
{
  return JucePlugin_Name;
}

bool NinjamVST3AudioProcessor::acceptsMidi() const
{
  return false;
}

bool NinjamVST3AudioProcessor::producesMidi() const
{
  return false;
}

bool NinjamVST3AudioProcessor::isMidiEffect() const
{
  return false;
}

double NinjamVST3AudioProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int NinjamVST3AudioProcessor::getNumPrograms()
{
  return 1;
}

int NinjamVST3AudioProcessor::getCurrentProgram()
{
  return 0;
}

void NinjamVST3AudioProcessor::setCurrentProgram(int index)
{
  juce::ignoreUnused(index);
}

const juce::String NinjamVST3AudioProcessor::getProgramName(int index)
{
  juce::ignoreUnused(index);
  return {};
}

void NinjamVST3AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
  juce::ignoreUnused(index, newName);
}

void NinjamVST3AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  const auto snapshot = clientService.getSnapshot();
  juce::ValueTree state("NinjamVst3State");
  state.setProperty("localGain", clientService.getLocalGain(), nullptr);
  state.setProperty("remoteGain", clientService.getRemoteGain(), nullptr);
  state.setProperty("phaseOffsetMs", clientService.getPhaseOffsetMs(), nullptr);
  state.setProperty("host", snapshot.host, nullptr);
  state.setProperty("user", snapshot.user, nullptr);
  state.setProperty("password", snapshot.password, nullptr);
  state.setProperty("monitorIncomingAudio", clientService.getMonitorIncomingAudio(), nullptr);
  state.setProperty("monitorTxAudio", clientService.getMonitorTxAudio(), nullptr);
  state.setProperty("metronomeEnabled", clientService.getMetronomeEnabled(), nullptr);

  if (auto xml = state.createXml())
  {
    copyXmlToBinary(*xml, destData);
  }
}

void NinjamVST3AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  const auto xmlState = getXmlFromBinary(data, sizeInBytes);
  if (xmlState == nullptr)
  {
    return;
  }

  const auto state = juce::ValueTree::fromXml(*xmlState);
  if (!state.isValid())
  {
    return;
  }

  clientService.setLocalGain(static_cast<float>(state.getProperty("localGain", 1.0f)));
  clientService.setRemoteGain(static_cast<float>(state.getProperty("remoteGain", 1.0f)));
  clientService.setPhaseOffsetMs(static_cast<float>(state.getProperty("phaseOffsetMs", 0.0f)));

  const auto host = state.getProperty("host", {}).toString();
  const auto user = state.getProperty("user", {}).toString();
  const auto password = state.getProperty("password", {}).toString();
  clientService.setCredentials(host, user, password);

  setMonitorIncomingAudio(static_cast<bool>(state.getProperty("monitorIncomingAudio", false)));
  setMonitorTxAudio(static_cast<bool>(state.getProperty("monitorTxAudio", false)));
  setMetronomeEnabled(static_cast<bool>(state.getProperty("metronomeEnabled", true)));

  if (host.isNotEmpty() && user.isNotEmpty())
  {
    clientService.addLogLine("Auto-connecting from project session");
    clientService.connect();
    autoConnectAttempted = true;
  }
}

void NinjamVST3AudioProcessor::connectToServer(const juce::String& host, const juce::String& user, const juce::String& password)
{
  autoConnectAttempted = true;
  clientService.setCredentials(host, user, password);
  clientService.connect();
}

void NinjamVST3AudioProcessor::disconnectFromServer()
{
  clientService.disconnect();
}

void NinjamVST3AudioProcessor::sendUserCommand(const juce::String& commandText)
{
  clientService.sendCommand(commandText);
}

void NinjamVST3AudioProcessor::setMonitorIncomingAudio(bool enabled)
{
  clientService.setMonitorIncomingAudio(enabled);
  saveMonitorIncomingSetting(enabled);
}

bool NinjamVST3AudioProcessor::getMonitorIncomingAudio() const
{
  return clientService.getMonitorIncomingAudio();
}

void NinjamVST3AudioProcessor::setMonitorTxAudio(bool enabled)
{
  clientService.setMonitorTxAudio(enabled);
  saveMonitorTxSetting(enabled);
}

bool NinjamVST3AudioProcessor::getMonitorTxAudio() const
{
  return clientService.getMonitorTxAudio();
}

void NinjamVST3AudioProcessor::setMetronomeEnabled(bool enabled)
{
  clientService.setMetronomeEnabled(enabled);
  saveMetronomeSetting(enabled);
}

bool NinjamVST3AudioProcessor::getMetronomeEnabled() const
{
  return clientService.getMetronomeEnabled();
}

NinjamClientService& NinjamVST3AudioProcessor::getClientService()
{
  return clientService;
}

const NinjamClientService& NinjamVST3AudioProcessor::getClientService() const
{
  return clientService;
}

void NinjamVST3AudioProcessor::initialiseSettings()
{
  juce::PropertiesFile::Options options;
  options.applicationName = "NinjamVST3";
  options.filenameSuffix = "settings";
  options.folderName = "Nykwil";
  options.osxLibrarySubFolder = "Application Support";
  options.storageFormat = juce::PropertiesFile::storeAsXML;

  appProperties.setStorageParameters(options);
}

void NinjamVST3AudioProcessor::loadCredentialsFromSettings()
{
  if (auto* settings = appProperties.getUserSettings())
  {
    clientService.setMonitorIncomingAudio(settings->getBoolValue("monitorIncomingAudio", false));
    clientService.setMonitorTxAudio(settings->getBoolValue("monitorTxAudio", false));
    clientService.setMetronomeEnabled(settings->getBoolValue("metronomeEnabled", true));
  }
}

void NinjamVST3AudioProcessor::saveMonitorIncomingSetting(bool enabled)
{
  if (auto* settings = appProperties.getUserSettings())
  {
    settings->setValue("monitorIncomingAudio", enabled);
    settings->saveIfNeeded();
  }
}

void NinjamVST3AudioProcessor::saveMonitorTxSetting(bool enabled)
{
  if (auto* settings = appProperties.getUserSettings())
  {
    settings->setValue("monitorTxAudio", enabled);
    settings->saveIfNeeded();
  }
}

void NinjamVST3AudioProcessor::saveMetronomeSetting(bool enabled)
{
  if (auto* settings = appProperties.getUserSettings())
  {
    settings->setValue("metronomeEnabled", enabled);
    settings->saveIfNeeded();
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new NinjamVST3AudioProcessor();
}
