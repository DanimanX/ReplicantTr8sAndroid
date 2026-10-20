#pragma once

#include <vector>

#include "dsp_common.h"
#include "reverb.h"

namespace rt8 {

class MasterBus {
public:
    void prepare(double newSampleRate, int maxBlockSize);
    void reset();

    void setReverbLevel(float v) { reverbLevel = clampf(v, 0.0f, 1.0f); }
    void setDelayLevel(float v) { delayLevel = clampf(v, 0.0f, 1.0f); }
    void setMasterFxFilter(float v);
    void setMasterDrive(float v) { masterDrive = clampf(v, 0.0f, 1.0f); }
    void setMasterVolume(float v) { masterVolume = clampf(v, 0.0f, 1.2f); }

    void setScatterActive(bool active) { scatterActive = active; }
    bool isScatterActive() const { return scatterActive; }
    void setScatterType(int type) { scatterType = clampi(type, 1, 8); }
    int getScatterType() const { return scatterType; }
    void setScatterDepth(float depth) { scatterDepth = clampf(depth, 0.0f, 1.0f); }
    float getScatterDepth() const { return scatterDepth; }

    void setTempoBpm(double bpm);

    void processFrame(float& mainL, float& mainR, float revSendL, float revSendR,
                      float dlySendL, float dlySendR, int currentStep);

    float getReverbLevel() const { return reverbLevel; }
    float getDelayLevel() const { return delayLevel; }
    float getFilter() const { return masterFilter; }
    float getDrive() const { return masterDrive; }
    float getVolume() const { return masterVolume; }

private:
    void updateFilterCoefficients();

    double sampleRate = 44100.0;
    double currentBpm = 128.0;

    float reverbLevel = 0.50f;
    float delayLevel = 0.45f;
    float masterFilter = 0.5f;
    float masterDrive = 0.10f;
    float masterVolume = 0.85f;

    bool scatterActive = false;
    int scatterType = 1;
    float scatterDepth = 0.5f;

    std::vector<float> delayBufL, delayBufR;
    int delayWritePos = 0;
    int delaySamples = 16537;

    Reverb reverb;

    // Stable topology-preserving (TPT) state-variable filter. The previous
    // Chamberlin SVF went unstable at high cutoffs, produced NaNs, and the
    // reverb/delay feedback loops then kept the whole bus full of NaNs.
    bool filterBypass = true;
    bool filterHighPass = false;
    float filterK = 0.8f;
    float filterA1 = 0.0f, filterA2 = 0.0f, filterA3 = 0.0f;
    float svfIc1L = 0.0f, svfIc2L = 0.0f;
    float svfIc1R = 0.0f, svfIc2R = 0.0f;

    std::vector<float> scatterBufL, scatterBufR;
    int scatterWritePos = 0;
    double scatterReadPhase = 0.0;
};

} // namespace rt8
