#include "master_bus.h"

#include <algorithm>
#include <cmath>

namespace rt8 {

void MasterBus::prepare(double newSampleRate, int /*maxBlockSize*/) {
    sampleRate = std::max(8000.0, newSampleRate);
    const int maxDelayLen = static_cast<int>(sampleRate * 2.5) + 1024;
    delayBufL.assign(static_cast<size_t>(maxDelayLen), 0.0f);
    delayBufR.assign(static_cast<size_t>(maxDelayLen), 0.0f);

    const int maxScatterLen = static_cast<int>(sampleRate * 3.0) + 1024;
    scatterBufL.assign(static_cast<size_t>(maxScatterLen), 0.0f);
    scatterBufR.assign(static_cast<size_t>(maxScatterLen), 0.0f);

    reverb.prepare(sampleRate);
    reverb.setParameters(0.68f, 0.36f, 0.96f);
    setTempoBpm(currentBpm);
    reset();
}

void MasterBus::reset() {
    std::fill(delayBufL.begin(), delayBufL.end(), 0.0f);
    std::fill(delayBufR.begin(), delayBufR.end(), 0.0f);
    std::fill(scatterBufL.begin(), scatterBufL.end(), 0.0f);
    std::fill(scatterBufR.begin(), scatterBufR.end(), 0.0f);
    delayWritePos = 0;
    scatterWritePos = 0;
    scatterReadPhase = 0.0;
    svfLpL = svfBpL = svfLpR = svfBpR = 0.0f;
    reverb.reset();
}

void MasterBus::setTempoBpm(double bpm) {
    currentBpm = clampd(bpm, 40.0, 300.0);
    const double dottedEighthSec = (60.0 / currentBpm) * 0.75;
    delaySamples = clampi(static_cast<int>(dottedEighthSec * sampleRate), 256,
                          static_cast<int>(delayBufL.size()) - 4);
}

void MasterBus::processFrame(float& mainL, float& mainR, float revSendL, float revSendR,
                             float dlySendL, float dlySendR, int currentStep) {
    if (!delayBufL.empty()) {
        const int bufSize = static_cast<int>(delayBufL.size());
        const int readPosL = (delayWritePos - delaySamples + bufSize) % bufSize;
        const int readPosR = (delayWritePos - (delaySamples * 2 / 3) + bufSize) % bufSize;

        const float dlyL = delayBufL[static_cast<size_t>(readPosL)];
        const float dlyR = delayBufR[static_cast<size_t>(readPosR)];

        const float feedback = 0.36f + delayLevel * 0.28f;
        delayBufL[static_cast<size_t>(delayWritePos)] = std::tanh(dlySendL + dlyR * feedback);
        delayBufR[static_cast<size_t>(delayWritePos)] = std::tanh(dlySendR + dlyL * feedback);
        delayWritePos = (delayWritePos + 1) % bufSize;

        const float dlyGain = (delayLevel > 0.002f) ? (0.35f + delayLevel * 0.75f) : 0.0f;
        mainL += dlyL * dlyGain;
        mainR += dlyR * dlyGain;
    }

    if (reverbLevel > 0.002f) {
        float rl = revSendL;
        float rr = revSendR;
        reverb.process(rl, rr);
        const float revGain = 0.35f + reverbLevel * 0.85f;
        mainL += rl * revGain;
        mainR += rr * revGain;
    }

    if (!scatterBufL.empty()) {
        const int sSize = static_cast<int>(scatterBufL.size());
        scatterBufL[static_cast<size_t>(scatterWritePos)] = mainL;
        scatterBufR[static_cast<size_t>(scatterWritePos)] = mainR;

        if (scatterActive) {
            const double stepSamples = std::max(512.0, (60.0 / currentBpm) * 0.25 * sampleRate);
            const double loopWindow = stepSamples * (scatterDepth > 0.65f ? 0.5 : 1.0);
            scatterReadPhase += 1.0;
            if (scatterReadPhase >= loopWindow) scatterReadPhase = 0.0;

            if (scatterType == 1) {
                const int rIdx = (scatterWritePos - static_cast<int>(scatterReadPhase) + sSize) % sSize;
                mainL = scatterBufL[static_cast<size_t>(rIdx)];
                mainR = scatterBufR[static_cast<size_t>(rIdx)];
            } else if (scatterType == 2) {
                const bool gateOn = (std::fmod(scatterReadPhase, loopWindow * 0.5) < loopWindow * 0.25);
                const float g = gateOn ? 1.0f : (1.0f - scatterDepth);
                mainL *= g;
                mainR *= g;
            } else if (scatterType == 3 || scatterType == 4) {
                const int rIdx = (scatterWritePos - static_cast<int>(loopWindow - scatterReadPhase) + sSize) % sSize;
                mainL = scatterBufL[static_cast<size_t>(rIdx)];
                mainR = scatterBufR[static_cast<size_t>(rIdx)];
            } else if (scatterType == 5) {
                const int rIdx = (scatterWritePos - static_cast<int>(scatterReadPhase * 0.5) + sSize) % sSize;
                mainL = scatterBufL[static_cast<size_t>(rIdx)];
                mainR = scatterBufR[static_cast<size_t>(rIdx)];
            } else {
                const float panMod = (currentStep & 1) ? 0.2f : 0.8f;
                const float q = 4.0f + (1.0f - scatterDepth) * 12.0f;
                mainL = (std::round(mainL * q) / q) * (1.2f - panMod);
                mainR = (std::round(mainR * q) / q) * (0.2f + panMod);
            }
        } else {
            scatterReadPhase = 0.0;
        }
        scatterWritePos = (scatterWritePos + 1) % sSize;
    }

    if (std::abs(masterFilter - 0.5f) > 0.02f) {
        const bool isHpf = (masterFilter > 0.5f);
        float normFreq = isHpf ? ((masterFilter - 0.52f) / 0.48f) : (masterFilter / 0.48f);
        normFreq = clampf(normFreq, 0.0f, 1.0f);
        const float cutoffHz = isHpf ? (30.0f * std::pow(180.0f, normFreq))
                                     : (80.0f * std::pow(220.0f, normFreq));
        const float f = 2.0f * std::sin(3.14159265358979323846f * std::min(0.44f, cutoffHz / static_cast<float>(sampleRate)));
        const float q = 0.52f;

        const float hpL = mainL - svfLpL - q * svfBpL;
        svfBpL += f * hpL;
        svfLpL += f * svfBpL;

        const float hpR = mainR - svfLpR - q * svfBpR;
        svfBpR += f * hpR;
        svfLpR += f * svfBpR;

        mainL = isHpf ? hpL : svfLpL;
        mainR = isHpf ? hpR : svfLpR;
    }

    const float driveGain = 1.0f + masterDrive * 2.2f;
    mainL = std::tanh(mainL * driveGain) * masterVolume;
    mainR = std::tanh(mainR * driveGain) * masterVolume;
}

} // namespace rt8
