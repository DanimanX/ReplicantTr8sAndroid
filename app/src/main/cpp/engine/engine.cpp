#include "engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace rt8 {

namespace {
constexpr uint8_t kNoteOn = 0x90;
constexpr uint8_t kNoteOff = 0x80;
constexpr uint8_t kControlChange = 0xB0;
constexpr uint8_t kProgramChange = 0xC0;
constexpr uint8_t kClock = 0xF8;
constexpr uint8_t kStart = 0xFA;
constexpr uint8_t kContinue = 0xFB;
constexpr uint8_t kStop = 0xFC;
constexpr uint8_t kSongPosition = 0xF2;
} // namespace

Engine::Engine() {
    auto preview = std::make_unique<FirmwareBank>();
    buildPreviewBank(*preview);
    bankStorage_.push_back(std::move(preview));
    bank_.store(bankStorage_.back().get(), std::memory_order_release);

    for (int v = 0; v < kNumVariations; ++v)
        for (int t = 0; t < kNumTracks; ++t)
            for (int s = 0; s < kNumSteps; ++s)
                grid_[v][t][s].store(0, std::memory_order_relaxed);

    for (int ch = 0; ch < kNumChannels; ++ch) {
        const size_t base = static_cast<size_t>(ch) * kChannelParamCount;
        channelParams_[base + kTune].store(0.5f);
        channelParams_[base + kDecay].store(0.5f);
        channelParams_[base + kCtrl].store(0.5f);
        channelParams_[base + kLevel].store(1.0f);
        channelParams_[base + kPan].store(0.5f);
        channelParams_[base + kReverbSend].store(0.2f);
        channelParams_[base + kDelaySend].store(0.1f);
        toneIds_[ch].store(0);
        muted_[ch].store(false);
        soloed_[ch].store(false);
        meters_[ch].store(0.0f);
        voices_[ch].setChannelIndex(ch);
    }

    masterParams_[kMasterVolume].store(0.85f);
    masterParams_[kMasterDrive].store(0.10f);
    masterParams_[kMasterReverb].store(0.50f);
    masterParams_[kMasterDelay].store(0.45f);
    masterParams_[kMasterFilter].store(0.50f);
    masterParams_[kScatterDepth].store(0.50f);

    loadKit(1);
    loadPattern(103, false);
    resetRequested_.store(false, std::memory_order_relaxed);
}

void Engine::prepare(double newSampleRate, int maxBlockFrames) {
    sampleRate_ = std::max(8000.0, newSampleRate);
    maxBlockFrames_ = std::max(256, std::min(maxBlockFrames, kMaxBlockFrames));
    for (auto& v : voices_) {
        v.setBank(bank());
        v.prepare(sampleRate_);
    }
    masterBus_.prepare(sampleRate_, maxBlockFrames_);
    silentBufferFrames_ = maxBlockFrames_;
    silentBuffer_ = std::make_unique<float[]>(static_cast<size_t>(maxBlockFrames_) * 2);
    sampleCounterInStep_ = 0.0;
    samplesUntilClock_ = 0.0;
}

int64_t Engine::nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Engine::applyBlockConfig(const FirmwareBank* b) {
    if (b == nullptr) return;

    const double bpm = tempo_.load(std::memory_order_relaxed);
    masterBus_.setTempoBpm(bpm);
    masterBus_.setMasterVolume(masterParams_[kMasterVolume].load(std::memory_order_relaxed));
    masterBus_.setMasterDrive(masterParams_[kMasterDrive].load(std::memory_order_relaxed));
    masterBus_.setReverbLevel(masterParams_[kMasterReverb].load(std::memory_order_relaxed));
    masterBus_.setDelayLevel(masterParams_[kMasterDelay].load(std::memory_order_relaxed));
    masterBus_.setMasterFxFilter(masterParams_[kMasterFilter].load(std::memory_order_relaxed));
    masterBus_.setScatterDepth(masterParams_[kScatterDepth].load(std::memory_order_relaxed));
    masterBus_.setScatterActive(scatterActive_.load(std::memory_order_relaxed));
    masterBus_.setScatterType(scatterType_.load(std::memory_order_relaxed));

    for (int ch = 0; ch < kNumChannels; ++ch) {
        auto& v = voices_[static_cast<size_t>(ch)];
        const size_t base = static_cast<size_t>(ch) * kChannelParamCount;
        v.setBank(b);
        v.setTune(channelParams_[base + kTune].load(std::memory_order_relaxed));
        v.setDecay(channelParams_[base + kDecay].load(std::memory_order_relaxed));
        v.setCtrl(channelParams_[base + kCtrl].load(std::memory_order_relaxed));
        v.setLevel(channelParams_[base + kLevel].load(std::memory_order_relaxed));
        v.setPan(channelParams_[base + kPan].load(std::memory_order_relaxed));
        const int tone = toneIds_[ch].load(std::memory_order_relaxed);
        if (tone != lastToneApplied_[ch]) {
            v.setToneId(static_cast<uint16_t>(tone));
            lastToneApplied_[ch] = tone;
        }
    }
}

bool Engine::isAudible(int ch) const {
    bool anySolo = false;
    for (int i = 0; i < kNumChannels; ++i) {
        if (soloed_[static_cast<size_t>(i)].load(std::memory_order_relaxed)) {
            anySolo = true;
            break;
        }
    }
    if (anySolo) return soloed_[static_cast<size_t>(ch)].load(std::memory_order_relaxed);
    return !muted_[static_cast<size_t>(ch)].load(std::memory_order_relaxed);
}

void Engine::process(float* interleavedStereo, int frames, int64_t blockStartNanos) {
    if (interleavedStereo == nullptr || frames <= 0) return;

    if (frames > maxBlockFrames_) {
        int remaining = frames;
        float* out = interleavedStereo;
        int64_t start = blockStartNanos > 0 ? blockStartNanos : nowNanos();
        while (remaining > 0) {
            const int chunk = std::min(remaining, maxBlockFrames_);
            process(out, chunk, start);
            out += static_cast<size_t>(chunk) * 2;
            start += static_cast<int64_t>((static_cast<double>(chunk) / sampleRate_) * 1.0e9);
            remaining -= chunk;
        }
        return;
    }

    const FirmwareBank* b = bank();
    applyBlockConfig(b);

    const int64_t start = blockStartNanos > 0 ? blockStartNanos : nowNanos();
    const double bpm = tempo_.load(std::memory_order_relaxed);
    const double shuffle = shuffle_.load(std::memory_order_relaxed);
    const bool clockReceive = clockReceive_.load(std::memory_order_relaxed);
    const bool playing = playing_.load(std::memory_order_relaxed);
    const bool anySolo = std::any_of(soloed_.begin(), soloed_.end(),
                                     [](const std::atomic<bool>& a) { return a.load(std::memory_order_relaxed); });

    if (clockReceive != wasClockReceive_) {
        wasClockReceive_ = clockReceive;
        clockPulse_ = 0;
        clockTickCount_ = 0;
        lastClockNanos_ = 0;
        samplesUntilClock_ = 0.0;
        sampleCounterInStep_ = 0.0;
    }

    // Gather inbound events once per block, then merge them by sample offset.
    std::array<std::pair<int, InEvent>, 256> pending {};
    int pendingCount = 0;
    InEvent ev {};
    while (pendingCount < static_cast<int>(pending.size()) && popIn(ev)) {
        int offset = 0;
        if (ev.timestampNanos > start) {
            const double delta = static_cast<double>(ev.timestampNanos - start);
            offset = static_cast<int>(delta * sampleRate_ / 1.0e9);
            if (offset >= frames) offset = frames - 1;
        }
        pending[static_cast<size_t>(pendingCount)] = { offset, ev };
        ++pendingCount;
    }
    std::stable_sort(pending.begin(), pending.begin() + pendingCount,
              [](const auto& a, const auto& b) { return a.first < b.first; });

    const bool reset = resetRequested_.exchange(false, std::memory_order_relaxed);
    const bool transportMaster = !clockReceive;
    if (playing && !lastPlaying_) {
        if (transportSend_.load(std::memory_order_relaxed) && transportMaster) emitOut(kStart, 0, 0, start);
        playheadStep_.store(0, std::memory_order_relaxed);
        sampleCounterInStep_ = 0.0;
        clockPulse_ = 0;
        clockTickCount_ = 0;
        samplesUntilClock_ = 0.0;
        advanceStep(0, start);
        lastPlaying_ = true;
    } else if (!playing && lastPlaying_) {
        if (transportSend_.load(std::memory_order_relaxed) && transportMaster) emitOut(kStop, 0, 0, start);
        lastPlaying_ = false;
    } else if (reset) {
        playheadStep_.store(0, std::memory_order_relaxed);
        sampleCounterInStep_ = 0.0;
        clockPulse_ = 0;
        clockTickCount_ = 0;
        samplesUntilClock_ = 0.0;
        if (playing) advanceStep(0, start);
    }

    const bool runningInternal = playing && !clockReceive;
    const bool sendingClock = playing && clockSend_.load(std::memory_order_relaxed) && !clockReceive;

    float revSend[kNumChannels] {};
    float dlySend[kNumChannels] {};
    bool audible[kNumChannels] {};
    for (int ch = 0; ch < kNumChannels; ++ch) {
        const size_t base = static_cast<size_t>(ch) * kChannelParamCount;
        revSend[ch] = channelParams_[base + kReverbSend].load(std::memory_order_relaxed);
        dlySend[ch] = channelParams_[base + kDelaySend].load(std::memory_order_relaxed);
        if (anySolo) audible[ch] = soloed_[static_cast<size_t>(ch)].load(std::memory_order_relaxed);
        else audible[ch] = !muted_[static_cast<size_t>(ch)].load(std::memory_order_relaxed);
        meters_[ch].store(meters_[ch].load(std::memory_order_relaxed) * 0.86f, std::memory_order_relaxed);
    }

    int sWrite = scopeWrite_.load(std::memory_order_relaxed);
    int evIndex = 0;
    int currentStep = playheadStep_.load(std::memory_order_relaxed);
    double baseSamplesPerStep = (60.0 / std::max(40.0, bpm)) * 0.25 * sampleRate_;

    for (int i = 0; i < frames; ++i) {
        const int64_t sampleTs = start + static_cast<int64_t>(
                                               (static_cast<double>(i) / sampleRate_) * 1.0e9);

        while (evIndex < pendingCount && pending[static_cast<size_t>(evIndex)].first <= i) {
            handleEvent(pending[static_cast<size_t>(evIndex)].second, sampleTs);
            ++evIndex;
        }

        if (runningInternal) {
            const double swingFactor = (currentStep & 1) ? (1.0 - shuffle * 0.45)
                                                         : (1.0 + shuffle * 0.45);
            const double stepLen = std::max(256.0, baseSamplesPerStep * swingFactor);
            sampleCounterInStep_ += 1.0;
            if (sampleCounterInStep_ >= stepLen) {
                sampleCounterInStep_ -= stepLen;
                currentStep = (currentStep + 1) % kNumSteps;
                playheadStep_.store(currentStep, std::memory_order_relaxed);
                advanceStep(currentStep, sampleTs);
            }
        }

        if (sendingClock) {
            const double samplesPerClock = (60.0 / std::max(40.0, bpm)) * sampleRate_ / 24.0;
            samplesUntilClock_ += 1.0;
            if (samplesUntilClock_ >= samplesPerClock) {
                samplesUntilClock_ -= samplesPerClock;
                emitOut(kClock, 0, 0, sampleTs);
            }
        } else {
            samplesUntilClock_ = 0.0;
        }

        float mixL = 0.0f;
        float mixR = 0.0f;
        float sendRevL = 0.0f;
        float sendRevR = 0.0f;
        float sendDlyL = 0.0f;
        float sendDlyR = 0.0f;

        for (int ch = 0; ch < kNumChannels; ++ch) {
            auto& v = voices_[static_cast<size_t>(ch)];
            if (!v.isActive()) continue;
            float chL = 0.0f;
            float chR = 0.0f;
            v.renderNextSample(chL, chR);
            if (!audible[ch]) continue;

            mixL += chL;
            mixR += chR;
            sendRevL += chL * revSend[ch];
            sendRevR += chR * revSend[ch];
            sendDlyL += chL * dlySend[ch];
            sendDlyR += chR * dlySend[ch];

            const float peak = std::max(std::abs(chL), std::abs(chR));
            if (peak > meters_[ch].load(std::memory_order_relaxed))
                meters_[ch].store(peak, std::memory_order_relaxed);
        }

        masterBus_.processFrame(mixL, mixR, sendRevL, sendRevR, sendDlyL, sendDlyR, currentStep);

        interleavedStereo[static_cast<size_t>(i) * 2] = mixL;
        interleavedStereo[static_cast<size_t>(i) * 2 + 1] = mixR;

        const float masterPeak = std::max(std::abs(mixL), std::abs(mixR));
        if (masterPeak > masterPeakL_.load(std::memory_order_relaxed))
            masterPeakL_.store(masterPeak, std::memory_order_relaxed);
        if (masterPeak > masterPeakR_.load(std::memory_order_relaxed))
            masterPeakR_.store(masterPeak, std::memory_order_relaxed);

        if ((i & 15) == 0) {
            scopeRing_[static_cast<size_t>(sWrite)].store(0.5f * (mixL + mixR),
                                                         std::memory_order_relaxed);
            sWrite = (sWrite + 1) & 127;
        }
    }

    scopeWrite_.store(sWrite, std::memory_order_relaxed);
    masterPeakL_.store(masterPeakL_.load(std::memory_order_relaxed) * 0.82f, std::memory_order_relaxed);
    masterPeakR_.store(masterPeakR_.load(std::memory_order_relaxed) * 0.82f, std::memory_order_relaxed);
}

void Engine::renderSilent(int frames) {
    if (!silentBuffer_ || frames <= 0) return;
    int remaining = std::min(frames, 4096);
    while (remaining > 0) {
        const int n = std::min(remaining, maxBlockFrames_);
        process(silentBuffer_.get(), n, nowNanos());
        remaining -= n;
    }
}

void Engine::advanceStep(int step, int64_t timestampNanos) {
    step = clampi(step, 0, kNumSteps - 1);
    const bool fill = fill_.load(std::memory_order_relaxed);
    const int variation = clampi(fill ? (kNumVariations - 1) : variation_.load(std::memory_order_relaxed),
                                 0, kNumVariations - 1);
    const float accent = accent_.load(std::memory_order_relaxed);
    const uint16_t accentCell = grid_[static_cast<size_t>(variation)][kNumChannels][static_cast<size_t>(step)]
                                    .load(std::memory_order_relaxed);
    const uint8_t accentVel = static_cast<uint8_t>(accentCell & 0xFF);
    const float stepAccent = (accentVel > 85) ? accent : (accentVel > 0 ? accent * 0.35f : 0.0f);

    for (int ch = 0; ch < kNumChannels; ++ch) {
        if (!isAudible(ch)) continue;
        const uint16_t packed = grid_[static_cast<size_t>(variation)][static_cast<size_t>(ch)]
                                     [static_cast<size_t>(step)]
                                         .load(std::memory_order_relaxed);
        uint8_t velocity = static_cast<uint8_t>(packed & 0xFF);
        const uint8_t flags = static_cast<uint8_t>((packed >> 8) & 0xFF);

        if (fill && step >= 12 && (ch == 1 || ch == 2 || ch == 3 || ch == 4)) {
            if ((step == 12 && ch == 2) || (step == 13 && ch == 3) || (step >= 14 && ch == 1))
                velocity = 105;
        }

        if (velocity == 0) continue;
        const float normVel = clampf(static_cast<float>(velocity) / 95.0f, 0.2f, 1.2f);
        const bool flam = ((flags & 0x0F) != 0) || (fill && step == 15 && ch == 1);
        triggerVoice(ch, normVel, flam, timestampNanos, stepAccent);
    }

    playheadStep_.store(step, std::memory_order_relaxed);
    if (fill && step == kNumSteps - 1) fill_.store(false, std::memory_order_relaxed);
}

void Engine::triggerVoice(int ch, float velocity, bool flam, int64_t timestampNanos, float accentOverride) {
    if (!validChannel(ch) || !isAudible(ch)) return;
    if (ch == 7) voices_[8].choke();
    const float accentAmount = accentOverride >= 0.0f
                                   ? accentOverride
                                   : accent_.load(std::memory_order_relaxed) * 0.5f;
    voices_[static_cast<size_t>(ch)].trigger(velocity, accentAmount, flam);
    emitNote(ch, velocity, timestampNanos);
}

void Engine::handleEvent(const InEvent& event, int64_t timestampNanos) {
    if (event.kind == 1) {
        triggerVoice(event.data1, event.velocity, event.data2 != 0, timestampNanos);
        return;
    }

    const uint8_t status = event.status;
    if (status >= 0xF0) {
        switch (status) {
            case kClock: handleClock(timestampNanos); break;
            case kStart: handleStart(timestampNanos); break;
            case kContinue: handleContinue(); break;
            case kStop: handleStop(); break;
            case kSongPosition:
                if (clockReceive_.load(std::memory_order_relaxed))
                    handleSongPosition(event.data1 | (event.data2 << 7));
                break;
            default: break;
        }
        return;
    }

    const int channel = status & 0x0F;
    const int filter = midiChannel_.load(std::memory_order_relaxed);
    if (filter >= 0 && channel != filter) return;

    const int type = status & 0xF0;
    if (type == kNoteOn && event.data2 > 0) {
        const int ch = noteToChannel(event.data1);
        if (ch >= 0) triggerVoice(ch, static_cast<float>(event.data2) / 127.0f, false, timestampNanos);
        return;
    }
    if (type == kNoteOff || (type == kNoteOn && event.data2 == 0)) return;

    if (type == kControlChange) {
        const float norm = static_cast<float>(event.data2) / 127.0f;
        switch (event.data1) {
            case 7: setMasterParam(kMasterVolume, norm); break;
            case 74: setMasterParam(kMasterFilter, norm); break;
            case 91: setMasterParam(kMasterReverb, norm); break;
            case 93: setMasterParam(kMasterDelay, norm); break;
            case 94: setMasterParam(kScatterDepth, norm); break;
            case 95: setScatterType(1 + static_cast<int>(norm * 7.0f)); break;
            case 20: case 21: case 22: case 23: case 24: case 25: case 26:
                setChannelParam(getSelectedChannel(), event.data1 - 20, norm);
                break;
            case 120:
            case 123:
                for (auto& v : voices_) v.choke();
                break;
            default: break;
        }
        return;
    }

    if (type == kProgramChange) loadKit(static_cast<int>(event.data1));
}

void Engine::handleClock(int64_t timestampNanos) {
    if (!clockReceive_.load(std::memory_order_relaxed)) return;

    externalClockSeenNanos_.store(nowNanos(), std::memory_order_relaxed);

    if (lastClockNanos_ > 0) {
        const double delta = static_cast<double>(timestampNanos - lastClockNanos_) / 1.0e9;
        if (delta > 0.002 && delta < 0.25) {
            const double measured = clampd(60.0 / (delta * 24.0), 40.0, 300.0);
            ++clockTickCount_;
            if (clockTickCount_ > 12) {
                const double current = tempo_.load(std::memory_order_relaxed);
                tempo_.store(clampd(current * 0.82 + measured * 0.18, 40.0, 300.0),
                             std::memory_order_relaxed);
            }
        }
    }
    lastClockNanos_ = timestampNanos;

    if (!playing_.load(std::memory_order_relaxed)) return;

    clockPulse_ = (clockPulse_ + 1) % 24;
    if (clockPulse_ % 6 == 0) {
        const int step = (playheadStep_.load(std::memory_order_relaxed) + 1) % kNumSteps;
        advanceStep(step, timestampNanos);
    }
}

void Engine::handleStart(int64_t timestampNanos) {
    if (!clockReceive_.load(std::memory_order_relaxed)) return;
    playing_.store(true, std::memory_order_relaxed);
    resetRequested_.store(false, std::memory_order_relaxed);
    playheadStep_.store(0, std::memory_order_relaxed);
    sampleCounterInStep_ = 0.0;
    clockPulse_ = 0;
    clockTickCount_ = 0;
    lastPlaying_ = true;
    advanceStep(0, timestampNanos);
}

void Engine::handleStop() {
    if (!clockReceive_.load(std::memory_order_relaxed)) return;
    playing_.store(false, std::memory_order_relaxed);
    lastPlaying_ = false;
}

void Engine::handleContinue() {
    if (!clockReceive_.load(std::memory_order_relaxed)) return;
    playing_.store(true, std::memory_order_relaxed);
    lastPlaying_ = true;
}

void Engine::handleSongPosition(int positionSixteenths) {
    playheadStep_.store(((positionSixteenths % kNumSteps) + kNumSteps) % kNumSteps,
                        std::memory_order_relaxed);
    clockPulse_ = 0;
}

void Engine::emitOut(uint8_t status, uint8_t d1, uint8_t d2, int64_t timestampNanos) {
    const uint32_t head = outHead_.load(std::memory_order_relaxed);
    const uint32_t tail = outTail_.load(std::memory_order_acquire);
    if (head - tail >= outRing_.size()) return;
    auto& slot = outRing_[head % outRing_.size()];
    slot.status = status;
    slot.data1 = d1;
    slot.data2 = d2;
    slot.pad = 0;
    slot.timestampNanos = timestampNanos;
    outHead_.store(head + 1, std::memory_order_release);
}

void Engine::emitNote(int ch, float velocity, int64_t timestampNanos) {
    if (!midiOut_.load(std::memory_order_relaxed)) return;
    const int note = kChannelNote[static_cast<size_t>(ch)];
    const int vel = clampi(static_cast<int>(std::lround(velocity * 127.0f)), 1, 127);
    const int filter = midiChannel_.load(std::memory_order_relaxed);
    const int channel = filter >= 0 ? filter : 9;
    emitOut(static_cast<uint8_t>(kNoteOn | channel), static_cast<uint8_t>(note),
            static_cast<uint8_t>(vel), timestampNanos);
    emitOut(static_cast<uint8_t>(kNoteOff | channel), static_cast<uint8_t>(note), 0,
            timestampNanos + 50'000'000);
}

int Engine::pollMidiOut(uint8_t* records, int maxRecords) {
    if (records == nullptr || maxRecords <= 0) return 0;
    int count = 0;
    uint32_t tail = outTail_.load(std::memory_order_relaxed);
    const uint32_t head = outHead_.load(std::memory_order_acquire);
    while (tail != head && count < maxRecords) {
        const auto& ev = outRing_[tail % outRing_.size()];
        uint8_t* rec = records + static_cast<size_t>(count) * 16;
        rec[0] = ev.status;
        rec[1] = ev.data1;
        rec[2] = ev.data2;
        rec[3] = ev.pad;
        std::memcpy(rec + 8, &ev.timestampNanos, sizeof(int64_t));
        ++tail;
        ++count;
    }
    outTail_.store(tail, std::memory_order_release);
    return count;
}

void Engine::pushIn(const InEvent& event) {
    std::lock_guard<std::mutex> lock(inMutex_);
    const uint32_t head = inHead_.load(std::memory_order_relaxed);
    const uint32_t tail = inTail_.load(std::memory_order_acquire);
    if (head - tail >= inRing_.size()) return;
    inRing_[head % inRing_.size()] = event;
    inHead_.store(head + 1, std::memory_order_release);
}

bool Engine::popIn(InEvent& event) {
    const uint32_t tail = inTail_.load(std::memory_order_relaxed);
    const uint32_t head = inHead_.load(std::memory_order_acquire);
    if (tail == head) return false;
    event = inRing_[tail % inRing_.size()];
    inTail_.store(tail + 1, std::memory_order_release);
    return true;
}

void Engine::pushMidiMessage(uint8_t status, uint8_t data1, uint8_t data2, int64_t timestampNanos) {
    InEvent ev {};
    ev.kind = 0;
    ev.status = status;
    ev.data1 = data1;
    ev.data2 = data2;
    ev.timestampNanos = timestampNanos;
    pushIn(ev);
}

void Engine::trigger(int ch, float velocity, bool flam) {
    if (!validChannel(ch)) return;
    InEvent ev {};
    ev.kind = 1;
    ev.status = 0;
    ev.data1 = static_cast<uint8_t>(ch);
    ev.velocity = clampf(velocity, 0.05f, 1.2f);
    ev.data2 = flam ? 1 : 0;
    ev.timestampNanos = nowNanos();
    pushIn(ev);
}

void Engine::setPlaying(bool play) {
    if (play) {
        resetRequested_.store(true, std::memory_order_relaxed);
        playing_.store(true, std::memory_order_relaxed);
    } else {
        playing_.store(false, std::memory_order_relaxed);
    }
}

void Engine::setTempo(double bpm) {
    tempo_.store(clampd(bpm, 40.0, 300.0), std::memory_order_relaxed);
}

void Engine::setClockReceive(bool on) {
    clockReceive_.store(on, std::memory_order_relaxed);
    if (on) externalClockSeenNanos_.store(0, std::memory_order_relaxed);
}

bool Engine::isExternalClockActive() const {
    const int64_t seen = externalClockSeenNanos_.load(std::memory_order_relaxed);
    if (!clockReceive_.load(std::memory_order_relaxed) || seen == 0) return false;
    return (nowNanos() - seen) < 1'500'000'000LL;
}

void Engine::setChannelParam(int ch, int param, float value) {
    if (!validChannel(ch) || param < 0 || param >= kChannelParamCount) return;
    channelParams_[static_cast<size_t>(ch) * kChannelParamCount + static_cast<size_t>(param)]
        .store(clampf(value, 0.0f, 1.0f), std::memory_order_relaxed);
}

float Engine::getChannelParam(int ch, int param) const {
    if (!validChannel(ch) || param < 0 || param >= kChannelParamCount) return 0.0f;
    return channelParams_[static_cast<size_t>(ch) * kChannelParamCount + static_cast<size_t>(param)]
        .load(std::memory_order_relaxed);
}

void Engine::setMasterParam(int param, float value) {
    if (param < 0 || param >= kMasterParamCount) return;
    masterParams_[static_cast<size_t>(param)].store(clampf(value, 0.0f, 1.2f),
                                                     std::memory_order_relaxed);
}

float Engine::getMasterParam(int param) const {
    if (param < 0 || param >= kMasterParamCount) return 0.0f;
    return masterParams_[static_cast<size_t>(param)].load(std::memory_order_relaxed);
}

void Engine::setTone(int ch, int toneId) {
    if (!validChannel(ch)) return;
    toneIds_[ch].store(clampi(toneId, 0, kNumTones - 1), std::memory_order_relaxed);
}

void Engine::loadKit(int index) {
    const FirmwareBank* b = bank();
    if (b == nullptr) return;
    index = clampi(index, 0, kNumKits - 1);
    const auto& kit = b->kits[index];
    for (int ch = 0; ch < kNumChannels; ++ch) {
        const auto& cp = kit.channels[ch];
        const size_t base = static_cast<size_t>(ch) * kChannelParamCount;
        channelParams_[base + kTune].store(static_cast<float>(cp.tune) / 255.0f);
        channelParams_[base + kDecay].store(static_cast<float>(cp.decay) / 255.0f);
        channelParams_[base + kCtrl].store(static_cast<float>(cp.ctrl) / 255.0f);
        channelParams_[base + kLevel].store(1.0f);
        channelParams_[base + kPan].store(static_cast<float>(cp.pan) / 255.0f);
        channelParams_[base + kReverbSend].store(static_cast<float>(cp.revSend) / 255.0f);
        channelParams_[base + kDelaySend].store(static_cast<float>(cp.dlySend) / 255.0f);
        toneIds_[ch].store(clampi(static_cast<int>(cp.toneId), 0, kNumTones - 1));
    }
    kitIndex_.store(index, std::memory_order_relaxed);
}

void Engine::loadPattern(int index, bool alsoLoadKit) {
    const FirmwareBank* b = bank();
    if (b == nullptr) return;
    index = clampi(index, 0, kNumPatterns - 1);
    const auto& p = b->patterns[index];

    for (int v = 0; v < kNumVariations; ++v) {
        for (int t = 0; t < kNumTracks; ++t) {
            for (int s = 0; s < kNumSteps; ++s) {
                const uint16_t packed = static_cast<uint16_t>(p.variations[v].velocity[t][s]) |
                                        (static_cast<uint16_t>(p.variations[v].flags[t][s]) << 8);
                grid_[static_cast<size_t>(v)][static_cast<size_t>(t)][static_cast<size_t>(s)]
                    .store(packed, std::memory_order_relaxed);
            }
        }
    }

    tempo_.store(clampd(static_cast<double>(p.tempoTenths) / 10.0, 40.0, 300.0),
                 std::memory_order_relaxed);
    patternIndex_.store(index, std::memory_order_relaxed);
    resetRequested_.store(true, std::memory_order_relaxed);
    playheadStep_.store(0, std::memory_order_relaxed);

    if (alsoLoadKit && p.kitNumber >= 1 && p.kitNumber <= kNumKits)
        loadKit(static_cast<int>(p.kitNumber) - 1);
}

void Engine::copyVariation(int src, int dst) {
    src = clampi(src, 0, kNumVariations - 1);
    dst = clampi(dst, 0, kNumVariations - 1);
    if (src == dst) return;
    for (int t = 0; t < kNumTracks; ++t)
        for (int s = 0; s < kNumSteps; ++s)
            grid_[static_cast<size_t>(dst)][static_cast<size_t>(t)][static_cast<size_t>(s)]
                .store(grid_[static_cast<size_t>(src)][static_cast<size_t>(t)][static_cast<size_t>(s)]
                           .load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
}

void Engine::clearVariation(int variation) {
    variation = clampi(variation, 0, kNumVariations - 1);
    for (int t = 0; t < kNumTracks; ++t)
        for (int s = 0; s < kNumSteps; ++s)
            grid_[static_cast<size_t>(variation)][static_cast<size_t>(t)][static_cast<size_t>(s)]
                .store(0, std::memory_order_relaxed);
}

void Engine::setStep(int variation, int track, int step, int velocity, int flags) {
    variation = clampi(variation, 0, kNumVariations - 1);
    track = clampi(track, 0, kNumTracks - 1);
    step = clampi(step, 0, kNumSteps - 1);
    const uint16_t packed = static_cast<uint16_t>(clampi(velocity, 0, 127)) |
                            (static_cast<uint16_t>(clampi(flags, 0, 255)) << 8);
    grid_[static_cast<size_t>(variation)][static_cast<size_t>(track)][static_cast<size_t>(step)]
        .store(packed, std::memory_order_relaxed);
}

int Engine::getStep(int variation, int track, int step) const {
    variation = clampi(variation, 0, kNumVariations - 1);
    track = clampi(track, 0, kNumTracks - 1);
    step = clampi(step, 0, kNumSteps - 1);
    return grid_[static_cast<size_t>(variation)][static_cast<size_t>(track)][static_cast<size_t>(step)]
        .load(std::memory_order_relaxed);
}

int Engine::getStepVelocity(int variation, int track, int step) const {
    return getStep(variation, track, step) & 0xFF;
}

void Engine::getPatternRow(int variation, int track, uint8_t* outStepVelFlags) const {
    if (outStepVelFlags == nullptr) return;
    variation = clampi(variation, 0, kNumVariations - 1);
    track = clampi(track, 0, kNumTracks - 1);
    for (int s = 0; s < kNumSteps; ++s) {
        const uint16_t packed = static_cast<uint16_t>(getStep(variation, track, s));
        outStepVelFlags[static_cast<size_t>(s) * 2] = static_cast<uint8_t>(packed & 0xFF);
        outStepVelFlags[static_cast<size_t>(s) * 2 + 1] = static_cast<uint8_t>((packed >> 8) & 0xFF);
    }
}

bool Engine::loadFirmware(const uint8_t* data, size_t size, std::string& error) {
    auto candidate = std::make_unique<FirmwareBank>();
    if (!parseFirmwareBytes(data, size, *candidate, error)) return false;

    {
        std::lock_guard<std::mutex> lock(bankMutex_);
        bankStorage_.push_back(std::move(candidate));
        bank_.store(bankStorage_.back().get(), std::memory_order_release);
    }
    const int kit = kitIndex_.load(std::memory_order_relaxed);
    const int pattern = patternIndex_.load(std::memory_order_relaxed);
    loadKit(kit);
    loadPattern(pattern, false);
    return true;
}

void Engine::clearFirmware() {
    auto preview = std::make_unique<FirmwareBank>();
    buildPreviewBank(*preview);
    {
        std::lock_guard<std::mutex> lock(bankMutex_);
        bankStorage_.push_back(std::move(preview));
        bank_.store(bankStorage_.back().get(), std::memory_order_release);
    }
    const int kit = kitIndex_.load(std::memory_order_relaxed);
    const int pattern = patternIndex_.load(std::memory_order_relaxed);
    loadKit(kit);
    loadPattern(pattern, false);
}

bool Engine::isFirmwareLoaded() const {
    const FirmwareBank* b = bank();
    return b != nullptr && b->imported;
}

std::string Engine::firmwareName() const {
    const FirmwareBank* b = bank();
    return b != nullptr ? std::string(b->name) : std::string("None");
}

std::string Engine::kitName(int index) const {
    const FirmwareBank* b = bank();
    if (b == nullptr) return "None";
    index = clampi(index, 0, kNumKits - 1);
    return std::string(b->kits[index].name);
}

std::string Engine::patternName(int index) const {
    const FirmwareBank* b = bank();
    if (b == nullptr) return "None";
    index = clampi(index, 0, kNumPatterns - 1);
    return std::string(b->patterns[index].name);
}

std::string Engine::patternCode(int index) const {
    const FirmwareBank* b = bank();
    if (b == nullptr) return "--";
    index = clampi(index, 0, kNumPatterns - 1);
    return std::string(b->patterns[index].code);
}

std::string Engine::toneName(int index) const {
    const FirmwareBank* b = bank();
    if (b == nullptr) return "None";
    index = clampi(index, 0, kNumTones - 1);
    return std::string(b->tones[index].name);
}

int Engine::kitToneId(int kitIndex, int channel) const {
    const FirmwareBank* b = bank();
    if (b == nullptr) return 0;
    kitIndex = clampi(kitIndex, 0, kNumKits - 1);
    channel = clampi(channel, 0, kNumChannels - 1);
    return clampi(static_cast<int>(b->kits[kitIndex].channels[channel].toneId), 0, kNumTones - 1);
}

void Engine::getParams(float* out) const {
    if (out == nullptr) return;
    std::fill(out, out + 96, 0.0f);
    for (int ch = 0; ch < kNumChannels; ++ch) {
        const size_t base = static_cast<size_t>(ch) * kChannelParamCount;
        for (int p = 0; p < kChannelParamCount; ++p)
            out[base + static_cast<size_t>(p)] =
                channelParams_[base + static_cast<size_t>(p)].load(std::memory_order_relaxed);
    }
    out[77] = static_cast<float>(tempo_.load(std::memory_order_relaxed));
    out[78] = shuffle_.load(std::memory_order_relaxed);
    out[79] = accent_.load(std::memory_order_relaxed);
    for (int p = 0; p < kMasterParamCount; ++p)
        out[80 + p] = masterParams_[static_cast<size_t>(p)].load(std::memory_order_relaxed);
}

void Engine::getStatus(int* out) const {
    if (out == nullptr) return;
    std::fill(out, out + 64, 0);
    out[0] = kitIndex_.load(std::memory_order_relaxed);
    out[1] = patternIndex_.load(std::memory_order_relaxed);
    out[2] = variation_.load(std::memory_order_relaxed);
    out[3] = selected_.load(std::memory_order_relaxed);
    out[4] = playing_.load(std::memory_order_relaxed) ? 1 : 0;
    out[5] = playheadStep_.load(std::memory_order_relaxed);
    out[6] = fill_.load(std::memory_order_relaxed) ? 1 : 0;
    out[7] = scatterActive_.load(std::memory_order_relaxed) ? 1 : 0;
    out[8] = scatterType_.load(std::memory_order_relaxed);
    out[9] = isFirmwareLoaded() ? 1 : 0;
    out[10] = clockReceive_.load(std::memory_order_relaxed) ? 1 : 0;
    out[11] = clockSend_.load(std::memory_order_relaxed) ? 1 : 0;
    out[12] = transportSend_.load(std::memory_order_relaxed) ? 1 : 0;
    out[13] = midiOut_.load(std::memory_order_relaxed) ? 1 : 0;
    out[14] = isExternalClockActive() ? 1 : 0;
    out[15] = midiChannel_.load(std::memory_order_relaxed);
    for (int ch = 0; ch < kNumChannels; ++ch)
        out[16 + ch] = toneIds_[ch].load(std::memory_order_relaxed);
    int muteMask = 0;
    int soloMask = 0;
    for (int ch = 0; ch < kNumChannels; ++ch) {
        if (muted_[ch].load(std::memory_order_relaxed)) muteMask |= (1 << ch);
        if (soloed_[ch].load(std::memory_order_relaxed)) soloMask |= (1 << ch);
    }
    out[27] = muteMask;
    out[28] = soloMask;
    out[29] = kNumPatterns;
    out[30] = kNumKits;
    out[31] = kNumTones;
    out[32] = static_cast<int>(std::lround(tempo_.load(std::memory_order_relaxed) * 10.0));
}

void Engine::getScope(float* out128) const {
    if (out128 == nullptr) return;
    const int w = scopeWrite_.load(std::memory_order_relaxed);
    for (int i = 0; i < 128; ++i)
        out128[i] = scopeRing_[static_cast<size_t>((w + i) & 127)].load(std::memory_order_relaxed);
}

void Engine::getMeters(float* out13) const {
    if (out13 == nullptr) return;
    for (int ch = 0; ch < kNumChannels; ++ch)
        out13[ch] = meters_[ch].load(std::memory_order_relaxed);
    out13[11] = masterPeakL_.load(std::memory_order_relaxed);
    out13[12] = masterPeakR_.load(std::memory_order_relaxed);
}

} // namespace rt8
