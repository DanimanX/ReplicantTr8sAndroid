#include "acb_voice.h"

#include <algorithm>
#include <cmath>

namespace rt8 {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kPi = 3.14159265358979323846f;
const FirmwareTone kFallbackTone {0, "Tone", 1, 1};
} // namespace

void AcbVoice::prepare(double newSampleRate) {
    sampleRate = std::max(8000.0, newSampleRate);
    reset();
}

void AcbVoice::reset() {
    active = false;
    envTime = 0.0f;
    phase1 = phase2 = phase3 = 0.0f;
    metalPhases.fill(0.0f);
    svfLp1 = svfBp1 = svfLp2 = svfBp2 = 0.0f;
    chokeGain = 1.0f;
    choking = false;
    flamPending = false;
}

const FirmwareTone& AcbVoice::tone() const {
    if (bank_ != nullptr && currentToneId < kNumTones) return bank_->tones[currentToneId];
    return kFallbackTone;
}

void AcbVoice::setToneId(uint16_t id) {
    currentToneId = std::min<uint16_t>(id, kNumTones - 1);
}

float AcbVoice::nextWhiteNoise() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(static_cast<int32_t>(rngState)) * (1.0f / 2147483648.0f);
}

void AcbVoice::trigger(float velocity, float accentAmount, bool isFlam) {
    const float boostedVel = clampf(velocity * (1.0f + accentAmount * 0.45f), 0.05f, 1.45f);
    currentVelocity = boostedVel;
    envTime = 0.0f;
    phase1 = 0.0f;
    phase2 = 0.0f;
    phase3 = 0.0f;
    svfLp1 = svfBp1 = svfLp2 = svfBp2 = 0.0f;
    chokeGain = 1.0f;
    choking = false;
    active = true;

    if (useCustomSample && sample_ && !sample_->left.empty())
        samplePlayhead = sampleReverse ? static_cast<double>(sample_->left.size() - 1) : 0.0;

    if (isFlam) {
        flamPending = true;
        flamCountdownSamples = static_cast<int>(sampleRate * 0.024);
        flamVelocity = std::min(1.45f, boostedVel * 1.12f);
    } else {
        flamPending = false;
    }

    recentPeak.store(std::min(1.0f, boostedVel), std::memory_order_relaxed);
}

void AcbVoice::choke() {
    if (active) choking = true;
}

void AcbVoice::renderNextSample(float& outL, float& outR) {
    if (flamPending) {
        if (--flamCountdownSamples <= 0) {
            flamPending = false;
            trigger(flamVelocity, 0.0f, false);
        }
    }

    if (!active) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    float mono = 0.0f;
    if (useCustomSample && sample_ && !sample_->left.empty())
        mono = renderUserSample();
    else
        mono = synthesizeSample();

    if (choking) {
        chokeGain *= 0.965f;
        mono *= chokeGain;
        if (chokeGain < 0.001f) {
            active = false;
            choking = false;
        }
    }

    mono *= currentVelocity * (levelParam * kMaxMixerGain);
    mono = std::tanh(mono * 1.15f);
    if (!isFiniteSample(mono)) {
        active = false;
        mono = 0.0f;
    }
    mono = flushDenormal(mono);

    const float panAngle = clampf(panParam, 0.0f, 1.0f) * (kPi * 0.5f);
    outL = mono * std::cos(panAngle);
    outR = mono * std::sin(panAngle);

    envTime += 1.0f / static_cast<float>(sampleRate);
    if (envTime > 4.5f) active = false;
}

float AcbVoice::synthesizeSample() {
    const auto& t = tone();
    const int cat = t.category;
    const uint16_t id = t.id;

    if (id <= 25 || (id >= 81 && id <= 83) || id == 158) return render808Model(cat);
    if ((id >= 26 && id <= 41) || id == 84 || id == 85 || id == 157) return render909Model(cat);
    if (id >= 42 && id <= 56) return render707Or626Model(cat, true);
    if (id >= 57 && id <= 71) return render727Model(cat);
    if (id >= 72 && id <= 80) return render606Model(cat);
    if (id >= 86 && id <= 156) return renderFMModel(cat);
    if (id >= 159 && id <= 173) return renderCR78Model(cat);
    if (id >= 181 && id <= 354) return render707Or626Model(cat, false);
    return renderPCMOrSynthModel(cat);
}

float AcbVoice::render808Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.2f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    if (cat == 1 || currentToneId == 158) {
        const bool isLong = (currentToneId == 2 || currentToneId == 3 || currentToneId == 158);
        const float baseFreq = (currentToneId == 158 ? 55.0f : 48.0f) * tuneRatio;
        const float decayTime = (isLong ? 0.95f : 0.42f) * decayScale;
        const float pitchEnv = std::exp(-envTime * 68.0f);
        const float subDrift = std::exp(-envTime * 4.5f);
        const float freq = baseFreq * (0.94f + 0.12f * subDrift + (1.6f + ctrlParam * 2.2f) * pitchEnv);

        phase1 += freq * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float ampEnv = std::exp(-envTime / std::max(0.04f, decayTime));
        if (ampEnv < 0.0005f) { active = false; return 0.0f; }

        const float body = std::sin(phase1 * kTwoPi) + 0.08f * std::sin(phase1 * kTwoPi * 2.0f);
        const float click = std::exp(-envTime * 420.0f) * (0.25f + ctrlParam * 0.65f);
        return (body * ampEnv + click) * 1.15f;
    }

    if (cat == 2) {
        const float f1 = 180.0f * tuneRatio * (1.0f + 0.45f * std::exp(-envTime * 95.0f));
        const float f2 = 330.0f * tuneRatio * (1.0f + 0.35f * std::exp(-envTime * 95.0f));
        phase1 += f1 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += f2 * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float toneEnv = std::exp(-envTime * (15.0f / decayScale));
        const float noiseEnv = std::exp(-envTime * (9.5f / decayScale));
        if (toneEnv < 0.0005f && noiseEnv < 0.0005f) { active = false; return 0.0f; }

        const float shells = (std::sin(phase1 * kTwoPi) * 0.62f + std::sin(phase2 * kTwoPi) * 0.38f) * toneEnv;
        const float rawNoise = nextWhiteNoise();
        const float cutoff = std::min(0.45f, 2400.0f / static_cast<float>(sampleRate));
        svfLp1 += cutoff * rawNoise - cutoff * svfLp1;
        const float hpNoise = (rawNoise - svfLp1) * noiseEnv * (0.25f + ctrlParam * 1.15f);
        return (shells * 0.75f + hpNoise * 0.7f);
    }

    if (cat == 3 || cat == 8) {
        float baseFreq = 110.0f;
        if (channelIndex == 2) baseFreq = (cat == 8) ? 165.0f : 98.0f;
        else if (channelIndex == 3) baseFreq = (cat == 8) ? 245.0f : 142.0f;
        else if (channelIndex == 4) baseFreq = (cat == 8) ? 355.0f : 205.0f;

        baseFreq *= tuneRatio;
        const float pitchSweep = 1.0f + (cat == 8 ? 0.25f : 0.55f) * std::exp(-envTime * 32.0f);
        phase1 += (baseFreq * pitchSweep) * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float decayRate = (cat == 8 ? 9.5f : 6.5f) / decayScale;
        const float ampEnv = std::exp(-envTime * decayRate);
        if (ampEnv < 0.0005f) { active = false; return 0.0f; }

        const float pinkNoise = nextWhiteNoise() * std::exp(-envTime * 55.0f) * (0.1f + ctrlParam * 0.35f);
        return (std::sin(phase1 * kTwoPi) * ampEnv + pinkNoise) * 0.95f;
    }

    if (cat == 4 || cat == 9) {
        if (cat == 9 || currentToneId == 16) {
            const float f = 2480.0f * tuneRatio;
            phase1 += f * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
            const float env = std::exp(-envTime * (38.0f / decayScale));
            if (env < 0.0005f) { active = false; return 0.0f; }
            return std::sin(phase1 * kTwoPi) * env * 0.9f;
        }
        phase1 += 470.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 1720.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float env1 = std::exp(-envTime * (55.0f / decayScale));
        const float env2 = std::exp(-envTime * (85.0f / decayScale));
        if (env1 < 0.0005f) { active = false; return 0.0f; }
        const float sig = std::sin(phase1 * kTwoPi) * env1 * 0.6f + std::sin(phase2 * kTwoPi) * env2 * 0.85f;
        return std::tanh(sig * (1.8f + ctrlParam * 1.5f));
    }

    if (cat == 5 || cat == 10) {
        if (cat == 10 || currentToneId == 18) {
            const float attack = std::min(1.0f, envTime * 140.0f);
            const float env = attack * std::exp(-envTime * (36.0f / decayScale));
            if (envTime > 0.02f && env < 0.0005f) { active = false; return 0.0f; }
            const float n = nextWhiteNoise();
            svfLp1 += 0.45f * (n - svfLp1);
            return (n - svfLp1) * env * 0.85f;
        }
        float env = 0.0f;
        if (envTime < 0.033f) {
            const float burstPhase = std::fmod(envTime, 0.011f) / 0.011f;
            env = std::exp(-burstPhase * 4.2f);
        } else {
            env = 0.65f * std::exp(-(envTime - 0.033f) * (7.5f / decayScale));
        }
        if (envTime > 0.04f && env < 0.0005f) { active = false; return 0.0f; }

        const float n = nextWhiteNoise();
        const float f = 2.0f * std::sin(kPi * std::min(0.16f, (1150.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float q = 0.22f;
        const float hp = n - svfLp1 - q * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        return svfBp1 * env * (1.5f + ctrlParam * 0.9f);
    }

    if (cat == 11 || currentToneId == 25) {
        phase1 += 540.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 800.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float sq1 = (phase1 < 0.5f) ? 1.0f : -1.0f;
        const float sq2 = (phase2 < 0.5f) ? 1.0f : -1.0f;
        const float env = 0.65f * std::exp(-envTime * 35.0f) + 0.35f * std::exp(-envTime * (5.5f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        const float raw = (sq1 + sq2) * 0.5f;
        const float f = 2.0f * std::sin(kPi * std::min(0.16f, (880.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = raw - svfLp1 - 0.35f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        return svfBp1 * env * 1.4f;
    }

    static constexpr float freqs808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    float metalSum = 0.0f;
    for (size_t i = 0; i < 6; ++i) {
        metalPhases[i] += (freqs808[i] * tuneRatio * 3.8f) * dt;
        if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
        metalSum += (metalPhases[i] < 0.5f) ? 1.0f : -1.0f;
    }
    metalSum *= (1.0f / 6.0f);

    const bool isOpen = (channelIndex == 8 || currentToneId == 22);
    const bool isCymbal = (cat == 7 || channelIndex >= 9);
    const float decayRate = isCymbal ? (1.8f / decayScale) : (isOpen ? (4.2f / decayScale) : (26.0f / decayScale));
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0005f) { active = false; return 0.0f; }

    const float f = 2.0f * std::sin(kPi * std::min(0.16f, (6200.0f * tuneRatio) / static_cast<float>(sampleRate)));
    const float hp = metalSum - svfLp1 - 0.5f * svfBp1;
    svfBp1 += f * hp;
    svfLp1 += f * svfBp1;
    return (hp * 0.7f + svfBp1 * 0.5f) * env * (0.9f + ctrlParam * 0.5f);
}

float AcbVoice::render909Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.1f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    if (cat == 1 || currentToneId == 157) {
        const float baseFreq = 52.0f * tuneRatio;
        const float fastEnv = std::exp(-envTime * 95.0f);
        const float medEnv = std::exp(-envTime * 19.0f);
        const float freq = baseFreq + (210.0f * fastEnv + 58.0f * medEnv) * tuneRatio;

        phase1 += freq * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float tri = 4.0f * std::abs(phase1 - 0.5f) - 1.0f;
        const float shapedSine = std::sin(tri * (kPi * 0.5f));

        const float ampEnv = std::exp(-envTime * (2.6f / decayScale));
        if (ampEnv < 0.0005f) { active = false; return 0.0f; }

        const float clickEnv = std::exp(-envTime * 340.0f);
        const float click = (clickEnv * 0.85f + nextWhiteNoise() * std::exp(-envTime * 480.0f) * 0.55f) *
                            (0.2f + ctrlParam * 1.25f);
        return std::tanh((shapedSine * ampEnv + click) * 1.45f);
    }

    if (cat == 2) {
        const float sweep = 1.0f + 0.65f * std::exp(-envTime * 85.0f);
        phase1 += 185.0f * tuneRatio * sweep * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 342.0f * tuneRatio * sweep * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float bodyEnv = std::exp(-envTime * (16.0f / decayScale));
        const float snapEnv = 0.6f * std::exp(-envTime * (22.0f / decayScale)) +
                              0.4f * std::exp(-envTime * (7.5f / decayScale));
        if (bodyEnv < 0.0005f && snapEnv < 0.0005f) { active = false; return 0.0f; }

        const float body = (std::sin(phase1 * kTwoPi) * 0.65f + std::sin(phase2 * kTwoPi) * 0.35f) * bodyEnv;
        const float n = nextWhiteNoise();
        svfLp1 += 0.25f * (n - svfLp1);
        const float snappy = (n - svfLp1 * 0.6f) * snapEnv * (0.25f + ctrlParam * 1.35f);
        return std::tanh((body * 0.85f + snappy * 0.75f) * 1.25f);
    }

    if (cat == 3) {
        float baseFreq = (channelIndex == 2) ? 92.0f : (channelIndex == 3 ? 132.0f : 185.0f);
        baseFreq *= tuneRatio;
        const float freq = baseFreq * (1.0f + 0.85f * std::exp(-envTime * 28.0f));
        phase1 += freq * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        const float ampEnv = std::exp(-envTime * (5.8f / decayScale));
        if (ampEnv < 0.0005f) { active = false; return 0.0f; }
        const float click = nextWhiteNoise() * std::exp(-envTime * 160.0f) * (0.15f + ctrlParam * 0.45f);
        return (std::sin(phase1 * kTwoPi) * ampEnv + click) * 1.05f;
    }

    if (cat == 4) {
        phase1 += 250.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 495.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        phase3 += 1580.0f * tuneRatio * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;
        const float env = std::exp(-envTime * (65.0f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        const float mix = std::sin(phase1 * kTwoPi) * 0.4f + std::sin(phase2 * kTwoPi) * 0.6f +
                          std::sin(phase3 * kTwoPi) * 0.9f;
        return std::tanh(mix * env * (2.2f + ctrlParam * 1.4f));
    }

    if (cat == 5) {
        float env = 0.0f;
        if (envTime < 0.030f) {
            const float b = std::fmod(envTime, 0.010f) / 0.010f;
            env = std::exp(-b * 3.8f);
        } else {
            env = 0.72f * std::exp(-(envTime - 0.030f) * (6.8f / decayScale));
        }
        if (envTime > 0.04f && env < 0.0005f) { active = false; return 0.0f; }
        const float n = nextWhiteNoise();
        const float f = 2.0f * std::sin(kPi * std::min(0.16f, (1320.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = n - svfLp1 - 0.28f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        return std::tanh(svfBp1 * env * (1.8f + ctrlParam * 1.0f));
    }

    static constexpr float freqs909[6] = { 317.0f, 466.0f, 628.0f, 795.0f, 1045.0f, 1412.0f };
    float s = 0.0f;
    for (size_t i = 0; i < 6; ++i) {
        metalPhases[i] += (freqs909[i] * tuneRatio * 4.2f) * dt;
        if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
        s += std::sin(metalPhases[i] * kTwoPi + ((i & 1u) ? 0.4f : -0.4f));
    }
    const float raw = (s * 0.14f + nextWhiteNoise() * 0.42f);
    const float quantized6Bit = std::round(raw * 32.0f) * (1.0f / 32.0f);

    const bool isOpen = (channelIndex == 8 || currentToneId == 39);
    const bool isCrash = (currentToneId == 40 || channelIndex == 9);
    const bool isRide = (currentToneId == 41 || channelIndex == 10);
    const float decayRate = isCrash ? (1.4f / decayScale)
                                    : (isRide ? (1.1f / decayScale) : (isOpen ? (3.8f / decayScale) : (24.0f / decayScale)));
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0005f) { active = false; return 0.0f; }

    const float f = 2.0f * std::sin(kPi * std::min(0.16f, (isRide ? 5200.0f : 7400.0f) * tuneRatio /
                                                             static_cast<float>(sampleRate)));
    const float hp = quantized6Bit - svfLp1 - 0.4f * svfBp1;
    svfBp1 += f * hp;
    svfLp1 += f * svfBp1;

    float bell = 0.0f;
    if (isRide) {
        phase1 += 2840.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        bell = std::sin(phase1 * kTwoPi) * std::exp(-envTime * (5.5f / decayScale)) * 0.35f;
    }
    return (hp * 0.65f + svfBp1 * 0.55f + bell) * env * (0.95f + ctrlParam * 0.45f);
}

float AcbVoice::render707Or626Model(int cat, bool is707) {
    const float raw = is707 ? render909Model(cat) : render808Model(cat);
    const float levels = is707 ? 64.0f : 48.0f;
    const float crushed = std::round(raw * levels) / levels;
    return std::tanh(crushed * (1.2f + ctrlParam * 0.6f));
}

float AcbVoice::render727Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    if (cat == 12) {
        if (currentToneId == 68 || currentToneId == 69) {
            const float dur = (currentToneId == 69) ? (0.55f * decayScale) : (0.18f * decayScale);
            if (envTime > dur) { active = false; return 0.0f; }
            const float vib = std::sin(envTime * kTwoPi * 28.0f) * 0.035f;
            phase1 += (2780.0f * tuneRatio * (1.0f + vib)) * dt;
            if (phase1 >= 1.0f) phase1 -= 1.0f;
            const float env = std::min(1.0f, envTime * 80.0f) * std::exp(-(envTime / dur) * 2.2f);
            return (std::sin(phase1 * kTwoPi) * 0.75f + nextWhiteNoise() * 0.15f) * env;
        }
        if (currentToneId == 71) {
            const float env = std::exp(-envTime * (1.8f / decayScale));
            if (env < 0.0005f) { active = false; return 0.0f; }
            float chime = 0.0f;
            for (size_t i = 0; i < 6; ++i) {
                const float f = (2200.0f + static_cast<float>(i) * 670.0f +
                                 std::fmod(envTime * 900.0f, 1200.0f)) * tuneRatio;
                metalPhases[i] += f * dt;
                if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
                chime += std::sin(metalPhases[i] * kTwoPi);
            }
            return (chime * 0.16f) * env;
        }
        const float pulse = (std::fmod(envTime, 0.018f) < 0.004f) ? 1.0f : 0.15f;
        const float env = std::exp(-envTime * (4.5f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        phase1 += 1650.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        return std::sin(phase1 * kTwoPi) * pulse * env;
    }

    if (cat == 11) {
        const float f1 = ((currentToneId == 64) ? 880.0f : 660.0f) * tuneRatio;
        phase1 += f1 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += (f1 * 1.414f) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float env = std::exp(-envTime * (11.0f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        return (std::sin(phase1 * kTwoPi) * 0.7f + std::sin(phase2 * kTwoPi) * 0.35f) * env;
    }

    float baseFreq = 220.0f;
    if (currentToneId == 57) baseFreq = 390.0f;
    else if (currentToneId == 58) baseFreq = 260.0f;
    else if (currentToneId == 59) baseFreq = 310.0f;
    else if (currentToneId == 60) baseFreq = 295.0f;
    else if (currentToneId == 61) baseFreq = 195.0f;
    else if (currentToneId == 62) baseFreq = 250.0f;
    else if (currentToneId == 63) baseFreq = 175.0f;

    baseFreq *= tuneRatio;
    const float pitchBend = 1.0f + 0.35f * std::exp(-envTime * 45.0f);
    phase1 += baseFreq * pitchBend * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
    phase2 += baseFreq * 1.52f * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;

    const float decayRate = (currentToneId == 59 ? 24.0f : 8.5f) / decayScale;
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0005f) { active = false; return 0.0f; }
    const float slap = nextWhiteNoise() * std::exp(-envTime * 110.0f) * (0.25f + ctrlParam * 0.45f);
    return (std::sin(phase1 * kTwoPi) * 0.8f + std::sin(phase2 * kTwoPi) * 0.25f) * env + slap;
}

float AcbVoice::render606Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    if (cat == 1) {
        const float f = 58.0f * tuneRatio * (1.0f + 1.4f * std::exp(-envTime * 55.0f));
        phase1 += f * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        const float env = std::exp(-envTime * (6.2f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        const float click = std::exp(-envTime * 350.0f) * 0.45f;
        return std::tanh((std::sin(phase1 * kTwoPi) * env + click) * 1.3f);
    }
    return render808Model(cat);
}

float AcbVoice::renderCR78Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    if (currentToneId == 171) {
        const float dur = 0.38f * decayScale;
        if (envTime > dur) { active = false; return 0.0f; }
        const float ratchet = 0.5f + 0.5f * std::sin(envTime * kTwoPi * 72.0f);
        phase1 += (1450.0f + envTime * 1100.0f) * tuneRatio * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        return std::sin(phase1 * kTwoPi) * ratchet * (1.0f - envTime / dur) * 0.85f;
    }

    if (currentToneId >= 169 && currentToneId <= 172) {
        static constexpr float crMetal[4] = { 1120.0f, 1680.0f, 2490.0f, 3310.0f };
        float sum = 0.0f;
        for (size_t i = 0; i < 4; ++i) {
            metalPhases[i] += crMetal[i] * tuneRatio * dt;
            if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
            sum += (metalPhases[i] < 0.5f) ? 1.0f : -1.0f;
        }
        const float env = std::exp(-envTime * (12.0f / decayScale));
        if (env < 0.0005f) { active = false; return 0.0f; }
        return sum * 0.22f * env;
    }

    return render808Model(cat) * 0.92f;
}

float AcbVoice::renderFMModel(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 2.0f);
    const float decayScale = 0.2f + decayParam * 2.2f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    float baseCarrier = 110.0f;
    float modRatio = 1.414f + (currentToneId % 7) * 0.37f;
    float modIndexBase = 1.8f + ctrlParam * 6.5f;
    float pitchSweepAmt = 0.5f;
    float decayRate = 8.0f / decayScale;

    if (cat == 1) {
        baseCarrier = 50.0f;
        modRatio = 1.0f + (currentToneId % 4) * 0.5f;
        pitchSweepAmt = 3.2f;
        decayRate = 4.2f / decayScale;
    } else if (cat == 2) {
        baseCarrier = 175.0f;
        modRatio = 2.37f + (currentToneId % 5) * 0.41f;
        pitchSweepAmt = 1.1f;
        decayRate = 10.0f / decayScale;
    } else if (cat == 3 || cat == 8) {
        baseCarrier = (channelIndex == 2) ? 95.0f : (channelIndex == 3 ? 140.0f : 195.0f);
        modRatio = 1.73f;
        pitchSweepAmt = 1.4f;
        decayRate = 6.5f / decayScale;
    } else if (cat == 6 || cat == 7) {
        baseCarrier = 680.0f;
        modRatio = 3.14159f;
        modIndexBase = 4.5f + ctrlParam * 8.0f;
        decayRate = (channelIndex == 7) ? (22.0f / decayScale) : (3.5f / decayScale);
    }

    const float pitchEnv = 1.0f + pitchSweepAmt * std::exp(-envTime * 55.0f);
    const float cFreq = baseCarrier * tuneRatio * pitchEnv;
    const float mFreq = cFreq * modRatio;

    phase2 += mFreq * dt;
    if (phase2 >= 1.0f) phase2 -= 1.0f;

    const float modEnv = std::exp(-envTime * (decayRate * 1.8f));
    const float modSig = std::sin(phase2 * kTwoPi) * modIndexBase * modEnv;

    phase1 += cFreq * dt;
    if (phase1 >= 1.0f) phase1 -= 1.0f;

    const float ampEnv = std::exp(-envTime * decayRate);
    if (ampEnv < 0.0005f) { active = false; return 0.0f; }

    float fmOut = std::sin(phase1 * kTwoPi + modSig) * ampEnv;
    if (cat == 2 || cat == 5) fmOut += nextWhiteNoise() * modEnv * 0.35f;
    return std::tanh(fmOut * 1.25f);
}

float AcbVoice::renderPCMOrSynthModel(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 2.0f);
    const float decayScale = 0.25f + decayParam * 2.2f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    float baseFreq = (cat == 17) ? 65.41f : 220.0f;
    baseFreq *= tuneRatio;

    phase1 += baseFreq * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
    phase2 += (baseFreq * 1.1892f) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
    phase3 += (baseFreq * 1.4983f) * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;

    const float env = std::exp(-envTime * (3.8f / decayScale));
    if (env < 0.0005f) { active = false; return 0.0f; }

    const float saw1 = 2.0f * phase1 - 1.0f;
    const float saw2 = 2.0f * phase2 - 1.0f;
    const float saw3 = 2.0f * phase3 - 1.0f;
    const float raw = (cat == 19) ? ((saw1 + saw2 + saw3) * 0.33f)
                                  : (std::sin(phase1 * kTwoPi) * 0.7f + saw1 * 0.3f);

    const float cutoff = std::min(0.45f, (400.0f + ctrlParam * 4200.0f * env) /
                                             static_cast<float>(sampleRate));
    svfLp1 += cutoff * (raw - svfLp1);
    return std::tanh(svfLp1 * env * 1.35f);
}

void AcbVoice::setSample(std::shared_ptr<const SampleData> sample) {
    sample_ = std::move(sample);
    if (!sample_ || sample_->left.size() < 2) {
        clearSample();
        return;
    }
    useCustomSample = true;
    samplePlayhead = 0.0;
}

void AcbVoice::clearSample() {
    sample_.reset();
    useCustomSample = false;
    samplePlayhead = 0.0;
    sampleReverse = false;
}

float AcbVoice::renderUserSample() {
    if (!sample_ || sample_->left.size() <= 1) {
        active = false;
        return 0.0f;
    }
    const int length = static_cast<int>(sample_->left.size());
    const int idx0 = static_cast<int>(samplePlayhead);
    if (idx0 < 0 || idx0 >= length - 1) {
        active = false;
        return 0.0f;
    }

    const float frac = static_cast<float>(samplePlayhead - static_cast<double>(idx0));
    const float s0 = 0.5f * (sample_->left[static_cast<size_t>(idx0)] + sample_->right[static_cast<size_t>(idx0)]);
    const float s1 = 0.5f * (sample_->left[static_cast<size_t>(idx0 + 1)] +
                             sample_->right[static_cast<size_t>(idx0 + 1)]);
    const float interp = s0 + frac * (s1 - s0);

    const double pitchRatio = std::pow(2.0, (static_cast<double>(tuneParam) - 0.5) * 4.0) *
                              (sample_->sampleRate / sampleRate);
    if (sampleReverse) samplePlayhead -= pitchRatio;
    else samplePlayhead += pitchRatio;

    const float decayDuration = 0.08f + decayParam * 3.5f;
    const float ampEnv = std::exp(-envTime / decayDuration);
    if (ampEnv < 0.0005f) {
        active = false;
        return 0.0f;
    }

    float driven = interp * ampEnv;
    if (ctrlParam > 0.55f) {
        const float drive = 1.0f + (ctrlParam - 0.55f) * 6.0f;
        driven = std::tanh(driven * drive);
    } else if (ctrlParam < 0.45f) {
        const float cut = 0.05f + (ctrlParam / 0.45f) * 0.85f;
        svfLp1 += cut * (driven - svfLp1);
        driven = svfLp1;
    }
    return driven;
}

} // namespace rt8
