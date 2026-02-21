#include "NinjamClientService.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr int kTimerHz = 20;
constexpr int kMaxLogLines = 300;
constexpr float kRemoteMeterDecay = 0.92f;
constexpr float kGainMaxLinear = 3.1622777f; // +10 dB

enum SyncMode
{
  syncHostLocked = 0,
  syncFallbackStopped = 1,
  syncFallbackNoClock = 2
};
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

NinjamClientService::NinjamClientService()
{
  state.statusText = statusCodeToText(NJClient::NJC_STATUS_DISCONNECTED);
  state.bpm = 120;
  state.bpi = 16;

  client.ChatMessage_User = this;
  client.ChatMessage_Callback = &NinjamClientService::chatMessageCallback;
  client.LicenseAgreement_User = this;
  client.LicenseAgreementCallback = &NinjamClientService::licenseAgreementCallback;
  client.config_autosubscribe = 1;
  client.config_savelocalaudio = 0;
  client.config_play_prebuffer = 4096;
  client.config_metronome_mute = false;
  client.SetLocalChannelInfo(0, "Me", true, 0, true, 96, true, true);
  applySessionChannelModeToCore();
  // Keep NJClient local monitor muted; plugin handles Add/Listen monitoring.
  client.SetLocalChannelMonitoring(0, true, 1.0f, true, 0.0f, true, true, true, false);

  configureCorePaths();
  addLogLine("Service initialized");
  startTimerHz(kTimerHz);
}

NinjamClientService::~NinjamClientService()
{
  stopTimer();
  client.Disconnect();
  for (int i = 0; i < 8; ++i)
  {
    if (client.Run())
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection management
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::setCredentials(const juce::String& host, const juce::String& user, const juce::String& password)
{
  const juce::ScopedLock scopedLock(lock);
  state.host = host.trim();
  state.user = user.trim();
  state.password = password;
}

void NinjamClientService::connect()
{
  juce::String host, user, password;
  {
    const juce::ScopedLock scopedLock(lock);
    host = state.host.trim();
    user = state.user.trim();
    password = state.password;
  }

  if (host.isEmpty() || user.isEmpty())
  {
    const juce::ScopedLock scopedLock(lock);
    state.connected = false;
    state.statusText = "Missing host or username";
    appendLogLineUnlocked("Connect failed: host and username are required");
    return;
  }

  client.Connect(host.toRawUTF8(), user.toRawUTF8(), password.toRawUTF8());

  {
    const juce::ScopedLock scopedLock(lock);
    state.intervalProgress = 0.0f;
    state.statusText = "Connecting...";
    hostPhaseAccumulatorValid = false;
    hostPhaseAccumulatorBeats = 0.0;
    lastHostPhaseBeat = 0.0;
    appendLogLineUnlocked("Connecting to " + host + " as " + user);
  }
}

void NinjamClientService::disconnect()
{
  client.Disconnect();

  const juce::ScopedLock scopedLock(lock);
  state.connected = false;
  state.statusText = statusCodeToText(NJClient::NJC_STATUS_DISCONNECTED);
  state.syncStateText = "Classic";
  state.intervalProgress = 0.0f;
  lastHostPpqValid = false;
  lastHostBpmValid = false;
  hostLockedActive = false;
  lastSyncMode = -1;
  forceSeekPending = false;
  lastServerBpm = 0;
  lastServerBpi = 0;
  hostPhaseAccumulatorValid = false;
  hostPhaseAccumulatorBeats = 0.0;
  lastHostPhaseBeat = 0.0;
  phaseRingOffsetValid = false;
  phaseRingBeatOffset = 0.0;
  inputRingIntervalLen = 0;
  appendLogLineUnlocked("Disconnected from server");
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat / commands
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::sendCommand(const juce::String& text)
{
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return;

  const juce::ScopedLock scopedLock(lock);
  appendLogLineUnlocked("> " + trimmed);

  if (!state.connected)
  {
    appendLogLineUnlocked("Not connected");
    return;
  }

  if (trimmed.startsWithChar('/'))
  {
    const auto adminCommand = trimmed.substring(1).trim();
    if (adminCommand.isNotEmpty())
    {
      client.ChatMessage_Send("ADMIN", adminCommand.toRawUTF8());
      appendLogLineUnlocked("ADMIN " + adminCommand);
    }
  }
  else
  {
    client.ChatMessage_Send("MSG", trimmed.toRawUTF8());
    appendLogLineUnlocked("MSG " + trimmed);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio processing
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::processAudioBlock(juce::AudioBuffer<float>& buffer, const TransportState& transportState)
{
  const auto numChannels = juce::jmax(1, juce::jmin(2, buffer.getNumChannels()));
  const auto blockSize = buffer.getNumSamples();
  const bool hasHostClock = transportState.hostTimeSeconds >= 0.0;
  const bool hasMusicalClock = transportState.hostBpmValid && transportState.hostPpqValid;

  // ── Read shared state under lock ──
  float localGainValue = 1.0f;
  float remoteGainValue = 1.0f;
  float phaseOffsetMsValue = 0.0f;
  MonitorMode monitorMode = MonitorMode::IncomingOnly;
  bool metronomeEnabled = true;
  int syncMode = syncFallbackNoClock;
  bool isPlaying = true;
  bool isSeek = false;
  double sessionPos = -1.0;
  int roomBpi = 16;
  double sessionBpm = 120.0;
  double rawDawPhase = -1.0;

  {
    const juce::ScopedLock scopedLock(lock);
    localGainValue = state.localGain;
    remoteGainValue = state.remoteGain;
    phaseOffsetMsValue = state.phaseOffsetMs;
    monitorMode = state.monitorMode;
    metronomeEnabled = state.metronomeEnabled;
    roomBpi = juce::jmax(1, state.bpi);
    sessionBpm = static_cast<double>(juce::jmax(1, state.bpm));
    lastHostPpq = transportState.hostPpqPosition;
    lastHostPpqValid = transportState.hostPpqValid;
    lastHostBpm = transportState.hostBpm;
    lastHostBpmValid = transportState.hostBpmValid;

    // ── Determine sync mode and compute phase ──
    if (hasHostClock && transportState.isPlaying)
    {
      syncMode = syncHostLocked;
      isPlaying = true;
      isSeek = transportState.isSeek || !hostLockedActive;
      if (forceSeekPending)
      {
        isSeek = true;
        forceSeekPending = false;
      }

      if (hasMusicalClock)
        sessionBpm = transportState.hostBpm;

      // Compute cyclic DAW phase within BPI
      const double bpiD = static_cast<double>(roomBpi);
      double phaseBeat;
      if (hasMusicalClock)
        phaseBeat = std::fmod(transportState.hostPpqPosition, bpiD);
      else
        phaseBeat = std::fmod(transportState.hostTimeSeconds * sessionBpm / 60.0, bpiD);
      if (phaseBeat < 0.0)
        phaseBeat += bpiD;

      // Smoothed phase accumulator for NJClient session position.
      // Handles DAW PPQ wrapping at BPI boundaries by picking the
      // delta candidate (raw, +cycle, -cycle) closest to expected advance.
      const double beatsPerBlock = (static_cast<double>(blockSize) * sessionBpm) /
                                   (60.0 * static_cast<double>(juce::jmax(sampleRate, 1)));

      if (!hostPhaseAccumulatorValid || isSeek)
      {
        hostPhaseAccumulatorValid = true;
        hostPhaseAccumulatorBeats = phaseBeat;
        lastHostPhaseBeat = phaseBeat;
        isSeek = true;
      }
      else
      {
        const double rawDelta = phaseBeat - lastHostPhaseBeat;
        const double expected = juce::jmax(1.0e-6, beatsPerBlock);
        const double candidates[3] = { rawDelta, rawDelta + bpiD, rawDelta - bpiD };

        double bestDelta = expected;
        double bestScore = expected;
        for (double c : candidates)
        {
          if (c < -0.02)
            continue;
          const double score = std::abs(juce::jmax(0.0, c) - expected);
          if (score < bestScore)
          {
            bestScore = score;
            bestDelta = juce::jmax(0.0, c);
          }
        }

        bestDelta = juce::jlimit(0.0, juce::jmax(expected * 8.0, 0.75), bestDelta);
        hostPhaseAccumulatorBeats += bestDelta;
        lastHostPhaseBeat = phaseBeat;
      }

      sessionPos = hostPhaseAccumulatorBeats * 60.0 / sessionBpm;
      rawDawPhase = phaseBeat;
    }
    else if (hasHostClock)
    {
      syncMode = syncFallbackStopped;
    }

    if (syncMode != syncHostLocked)
    {
      isPlaying = true;
      isSeek = false;
      if (forceSeekPending)
      {
        isSeek = true;
        forceSeekPending = false;
      }
      if (isSeek)
        hostPhaseAccumulatorValid = false;
      sessionPos = client.GetSessionPosition() / 1000.0;
    }

    hostLockedActive = (syncMode == syncHostLocked);
  }

  // ── Configure NJClient metronome ──
  // When host-locked, we mute NJClient's metronome and render our own
  // (phase-aligned to DAW beats). Otherwise let NJClient handle it.
  const bool usePhaseRing = (syncMode == syncHostLocked);
  if (usePhaseRing)
  {
    client.config_metronome_mute = true;
  }
  else
  {
    client.config_metronome_mute = !metronomeEnabled;
  }

  // ── Log sync mode changes ──
  if (syncMode != lastSyncMode)
  {
    const juce::ScopedLock scopedLock(lock);
    switch (syncMode)
    {
      case syncHostLocked:       state.syncStateText = "Host Locked"; break;
      case syncFallbackStopped:  state.syncStateText = "Fallback (Host Stopped)"; break;
      case syncFallbackNoClock:  state.syncStateText = "Fallback (No Host Clock)"; break;
    }
    lastSyncMode = syncMode;
  }

  // ── Prepare scratch buffers ──
  if (inputScratch.getNumChannels() != numChannels || inputScratch.getNumSamples() != blockSize)
    inputScratch.setSize(numChannels, blockSize, false, false, true);
  for (int ch = 0; ch < numChannels; ++ch)
    inputScratch.copyFrom(ch, 0, buffer, ch, 0, blockSize);

  const bool addLocalMonitor = (monitorMode == MonitorMode::AddLocal);
  const bool monitorTxAudio = (monitorMode == MonitorMode::ListenLocal);

  if (monitorTxAudio || addLocalMonitor)
  {
    if (txMonitorScratch.getNumChannels() != numChannels || txMonitorScratch.getNumSamples() != blockSize)
      txMonitorScratch.setSize(numChannels, blockSize, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
      txMonitorScratch.copyFrom(ch, 0, inputScratch, ch, 0, blockSize);
  }

  if (monitorTxAudio || addLocalMonitor)
    txMonitorScratch.applyGain(localGainValue);

  // Measure send level from the input feeding NJClient.
  {
    float sendPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
      sendPeak = juce::jmax(sendPeak, inputScratch.getMagnitude(ch, 0, blockSize));
    const juce::ScopedLock scopedLock(lock);
    state.sendMeter = clampMeter(sendPeak);
  }

  if (outputScratch.getNumChannels() != numChannels || outputScratch.getNumSamples() != blockSize)
    outputScratch.setSize(numChannels, blockSize, false, false, true);
  outputScratch.clear();

  float* inBuffers[2] = { inputScratch.getWritePointer(0), nullptr };
  float* outBuffers[2] = { outputScratch.getWritePointer(0), nullptr };
  if (numChannels > 1)
  {
    inBuffers[1] = inputScratch.getWritePointer(1);
    outBuffers[1] = outputScratch.getWritePointer(1);
  }
  else
  {
    inBuffers[1] = inBuffers[0];
    outBuffers[1] = outBuffers[0];
  }

  // ── Process audio through NJClient ──
  bool renderedByClient = false;
  if (client.GetStatus() == NJClient::NJC_STATUS_OK)
  {
    renderedByClient = true;
    const int safeSampleRate = juce::jmax(sampleRate, 1);

    // ── INPUT RING: remap sender audio from DAW-beat → server-position order ──
    // Ensures DAW beat 0 audio always lands at server interval position 0,
    // so the receiver's output ring can map it back to their own beat 0.
    if (usePhaseRing && phaseRingOffsetValid)
    {
      int serverPosBefore = 0, intervalLenBefore = 0;
      client.GetPosition(&serverPosBefore, &intervalLenBefore);
      if (serverPosBefore < 0) serverPosBefore = 0;

      if (intervalLenBefore > 0 && intervalLenBefore >= blockSize)
      {
        const double bpi = static_cast<double>(roomBpi);
        const double ilen = static_cast<double>(intervalLenBefore);
        double dawBeat = std::fmod(rawDawPhase, bpi);
        if (dawBeat < 0.0) dawBeat += bpi;

        if (intervalLenBefore != inputRingIntervalLen)
        {
          inputRingBuffer.setSize(numChannels, intervalLenBefore, false, true, false);
          inputRingIntervalLen = intervalLenBefore;
        }

        // Write at DAW beat position
        int writePos = static_cast<int>(dawBeat / bpi * ilen);
        writePos = ((writePos % intervalLenBefore) + intervalLenBefore) % intervalLenBefore;
        ringCopy(inputRingBuffer, writePos, inputScratch, 0, numChannels, blockSize, intervalLenBefore);

        // Read at server position → overwrite inputScratch for NJClient
        int readPos = ((serverPosBefore % intervalLenBefore) + intervalLenBefore) % intervalLenBefore;
        ringCopy(inputScratch, 0, inputRingBuffer, readPos, numChannels, blockSize, intervalLenBefore);
      }
    }

    client.AudioProc(inBuffers, numChannels, outBuffers, numChannels,
                     blockSize, safeSampleRate, false, isPlaying, isSeek, sessionPos);

    // ── OUTPUT RING: remap receiver audio from server-position → DAW-beat order ──
    // Server position 0 maps to DAW beat 0.
    if (usePhaseRing)
    {
      int serverPosAfter = 0, intervalLen = 0;
      client.GetPosition(&serverPosAfter, &intervalLen);
      if (serverPosAfter < 0) serverPosAfter = 0;

      if (intervalLen > 0 && intervalLen >= blockSize)
      {
        // Resize ring on interval length change; invalidate calibration
        if (intervalLen != phaseRingIntervalLen)
        {
          phaseRingBuffer.setSize(numChannels, intervalLen, false, true, false);
          phaseRingIntervalLen = intervalLen;
          phaseRingOffsetValid = false;

          // Seed with partial block at boundary
          if (serverPosAfter > 0 && serverPosAfter <= blockSize)
          {
            for (int ch = 0; ch < numChannels; ++ch)
              phaseRingBuffer.copyFrom(ch, 0, outBuffers[ch] + (blockSize - serverPosAfter), serverPosAfter);
          }
        }
        else
        {
          // Write AudioProc output at server position
          int writePos = serverPosAfter - blockSize;
          if (writePos < 0) writePos += intervalLen;
          ringCopy(phaseRingBuffer, writePos, outputScratch, 0, numChannels, blockSize, intervalLen);
        }

        const double bpi = static_cast<double>(roomBpi);
        const double ilen = static_cast<double>(intervalLen);

        // Calibrate once at first interval boundary after connect/BPM/BPI change.
        // Records DAW phase at the moment the server interval wraps to 0.
        if (!phaseRingOffsetValid && serverPosAfter > 0 && serverPosAfter <= blockSize)
        {
          phaseRingBeatOffset = rawDawPhase;
          phaseRingOffsetValid = true;
        }

        const int manualOffsetSamples = static_cast<int>(
          static_cast<double>(phaseOffsetMsValue) * 0.001 * static_cast<double>(safeSampleRate));

        int readPos;
        if (phaseRingOffsetValid)
        {
          // Read at DAW beat position (beat 0 → server position 0)
          double dawBeat = std::fmod(rawDawPhase, bpi);
          if (dawBeat < 0.0) dawBeat += bpi;
          readPos = static_cast<int>(dawBeat / bpi * ilen) + manualOffsetSamples;
        }
        else
        {
          // Pre-calibration: pass through (track server position)
          readPos = serverPosAfter - blockSize + manualOffsetSamples;
        }
        readPos = ((readPos % intervalLen) + intervalLen) % intervalLen;

        outputScratch.clear();
        ringCopy(outputScratch, 0, phaseRingBuffer, readPos, numChannels, blockSize, intervalLen);

        if (metronomeEnabled)
          renderMetronome(outBuffers, numChannels, blockSize, sessionBpm, roomBpi, rawDawPhase, safeSampleRate);
      }
    }
  }

  // ── Write output ──
  if (monitorTxAudio)
  {
    for (int ch = 0; ch < numChannels; ++ch)
      buffer.copyFrom(ch, 0, txMonitorScratch, ch, 0, blockSize);
  }
  else if (renderedByClient)
  {
    for (int ch = 0; ch < numChannels; ++ch)
      buffer.copyFrom(ch, 0, outputScratch, ch, 0, blockSize);
    if (remoteGainValue != 1.0f)
      buffer.applyGain(remoteGainValue);

    if (addLocalMonitor)
    {
      for (int ch = 0; ch < numChannels; ++ch)
        buffer.addFrom(ch, 0, txMonitorScratch, ch, 0, blockSize);
    }
  }

  // ── Update meters ──
  updateMetersFromBuffer(buffer);
  const auto remote = client.GetOutputPeak();
  {
    const juce::ScopedLock scopedLock(lock);
    state.remoteMeter = clampMeter(state.remoteMeter * kRemoteMeterDecay + remote * (1.0f - kRemoteMeterDecay));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::setSampleRate(int sampleRateHz)
{
  sampleRate = juce::jmax(sampleRateHz, 1);
}

void NinjamClientService::setMonitorMode(MonitorMode mode)
{
  const juce::ScopedLock scopedLock(lock);
  if (state.monitorMode == mode)
    return;

  state.monitorMode = mode;
  switch (mode)
  {
    case MonitorMode::IncomingOnly: appendLogLineUnlocked("Monitor mode: incoming only"); break;
    case MonitorMode::AddLocal:     appendLogLineUnlocked("Monitor mode: add local"); break;
    case MonitorMode::ListenLocal:  appendLogLineUnlocked("Monitor mode: listen local"); break;
  }
}

NinjamClientService::MonitorMode NinjamClientService::getMonitorMode() const
{
  const juce::ScopedLock scopedLock(lock);
  return state.monitorMode;
}

void NinjamClientService::setMetronomeEnabled(bool enabled)
{
  const juce::ScopedLock scopedLock(lock);
  state.metronomeEnabled = enabled;
}

bool NinjamClientService::getMetronomeEnabled() const
{
  const juce::ScopedLock scopedLock(lock);
  return state.metronomeEnabled;
}

void NinjamClientService::setLocalGain(float value)
{
  const juce::ScopedLock scopedLock(lock);
  state.localGain = juce::jlimit(0.0f, kGainMaxLinear, value);
}

void NinjamClientService::setRemoteGain(float value)
{
  const juce::ScopedLock scopedLock(lock);
  state.remoteGain = juce::jlimit(0.0f, kGainMaxLinear, value);
}

float NinjamClientService::getLocalGain() const
{
  const juce::ScopedLock scopedLock(lock);
  return state.localGain;
}

float NinjamClientService::getRemoteGain() const
{
  const juce::ScopedLock scopedLock(lock);
  return state.remoteGain;
}

void NinjamClientService::setPhaseOffsetMs(float ms)
{
  const juce::ScopedLock scopedLock(lock);
  state.phaseOffsetMs = juce::jlimit(-500.0f, 500.0f, ms);
}

void NinjamClientService::setUserChannelMute(int userIdx, int channelIdx, bool mute)
{
  client.SetUserChannelState(userIdx, channelIdx,
                             false, false, false, 0.0f, false, 0.0f,
                             true, mute, false, false);
}

void NinjamClientService::setUserChannelSolo(int userIdx, int channelIdx, bool solo)
{
  client.SetUserChannelState(userIdx, channelIdx,
                             false, false, false, 0.0f, false, 0.0f,
                             false, false, true, solo);
}

void NinjamClientService::setUserChannelVolume(int userIdx, int channelIdx, float volume)
{
  client.SetUserChannelState(userIdx, channelIdx,
                             false, false, true, juce::jlimit(0.0f, kGainMaxLinear, volume),
                             false, 0.0f, false, false, false, false);
}

float NinjamClientService::getPhaseOffsetMs() const
{
  const juce::ScopedLock scopedLock(lock);
  return state.phaseOffsetMs;
}

NinjamClientService::Snapshot NinjamClientService::getSnapshot() const
{
  const juce::ScopedLock scopedLock(lock);
  return state;
}

void NinjamClientService::addLogLine(const juce::String& message)
{
  const juce::ScopedLock scopedLock(lock);
  appendLogLineUnlocked(message);
}

// ─────────────────────────────────────────────────────────────────────────────
// Timer / polling
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::timerCallback()
{
  for (int i = 0; i < 8; ++i)
  {
    if (client.Run())
      break;
  }

  if (client.HasUserInfoChanged() != 0)
  {
    ensureAllRemoteChannelsSubscribed();
    warnIfDuplicateUsername();
  }

  refreshStatusFromCore();
}

void NinjamClientService::ensureAllRemoteChannelsSubscribed()
{
  if (client.GetStatus() != NJClient::NJC_STATUS_OK)
    return;

  const auto users = client.GetNumUsers();
  for (int userIdx = 0; userIdx < users; ++userIdx)
  {
    for (int i = 0;; ++i)
    {
      const int chanIdx = client.EnumUserChannels(userIdx, i);
      if (chanIdx < 0)
        break;

      bool subscribed = true;
      if (client.GetUserChannelState(userIdx, chanIdx, &subscribed) == nullptr)
        continue;

      if (!subscribed)
      {
        client.SetUserChannelState(userIdx, chanIdx,
                                   true, true, true, 1.0f, false, 0.0f,
                                   false, false, false, false, false, 0);
      }
    }
  }
}

void NinjamClientService::warnIfDuplicateUsername()
{
  if (client.GetStatus() != NJClient::NJC_STATUS_OK)
  {
    duplicateNameWarned = false;
    return;
  }

  juce::String myUser;
  {
    const juce::ScopedLock scopedLock(lock);
    myUser = state.user.trim();
  }

  if (myUser.isEmpty())
  {
    duplicateNameWarned = false;
    return;
  }

  int sameNameCount = 0;
  const auto users = client.GetNumUsers();
  for (int i = 0; i < users; ++i)
  {
    const char* userName = client.GetUserState(i);
    if (userName != nullptr && myUser.equalsIgnoreCase(userName))
      ++sameNameCount;
  }

  if (sameNameCount > 1 && !duplicateNameWarned)
  {
    const juce::ScopedLock scopedLock(lock);
    appendLogLineUnlocked("Warning: duplicate username detected; use unique names per instance");
    duplicateNameWarned = true;
  }
  else if (sameNameCount <= 1)
  {
    duplicateNameWarned = false;
  }
}

void NinjamClientService::refreshStatusFromCore()
{
  const auto statusCode = client.GetStatus();

  int intervalPos = 0, intervalLen = 0;
  client.GetPosition(&intervalPos, &intervalLen);
  const auto progress = intervalLen > 0 ? static_cast<float>(intervalPos) / static_cast<float>(intervalLen) : 0.0f;

  const auto bpm = juce::roundToInt(client.GetActualBPM());
  const auto bpi = client.GetBPI();

  const juce::ScopedLock scopedLock(lock);
  state.connected = (statusCode == NJClient::NJC_STATUS_OK);
  state.statusText = statusCodeToText(statusCode);

  if (bpm > 0 && lastServerBpm > 0 && bpm != lastServerBpm)
  {
    forceSeekPending = true;
    appendLogLineUnlocked("Server BPM changed to " + juce::String(bpm) + ", scheduling resync");
  }
  if (bpi > 0 && lastServerBpi > 0 && bpi != lastServerBpi)
  {
    forceSeekPending = true;
    appendLogLineUnlocked("Server BPI changed to " + juce::String(bpi) + ", scheduling resync");
  }
  if (bpm > 0) lastServerBpm = bpm;
  if (bpi > 0) lastServerBpi = bpi;

  if (bpm > 0) state.serverBpm = bpm;
  state.hostBpmValid = lastHostBpmValid;
  state.hostBpm = lastHostBpmValid ? juce::roundToInt(lastHostBpm) : 0;

  if (hostLockedActive && lastHostBpmValid)
    state.bpm = juce::roundToInt(lastHostBpm);
  else if (bpm > 0)
    state.bpm = bpm;
  if (bpi > 0) state.bpi = bpi;

  if (hostLockedActive && lastHostPpqValid && state.bpi > 0)
  {
    const auto bpiD = static_cast<double>(state.bpi);
    auto beatInInterval = std::fmod(lastHostPpq, bpiD);
    if (beatInInterval < 0.0) beatInInterval += bpiD;
    state.intervalProgress = clampMeter(static_cast<float>(beatInInterval / bpiD));
  }
  else
  {
    state.intervalProgress = clampMeter(progress);
  }

  if (statusCode != lastStatusCode)
  {
    appendLogLineUnlocked("Status: " + state.statusText);
    lastStatusCode = statusCode;
  }

  // Enumerate remote users and channels
  state.remoteUsers.clear();
  if (state.connected)
  {
    const int numUsers = client.GetNumUsers();
    for (int u = 0; u < numUsers; ++u)
    {
      const char* userName = client.GetUserState(u);
      if (userName == nullptr)
        continue;

      RemoteUser user;
      user.name = juce::String(userName);
      user.userIndex = u;

      for (int i = 0;; ++i)
      {
        const int chanIdx = client.EnumUserChannels(u, i);
        if (chanIdx < 0)
          break;

        bool sub = false, muted = false, solo = false;
        float vol = 1.0f, pan = 0.0f;
        const char* chanName = client.GetUserChannelState(u, chanIdx, &sub, &vol, &pan, &muted, &solo);

        UserChannel ch;
        ch.name = chanName ? juce::String(chanName) : juce::String("ch" + juce::String(chanIdx));
        ch.channelIndex = chanIdx;
        ch.volume = vol;
        ch.muted = muted;
        ch.solo = solo;
        ch.peak = clampMeter(client.GetUserChannelPeak(u, chanIdx));
        user.channels.push_back(ch);
      }

      state.remoteUsers.push_back(std::move(user));
    }
  }

  if (!state.connected)
  {
    state.localMeter *= 0.9f;
    state.remoteMeter *= 0.9f;
    state.sendMeter *= 0.9f;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Metering
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::updateMetersFromBuffer(const juce::AudioBuffer<float>& buffer)
{
  const auto numCh = juce::jmin(2, buffer.getNumChannels());
  if (numCh <= 0 || buffer.getNumSamples() <= 0)
  {
    state.localMeter = 0.0f;
    return;
  }

  auto peak = 0.0f;
  for (int ch = 0; ch < numCh; ++ch)
    peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));

  const juce::ScopedLock scopedLock(lock);
  state.localMeter = clampMeter(peak);
}

float NinjamClientService::clampMeter(float value)
{
  return juce::jlimit(0.0f, 1.0f, value);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ring buffer helper
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::ringCopy(juce::AudioBuffer<float>& dst, int dstPos,
                                   const juce::AudioBuffer<float>& src, int srcPos,
                                   int numChannels, int numSamples, int ringLen)
{
  if (srcPos + numSamples <= ringLen && dstPos + numSamples <= dst.getNumSamples())
  {
    for (int ch = 0; ch < numChannels; ++ch)
      dst.copyFrom(ch, dstPos, src, ch, srcPos, numSamples);
  }
  else
  {
    const int srcWrap = ringLen;
    const int dstWrap = dst.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
      const int si = (srcPos + i) % srcWrap;
      const int di = (dstPos + i) % dstWrap;
      for (int ch = 0; ch < numChannels; ++ch)
        dst.getWritePointer(ch)[di] = src.getReadPointer(ch)[si];
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Internals
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::configureCorePaths()
{
  auto dataRoot = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Nykwil")
                    .getChildFile("NinjamNext");
  dataRoot.createDirectory();

  auto sessionRoot = dataRoot.getChildFile("sessions");
  sessionRoot.createDirectory();

  const auto sessionPath = sessionRoot.getFullPathName();
  juce::HeapBlock<char> mutablePath(static_cast<size_t>(sessionPath.getNumBytesAsUTF8() + 1));
  std::memcpy(mutablePath.getData(), sessionPath.toRawUTF8(), static_cast<size_t>(sessionPath.getNumBytesAsUTF8() + 1));
  client.SetWorkDir(mutablePath.getData());

  const auto logPath = dataRoot.getChildFile("ninjam-client.log").getFullPathName();
  client.SetLogFile(logPath.toRawUTF8());
}

void NinjamClientService::appendLogLineUnlocked(const juce::String& line)
{
  state.logLines.add(line);
  while (state.logLines.size() > kMaxLogLines)
    state.logLines.remove(0);
}

void NinjamClientService::handleChatMessage(const char** parms, int nparms)
{
  if (parms == nullptr || nparms <= 0 || parms[0] == nullptr)
    return;

  juce::String line;
  const auto type = juce::String(parms[0]);

  if (type == "MSG" || type == "PRIVMSG")
  {
    const auto from = (nparms > 1 && parms[1]) ? juce::String(parms[1]) : juce::String();
    const auto text = (nparms > 2 && parms[2]) ? juce::String(parms[2]) : juce::String();
    line = from.isNotEmpty() ? ("<" + from + "> " + text) : text;
  }
  else if (type == "JOIN" || type == "PART")
  {
    const auto who = (nparms > 1 && parms[1]) ? juce::String(parms[1]) : juce::String("(unknown)");
    line = "*** " + who + (type == "JOIN" ? " joined" : " left");
  }
  else
  {
    line = type;
    for (int i = 1; i < juce::jmin(nparms, 5); ++i)
    {
      if (parms[i] != nullptr && *parms[i] != 0)
        line += " | " + juce::String(parms[i]);
    }
  }

  const juce::ScopedLock scopedLock(lock);
  appendLogLineUnlocked(line);
}

void NinjamClientService::chatMessageCallback(void* userData, NJClient* inst, const char** parms, int nparms)
{
  juce::ignoreUnused(inst);
  if (auto* self = static_cast<NinjamClientService*>(userData))
    self->handleChatMessage(parms, nparms);
}

int NinjamClientService::onLicenseAgreement(const char* licenseText)
{
  const juce::ScopedLock scopedLock(lock);
  appendLogLineUnlocked("Server license presented; auto-accepting");
  if (licenseText != nullptr && *licenseText != 0)
  {
    juce::String firstLine(licenseText);
    const auto lineBreak = firstLine.indexOfChar('\n');
    if (lineBreak > 0)
      firstLine = firstLine.substring(0, lineBreak);
    appendLogLineUnlocked("License: " + firstLine.trim());
  }
  return 1;
}

int NinjamClientService::licenseAgreementCallback(void* userData, const char* licenseText)
{
  if (auto* self = static_cast<NinjamClientService*>(userData))
    return self->onLicenseAgreement(licenseText);
  return 1;
}

juce::String NinjamClientService::statusCodeToText(int statusCode)
{
  switch (statusCode)
  {
    case NJClient::NJC_STATUS_OK:           return "Connected";
    case NJClient::NJC_STATUS_PRECONNECT:   return "Connecting...";
    case NJClient::NJC_STATUS_INVALIDAUTH:  return "Invalid auth";
    case NJClient::NJC_STATUS_CANTCONNECT:  return "Cannot connect";
    case NJClient::NJC_STATUS_DISCONNECTED: return "Disconnected";
    default:                                return "Unknown status";
  }
}

void NinjamClientService::applySessionChannelModeToCore()
{
  int srcch = 0, bitrate = 96, outch = 0, flags = 0;
  bool broadcast = true;

  const char* channelNameRaw = client.GetLocalChannelInfo(0, &srcch, &bitrate, &broadcast, &outch, &flags);
  juce::String channelName = channelNameRaw ? juce::String(channelNameRaw) : juce::String("Me");

  float monitorVol = 1.0f, monitorPan = 0.0f;
  bool monitorMute = false, monitorSolo = false;
  if (client.GetLocalChannelMonitoring(0, &monitorVol, &monitorPan, &monitorMute, &monitorSolo) != 0)
  {
    monitorVol = 1.0f;
    monitorPan = 0.0f;
    monitorMute = false;
    monitorSolo = false;
  }

  // Clear session mode flags (bits 1 and 2) to use classic mode
  const int desiredFlags = flags & ~(2 | 4);

  client.DeleteLocalChannel(0);
  client.SetLocalChannelInfo(0, channelName.toRawUTF8(),
                             true, srcch, true, bitrate, true, broadcast,
                             true, outch, true, desiredFlags);
  client.SetLocalChannelMonitoring(0, true, monitorVol, true, monitorPan,
                                   true, monitorMute, true, monitorSolo);
}

// ─────────────────────────────────────────────────────────────────────────────
// Plugin-side metronome (phase-aligned to DAW beats)
// ─────────────────────────────────────────────────────────────────────────────

void NinjamClientService::renderMetronome(float** outBuffers, int numChannels, int blockSize,
                                          double bpm, int bpi, double phaseBeats, int sampleRateHz)
{
  const int clickLen = sampleRateHz / 100;
  const double sc = 6000.0 / static_cast<double>(sampleRateHz);
  const double beatInc = bpm / (60.0 * static_cast<double>(sampleRateHz));
  const double metroVol = static_cast<double>(client.config_metronome);

  for (int x = 0; x < blockSize; ++x)
  {
    const double beatNow = phaseBeats + static_cast<double>(x) * beatInc;
    const int beatNowInt = static_cast<int>(std::floor(beatNow + 1.0e-12));
    const int beatPrevInt = static_cast<int>(std::floor(beatNow - beatInc + 1.0e-12));

    if (beatNowInt != beatPrevInt)
    {
      metronomeClickState = 1;
      int beatInInterval = beatNowInt % bpi;
      if (beatInInterval < 0) beatInInterval += bpi;
      metronomeClickAccent = (beatInInterval == 0);
    }

    if (metronomeClickState > 0)
    {
      const double val = metronomeClickAccent
        ? std::sin(static_cast<double>(metronomeClickState) * sc) * metroVol
        : std::sin(static_cast<double>(metronomeClickState) * sc * 2.0) * 0.25 * metroVol;

      for (int ch = 0; ch < numChannels; ++ch)
        outBuffers[ch][x] += static_cast<float>(val);

      if (++metronomeClickState >= clickLen)
        metronomeClickState = 0;
    }
  }
}
