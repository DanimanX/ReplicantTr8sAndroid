#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "acb_voice.h"
#include "dsp_common.h"
#include "firmware.h"
#include "master_bus.h"

namespace rt8 {

class Engine {
public:
    Engine();
    ~Engine() = default;

    void prepare(double newSampleRate, int maxBlockFrames);
    void process(float* interleavedStereo, int frames, int64_t blockStartNanos);
    void renderSilent(int frames);

    // Transport and performance
    void setPlaying(bool play);
    bool isPlaying() const { return playing_.load(std::memory_order_relaxed); }
    void setTempo(double bpm);
    double getTempo() const { return tempo_.load(std::memory_order_relaxed); }
    void setShuffle(float v) { shuffle_.store(clampf(v, -0.5f, 0.5f), std::memory_order_relaxed); }
    float getShuffle() const { return shuffle_.load(std::memory_order_relaxed); }
    void setAccent(float v) { accent_.store(clampf(v, 0.0f, 1.0f), std::memory_order_relaxed); }
    float getAccent() const { return accent_.load(std::memory_order_relaxed); }
    void setVariation(int v) { variation_.store(clampi(v, 0, kNumVariations - 1), std::memory_order_relaxed); }
    int getVariation() const { return variation_.load(std::memory_order_relaxed); }
    void setFill(bool on) { fill_.store(on, std::memory_order_relaxed); }
    void toggleFill() { fill_.store(!fill_.load(std::memory_order_relaxed), std::memory_order_relaxed); }
    bool isFill() const { return fill_.load(std::memory_order_relaxed); }
    void setSelectedChannel(int ch) { selected_.store(clampi(ch, 0, kNumChannels - 1), std::memory_order_relaxed); }
    int getSelectedChannel() const { return selected_.load(std::memory_order_relaxed); }
    int getCurrentStep() const { return playheadStep_.load(std::memory_order_relaxed); }

    // Channel + master parameters
    void setChannelParam(int ch, int param, float value);
    float getChannelParam(int ch, int param) const;
    void setMasterParam(int param, float value);
    float getMasterParam(int param) const;
    void setMute(int ch, bool muted) { if (validChannel(ch)) muted_[ch].store(muted, std::memory_order_relaxed); }
    void setSolo(int ch, bool soloed) { if (validChannel(ch)) soloed_[ch].store(soloed, std::memory_order_relaxed); }
    bool isMuted(int ch) const { return validChannel(ch) && muted_[ch].load(std::memory_order_relaxed); }
    bool isSoloed(int ch) const { return validChannel(ch) && soloed_[ch].load(std::memory_order_relaxed); }
    void setTone(int ch, int toneId);

    // Pattern + kit management
    void loadKit(int index);
    void loadPattern(int index, bool alsoLoadKit);
    int getKitIndex() const { return kitIndex_.load(std::memory_order_relaxed); }
    int getPatternIndex() const { return patternIndex_.load(std::memory_order_relaxed); }
    void copyVariation(int src, int dst);
    void clearVariation(int variation);
    void setStep(int variation, int track, int step, int velocity, int flags = 0);
    int getStep(int variation, int track, int step) const;
    void getPatternRow(int variation, int track, uint8_t* outStepVelFlags) const;

    // Live pads
    void trigger(int ch, float velocity, bool flam = false);

    // MIDI
    void pushMidiMessage(uint8_t status, uint8_t data1, uint8_t data2, int64_t timestampNanos);
    int pollMidiOut(uint8_t* records, int maxRecords);
    void setMidiOutEnabled(bool on) { midiOut_.store(on, std::memory_order_relaxed); }
    bool isMidiOutEnabled() const { return midiOut_.load(std::memory_order_relaxed); }
    void setClockSend(bool on) { clockSend_.store(on, std::memory_order_relaxed); }
    bool isClockSend() const { return clockSend_.load(std::memory_order_relaxed); }
    void setClockReceive(bool on);
    bool isClockReceive() const { return clockReceive_.load(std::memory_order_relaxed); }
    void setTransportSend(bool on) { transportSend_.store(on, std::memory_order_relaxed); }
    bool isTransportSend() const { return transportSend_.load(std::memory_order_relaxed); }
    void setMidiChannel(int channel) { midiChannel_.store(channel < 0 ? -1 : clampi(channel, 0, 15), std::memory_order_relaxed); }
    int getMidiChannel() const { return midiChannel_.load(std::memory_order_relaxed); }
    bool isExternalClockActive() const;

    // Firmware
    bool loadFirmware(const uint8_t* data, size_t size, std::string& error);
    void clearFirmware();
    bool isFirmwareLoaded() const;
    std::string firmwareName() const;

    // Names (thread-safe; reads the immutable published bank)
    std::string kitName(int index) const;
    std::string patternName(int index) const;
    std::string patternCode(int index) const;
    std::string toneName(int index) const;
    int kitToneId(int kitIndex, int channel) const;
    std::string currentKitName() const { return kitName(getKitIndex()); }
    std::string currentPatternName() const { return patternName(getPatternIndex()); }

    // UI snapshots
    void getParams(float* out) const;      // 96 floats
    void getStatus(int* out) const;        // 64 ints
    void getScope(float* out128) const;
    void getMeters(float* out13) const;
    int getStepVelocity(int variation, int track, int step) const;

    // Scatter / master FX
    void setScatterActive(bool on) { scatterActive_.store(on, std::memory_order_relaxed); }
    void setScatterType(int type) { scatterType_.store(clampi(type, 1, 8), std::memory_order_relaxed); }
    bool isScatterActive() const { return scatterActive_.load(std::memory_order_relaxed); }
    int getScatterType() const { return scatterType_.load(std::memory_order_relaxed); }

    double getSampleRate() const { return sampleRate_; }
    int getMaxBlockFrames() const { return maxBlockFrames_; }

private:
    struct InEvent {
        uint8_t kind = 0; // 0 = MIDI, 1 = pad
        uint8_t status = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        float velocity = 0.0f;
        int64_t timestampNanos = 0;
    };

    struct OutEvent {
        uint8_t status = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        uint8_t pad = 0;
        int64_t timestampNanos = 0;
    };

    static bool validChannel(int ch) { return ch >= 0 && ch < kNumChannels; }
    const FirmwareBank* bank() const { return bank_.load(std::memory_order_acquire); }
    bool isAudible(int ch) const;
    void applyBlockConfig(const FirmwareBank* bank);
    void advanceStep(int step, int64_t timestampNanos);
    void triggerVoice(int ch, float velocity, bool flam, int64_t timestampNanos, float accentOverride = -1.0f);
    void handleEvent(const InEvent& event, int64_t timestampNanos);
    void handleClock(int64_t timestampNanos);
    void handleStart(int64_t timestampNanos);
    void handleStop();
    void handleContinue();
    void handleSongPosition(int positionSixteenths);
    void emitOut(uint8_t status, uint8_t d1, uint8_t d2, int64_t timestampNanos);
    void emitNote(int ch, float velocity, int64_t timestampNanos);
    void pushIn(const InEvent& event);
    bool popIn(InEvent& event);
    static int64_t nowNanos();

    double sampleRate_ = 48000.0;
    int maxBlockFrames_ = 512;

    std::array<AcbVoice, kNumChannels> voices_ {};
    MasterBus masterBus_;

    // Immutable firmware banks. Old banks are retained so an in-flight audio
    // block can finish safely after a hot-swap.
    std::atomic<const FirmwareBank*> bank_ { nullptr };
    mutable std::mutex bankMutex_;
    std::vector<std::unique_ptr<FirmwareBank>> bankStorage_;

    // Pattern grid, packed as (flags << 8) | velocity. Every cell is atomic so
    // the UI can edit while audio is running without a lock.
    std::array<std::array<std::array<std::atomic<uint16_t>, kNumSteps>, kNumTracks>, kNumVariations> grid_ {};

    std::array<std::atomic<float>, kNumChannels * kChannelParamCount> channelParams_ {};
    std::array<std::atomic<int>, kNumChannels> toneIds_ {};
    std::array<std::atomic<bool>, kNumChannels> muted_ {};
    std::array<std::atomic<bool>, kNumChannels> soloed_ {};
    std::array<std::atomic<float>, kMasterParamCount> masterParams_ {};

    std::atomic<int> kitIndex_ { 1 };
    std::atomic<int> patternIndex_ { 103 };
    std::atomic<int> variation_ { 0 };
    std::atomic<int> selected_ { 0 };
    std::atomic<double> tempo_ { 128.0 };
    std::atomic<float> shuffle_ { 0.0f };
    std::atomic<float> accent_ { 0.65f };
    std::atomic<bool> playing_ { false };
    std::atomic<bool> fill_ { false };
    std::atomic<bool> resetRequested_ { false };
    std::atomic<bool> scatterActive_ { false };
    std::atomic<int> scatterType_ { 1 };

    std::atomic<bool> midiOut_ { true };
    std::atomic<bool> clockSend_ { true };
    std::atomic<bool> clockReceive_ { false };
    std::atomic<bool> transportSend_ { true };
    std::atomic<int> midiChannel_ { 9 };

    std::atomic<int> playheadStep_ { 0 };
    std::array<std::atomic<float>, kNumChannels> meters_ {};
    std::atomic<float> masterPeakL_ { 0.0f };
    std::atomic<float> masterPeakR_ { 0.0f };
    std::array<std::atomic<float>, 128> scopeRing_ {};
    std::atomic<int> scopeWrite_ { 0 };

    // MIDI in ring (multiple producers, one consumer = audio thread).
    mutable std::mutex inMutex_;
    std::array<InEvent, 512> inRing_ {};
    std::atomic<uint32_t> inHead_ { 0 };
    std::atomic<uint32_t> inTail_ { 0 };

    // MIDI out ring (one producer = audio thread, one consumer = JNI).
    std::array<OutEvent, 2048> outRing_ {};
    std::atomic<uint32_t> outHead_ { 0 };
    std::atomic<uint32_t> outTail_ { 0 };

    // Audio-thread state.
    double sampleCounterInStep_ = 0.0;
    double samplesUntilClock_ = 0.0;
    int clockPulse_ = 0;
    int clockTickCount_ = 0;
    int64_t lastClockNanos_ = 0;
    std::atomic<int64_t> externalClockSeenNanos_ { 0 };
    bool lastPlaying_ = false;
    bool wasClockReceive_ = false;
    int lastToneApplied_[kNumChannels] { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::unique_ptr<float[]> silentBuffer_;
    int silentBufferFrames_ = 0;
};

} // namespace rt8
