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
    void setMasterFxFilter(float v) { masterFilter = clampf(v, 0.0f, 1.0f); }
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

    float svfLpL = 0.0f, svfBpL = 0.0f;
    float svfLpR = 0.0f, svfBpR = 0.0f;

    std::vector<float> scatterBufL, scatterBufR;
    int scatterWritePos = 0;
    double scatterReadPhase = 0.0;
};

} // namespace rt8
