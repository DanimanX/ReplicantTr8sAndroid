#include "master_bus.h"

#include <algorithm>
#include <cmath>

namespace rt8 {

namespace {
constexpr float kPi = 3.14159265358979323846f;
} // namespace

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
    updateFilterCoefficients();
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
    svfIc1L = svfIc2L = svfIc1R = svfIc2R = 0.0f;
    reverb.reset();
}

void MasterBus::setTempoBpm(double bpm) {
    currentBpm = clampd(bpm, 40.0, 300.0);
    const double dottedEighthSec = (60.0 / currentBpm) * 0.75;
    delaySamples = clampi(static_cast<int>(dottedEighthSec * sampleRate), 256,
                          static_cast<int>(delayBufL.size()) - 4);
}

void MasterBus::setMasterFxFilter(float v) {
    v = clampf(v, 0.0f, 1.0f);
    if (std::abs(v - masterFilter) < 1e-6f) return;
    masterFilter = v;
    updateFilterCoefficients();
}

void MasterBus::updateFilterCoefficients() {
    // 0.5 is the bypass detent. Below it the filter is a low-pass sweep, above
    // it a high-pass sweep, matching the DJ-style master filter on the panel.
    filterBypass = std::abs(masterFilter - 0.5f) <= 0.02f;
    if (filterBypass) {
        svfIc1L = svfIc2L = svfIc1R = svfIc2R = 0.0f;
        return;
    }

    filterHighPass = masterFilter > 0.5f;
    float cutoffHz;
    if (filterHighPass) {
        const float t = clampf((masterFilter - 0.52f) / 0.48f, 0.0f, 1.0f);
        cutoffHz = 30.0f * std::pow(200.0f, t);                 // 30 Hz .. 6 kHz
    } else {
        const float t = clampf((0.48f - masterFilter) / 0.48f, 0.0f, 1.0f);
        cutoffHz = 18000.0f * std::pow(60.0f / 18000.0f, t);    // 18 kHz .. 60 Hz
    }

    const float sr = static_cast<float>(sampleRate);
    cutoffHz = clampf(cutoffHz, 20.0f, 0.45f * sr);

    // Cytomic / TPT state-variable filter: unconditionally stable for any
    // cutoff below Nyquist, unlike the Chamberlin form used before.
    const float g = std::tan(kPi * cutoffHz / sr);
    const float k = filterK;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    filterA1 = a1;
    filterA2 = g * a1;
    filterA3 = g * filterA2;
}

void MasterBus::processFrame(float& mainL, float& mainR, float revSendL, float revSendR,
                             float dlySendL, float dlySendR, int currentStep) {
    // Never let a non-finite value enter a feedback path: one NaN in the delay
    // or reverb buffer would keep the master bus silent/clipped forever.
    mainL = sanitize(mainL);
    mainR = sanitize(mainR);
    revSendL = sanitize(revSendL);
    revSendR = sanitize(revSendR);
    dlySendL = sanitize(dlySendL);
    dlySendR = sanitize(dlySendR);

    if (!delayBufL.empty()) {
        const int bufSize = static_cast<int>(delayBufL.size());
        const int readPosL = (delayWritePos - delaySamples + bufSize) % bufSize;
        const int readPosR = (delayWritePos - (delaySamples * 2 / 3) + bufSize) % bufSize;

        const float dlyL = delayBufL[static_cast<size_t>(readPosL)];
        const float dlyR = delayBufR[static_cast<size_t>(readPosR)];

        const float feedback = 0.36f + delayLevel * 0.28f;
        delayBufL[static_cast<size_t>(delayWritePos)] =
            flushDenormal(std::tanh(dlySendL + dlyR * feedback));
        delayBufR[static_cast<size_t>(delayWritePos)] =
            flushDenormal(std::tanh(dlySendR + dlyL * feedback));
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

    if (!filterBypass) {
        const float v3L = mainL - svfIc2L;
        const float v1L = filterA1 * svfIc1L + filterA2 * v3L;
        const float v2L = svfIc2L + filterA2 * svfIc1L + filterA3 * v3L;
        svfIc1L = flushDenormal(2.0f * v1L - svfIc1L);
        svfIc2L = flushDenormal(2.0f * v2L - svfIc2L);
        mainL = filterHighPass ? (mainL - filterK * v1L - v2L) : v2L;

        const float v3R = mainR - svfIc2R;
        const float v1R = filterA1 * svfIc1R + filterA2 * v3R;
        const float v2R = svfIc2R + filterA2 * svfIc1R + filterA3 * v3R;
        svfIc1R = flushDenormal(2.0f * v1R - svfIc1R);
        svfIc2R = flushDenormal(2.0f * v2R - svfIc2R);
        mainR = filterHighPass ? (mainR - filterK * v1R - v2R) : v2R;

        if (!isFiniteSample(svfIc1L) || !isFiniteSample(svfIc2L) ||
            !isFiniteSample(svfIc1R) || !isFiniteSample(svfIc2R)) {
            svfIc1L = svfIc2L = svfIc1R = svfIc2R = 0.0f;
            mainL = 0.0f;
            mainR = 0.0f;
        }
    }

    const float driveGain = 1.0f + masterDrive * 2.2f;
    mainL = std::tanh(mainL * driveGain) * masterVolume;
    mainR = std::tanh(mainR * driveGain) * masterVolume;

    if (!isFiniteSample(mainL) || !isFiniteSample(mainR)) {
        // Last-resort recovery: clear every feedback buffer instead of letting
        // a NaN alias through the delay/reverb/scatter tail forever.
        reset();
        mainL = 0.0f;
        mainR = 0.0f;
    }
    mainL = clampf(flushDenormal(mainL), -1.0f, 1.0f);
    mainR = clampf(flushDenormal(mainR), -1.0f, 1.0f);
}

} // namespace rt8
