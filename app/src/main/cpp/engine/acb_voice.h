#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dsp_common.h"
#include "firmware.h"

namespace rt8 {

struct SampleData {
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate = 48000.0;
    std::string name;
};

class AcbVoice {
public:
    static constexpr int kMaxMixerLevel = 150;
    static constexpr float kMaxMixerGain = 150.0f / 255.0f;

    AcbVoice() = default;

    void setBank(const FirmwareBank* bank) { bank_ = bank; }
    void prepare(double newSampleRate);
    void reset();

    void trigger(float velocity, float accentAmount, bool isFlam = false);
    void choke();
    void renderNextSample(float& outL, float& outR);

    void setChannelIndex(int idx) { channelIndex = idx; }
    void setToneId(uint16_t id);
    uint16_t getToneId() const { return currentToneId; }

    void setTune(float v) { tuneParam = clampf(v, 0.0f, 1.0f); }
    void setDecay(float v) { decayParam = clampf(v, 0.0f, 1.0f); }
    void setCtrl(float v) { ctrlParam = clampf(v, 0.0f, 1.0f); }
    void setLevel(float v) { levelParam = clampf(v, 0.0f, 1.0f); }
    void setPan(float v) { panParam = clampf(v, 0.0f, 1.0f); }

    float getTune() const { return tuneParam; }
    float getDecay() const { return decayParam; }
    float getCtrl() const { return ctrlParam; }
    float getLevel() const { return levelParam; }
    float getPan() const { return panParam; }

    void setSample(std::shared_ptr<const SampleData> sample);
    void clearSample();
    bool hasCustomSample() const { return sample_ != nullptr && sample_->left.size() > 1; }
    void setUseCustomSample(bool shouldUse) { if (sample_) useCustomSample = shouldUse; }
    void setSampleReverse(bool rev) { sampleReverse = rev; }
    bool isSampleReversed() const { return sampleReverse; }
    std::string getSampleName() const { return sample_ ? sample_->name : std::string(); }

    bool isActive() const { return active; }
    float getRecentPeak() const { return recentPeak.load(std::memory_order_relaxed); }
    void decayRecentPeak(float factor = 0.92f) {
        recentPeak.store(recentPeak.load(std::memory_order_relaxed) * factor, std::memory_order_relaxed);
    }

private:
    const FirmwareTone& tone() const;
    float synthesizeSample();
    float render808Model(int cat);
    float render909Model(int cat);
    float render707Or626Model(int cat, bool is707);
    float render727Model(int cat);
    float render606Model(int cat);
    float renderCR78Model(int cat);
    float renderFMModel(int cat);
    float renderPCMOrSynthModel(int cat);
    float renderUserSample();
    float nextWhiteNoise();

    const FirmwareBank* bank_ = nullptr;

    int channelIndex = 0;
    uint16_t currentToneId = 0;
    double sampleRate = 44100.0;

    float tuneParam = 0.5f;
    float decayParam = 0.5f;
    float ctrlParam = 0.5f;
    float levelParam = 1.0f;
    float panParam = 0.5f;

    bool active = false;
    float currentVelocity = 0.0f;
    float envTime = 0.0f;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;
    std::array<float, 6> metalPhases { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    float svfLp1 = 0.0f, svfBp1 = 0.0f;
    float svfLp2 = 0.0f, svfBp2 = 0.0f;
    float chokeGain = 1.0f;
    bool choking = false;

    bool flamPending = false;
    int flamCountdownSamples = 0;
    float flamVelocity = 0.0f;

    uint32_t rngState = 0x12345678u;

    std::shared_ptr<const SampleData> sample_;
    bool useCustomSample = false;
    bool sampleReverse = false;
    double samplePlayhead = 0.0;

    std::atomic<float> recentPeak { 0.0f };
};

} // namespace rt8
