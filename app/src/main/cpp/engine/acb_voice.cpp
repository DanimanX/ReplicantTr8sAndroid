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
    phase1 = phase2 = phase3 = phase4 = 0.0f;
    metalPhases.fill(0.0f);
    svfLp1 = svfBp1 = svfLp2 = svfBp2 = svfLp3 = svfBp3 = 0.0f;
    snhPhase = snhHeld = 0.0f;
    dcBlockX = dcBlockY = 0.0f;
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
    phase4 = 0.0f;
    svfLp1 = svfBp1 = svfLp2 = svfBp2 = svfLp3 = svfBp3 = 0.0f;
    snhPhase = 1.0f;
    snhHeld = 0.0f;
    chokeGain = 1.0f;
    choking = false;
    active = true;

    if (useCustomSample && sample_ && !sample_->left.empty())
        samplePlayhead = sampleReverse ? static_cast<double>(sample_->left.size() - 1) : 0.0;

    if (isFlam) {
        flamPending = true;
        flamCountdownSamples = static_cast<int>(sampleRate * 0.024); // 24 ms classic TR flam
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
        chokeGain *= 0.962f; // ~2.8 ms smooth analog VCA choke
        mono *= chokeGain;
        if (chokeGain < 0.0008f) {
            active = false;
            choking = false;
        }
    }

    // 18 Hz high-pass DC blocker keeps sub transients symmetric and clean.
    const float dcCoeff = std::max(0.985f, 1.0f - (113.0f / static_cast<float>(sampleRate)));
    const float dcOut = mono - dcBlockX + dcCoeff * dcBlockY;
    dcBlockX = sanitize(mono);
    dcBlockY = sanitize(dcOut);
    mono = dcBlockY;

    mono *= currentVelocity * (levelParam * kMaxMixerGain);
    // Warm analog output transformer saturation.
    mono = std::tanh(mono * 1.18f);
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

    // Dispatch based on exact TR-8S v3.00 firmware tone ranges.
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

// ==============================================================================
// TR-808 ACB CIRCUIT MODELS (CCRMA / SERVICE MANUAL COMPONENT MODELING)
// ==============================================================================
float AcbVoice::render808Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.4f);
    const float decayScale = 0.22f + decayParam * 2.25f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // 1. 808 BASS DRUM (bridged-T network, dual pitch drop and sub sigh)
    if (cat == 1 || currentToneId == 158) {
        const bool isLong = (currentToneId == 2 || currentToneId == 3 || currentToneId == 158);
        const bool isBass2 = (currentToneId == 1 || currentToneId == 3);
        const float baseFreq = (currentToneId == 158 ? 55.0f : (isBass2 ? 49.0f : 46.5f)) * tuneRatio;
        const float decayTime = (isLong ? 1.25f : 0.48f) * decayScale;

        const float fastEnv = std::exp(-envTime * 215.0f);
        const float punchEnv = std::exp(-envTime * 34.0f);
        const float sighDrift = std::exp(-envTime * 3.2f);
        const float attackDepth = (isBass2 ? 2.1f : 1.45f) + ctrlParam * 2.2f;
        const float freq = baseFreq * (0.95f + 0.08f * sighDrift + 0.42f * punchEnv + attackDepth * fastEnv);

        phase1 += freq * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float ampEnv = std::exp(-envTime / std::max(0.045f, decayTime));
        if (ampEnv < 0.0004f) { active = false; return 0.0f; }

        // Bridged-T op-amp saturation asymmetry during the initial punch.
        const float harmEnv = 0.07f + 0.12f * punchEnv;
        const float body = std::sin(phase1 * kTwoPi)
                         + harmEnv * std::sin(phase1 * kTwoPi * 2.0f)
                         - 0.025f * std::cos(phase1 * kTwoPi * 3.0f);

        // Bipolar trigger pulse click plus a low-passed wood knock (no DC spike).
        const float pulseClick = std::sin(envTime * kTwoPi * 1180.0f) * std::exp(-envTime * 680.0f);
        const float woodNoise = nextWhiteNoise() * std::exp(-envTime * 520.0f);
        svfLp1 += 0.14f * (woodNoise - svfLp1);
        const float click = (pulseClick * 0.48f + svfLp1 * 0.55f) * (0.22f + ctrlParam * 0.95f);

        return std::tanh((body * ampEnv + click) * 1.28f);
    }

    // 2. 808 SNARE DRUM (dual bridged-T shells + high-passed snappy wires)
    if (cat == 2) {
        const bool isSnare2 = (currentToneId == 5);
        const float headSweep = 1.0f + 0.36f * std::exp(-envTime * 135.0f);
        const float f1 = (isSnare2 ? 188.0f : 180.0f) * tuneRatio * headSweep;
        const float f2 = (isSnare2 ? 342.0f : 330.0f) * tuneRatio * headSweep;
        phase1 += f1 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += f2 * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float toneEnv1 = std::exp(-envTime * (16.5f / decayScale));
        const float toneEnv2 = std::exp(-envTime * (23.0f / decayScale));
        const float wireEnv = 0.35f * std::exp(-envTime * 52.0f) + 0.65f * std::exp(-envTime * (10.2f / decayScale));
        if (toneEnv1 < 0.0004f && wireEnv < 0.0004f) { active = false; return 0.0f; }

        const float shells = std::sin(phase1 * kTwoPi) * toneEnv1 * 0.64f
                           + std::sin(phase2 * kTwoPi) * toneEnv2 * 0.42f;

        const float rawNoise = nextWhiteNoise();
        const float fSvf = 2.0f * std::sin(kPi * std::min(0.22f, (2650.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = rawNoise - svfLp1 - 0.42f * svfBp1;
        svfBp1 += fSvf * hp;
        svfLp1 += fSvf * svfBp1;

        const float snappy = (hp * 0.68f + svfBp1 * 0.52f) * wireEnv * (0.22f + ctrlParam * 1.25f);
        const float stickClick = std::sin(envTime * kTwoPi * 920.0f) * std::exp(-envTime * 580.0f) * 0.22f;
        return std::tanh((shells * 0.88f + snappy * 0.78f + stickClick) * 1.22f);
    }

    // 3. 808 TOMS (id 6..8, 12..14) and 808 CONGAS (id 9..11, 81..83)
    if (cat == 3 || cat == 8) {
        const bool isConga = (cat == 8 || (currentToneId >= 9 && currentToneId <= 11) ||
                              (currentToneId >= 81 && currentToneId <= 83));
        const bool isNoiseTom = (currentToneId >= 12 && currentToneId <= 14);

        float baseFreq = 138.0f;
        if (currentToneId == 6 || currentToneId == 12) baseFreq = 94.0f;
        else if (currentToneId == 7 || currentToneId == 13) baseFreq = 140.0f;
        else if (currentToneId == 8 || currentToneId == 14) baseFreq = 202.0f;
        else if (currentToneId == 9 || currentToneId == 81) baseFreq = 168.0f;
        else if (currentToneId == 10 || currentToneId == 82) baseFreq = 248.0f;
        else if (currentToneId == 11 || currentToneId == 83) baseFreq = 362.0f;
        else if (channelIndex == 2) baseFreq = isConga ? 168.0f : 94.0f;
        else if (channelIndex == 3) baseFreq = isConga ? 248.0f : 140.0f;
        else if (channelIndex == 4) baseFreq = isConga ? 362.0f : 202.0f;

        baseFreq *= tuneRatio;
        const float pitchSweep = isConga
            ? (1.0f + 0.32f * std::exp(-envTime * 54.0f))
            : (1.0f + 0.54f * std::exp(-envTime * 31.0f));
        phase1 += (baseFreq * pitchSweep) * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float decayRate = (isConga ? 9.8f : 6.2f) / decayScale;
        const float ampEnv = std::exp(-envTime * decayRate);
        if (ampEnv < 0.0004f) { active = false; return 0.0f; }

        const float body = std::sin(phase1 * kTwoPi) +
                           (isConga ? 0.11f : 0.05f) * std::sin(phase1 * kTwoPi * 2.0f);

        // Filtered pink/membrane noise (LPF @ 580 Hz) instead of raw white noise.
        const float rawN = nextWhiteNoise();
        const float cut = std::min(0.25f, 580.0f / static_cast<float>(sampleRate));
        svfLp1 += cut * (rawN - svfLp1);
        const float noiseAmt = isConga ? (0.06f + ctrlParam * 0.15f)
                                       : ((isNoiseTom ? 0.35f : 0.14f) + ctrlParam * 0.38f);
        const float membraneThud = svfLp1 * std::exp(-envTime * (isNoiseTom ? 26.0f : 58.0f)) * noiseAmt;

        return std::tanh((body * ampEnv + membraneThud) * 1.15f);
    }

    // 4. 808 RIM SHOT (cat 4, id 15) and 808 CLAVES (cat 9, id 16)
    if (cat == 4 || cat == 9) {
        if (cat == 9 || currentToneId == 16) {
            // 808 Claves: dual coupled bridged-T (2480 Hz + 1180 Hz cavity).
            phase1 += 2480.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
            phase2 += 1180.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
            const float env1 = std::exp(-envTime * (42.0f / decayScale));
            const float env2 = std::exp(-envTime * 115.0f);
            if (env1 < 0.0004f) { active = false; return 0.0f; }
            const float clave = std::sin(phase1 * kTwoPi) * env1 * 0.85f +
                                std::sin(phase2 * kTwoPi) * env2 * 0.28f;
            return std::tanh(clave * 1.25f);
        }
        // 808 Rim Shot: dual bridged-T resonators with a fast transient clip.
        const float snap = 1.0f + 0.22f * std::exp(-envTime * 450.0f);
        phase1 += 485.0f * tuneRatio * snap * dt;  if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 1690.0f * tuneRatio * snap * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float envBody = std::exp(-envTime * (82.0f / decayScale));
        const float envRim = std::exp(-envTime * (155.0f / decayScale));
        if (envBody < 0.0004f) { active = false; return 0.0f; }
        const float click = std::sin(envTime * kTwoPi * 2200.0f) * std::exp(-envTime * 750.0f) * 0.45f;
        const float sig = std::sin(phase1 * kTwoPi) * envBody * 0.68f
                        + std::sin(phase2 * kTwoPi) * envRim * 0.92f
                        + click;
        return std::tanh(sig * (1.9f + ctrlParam * 1.4f));
    }

    // 5. 808 HAND CLAP (cat 5, id 17, 19, 20) and 808 MARACAS (cat 10, id 18)
    if (cat == 5 || cat == 10) {
        if (cat == 10 || currentToneId == 18) {
            // 808 Maracas: 4 ms shaker attack plus 5.5 kHz high-passed noise.
            const float attack = 1.0f - std::exp(-envTime * 280.0f);
            const float env = attack * std::exp(-envTime * (38.0f / decayScale));
            if (envTime > 0.015f && env < 0.0004f) { active = false; return 0.0f; }
            const float n = nextWhiteNoise();
            const float f = 2.0f * std::sin(kPi * std::min(0.22f, (5500.0f * tuneRatio) / static_cast<float>(sampleRate)));
            const float hp = n - svfLp1 - 0.5f * svfBp1;
            svfBp1 += f * hp;
            svfLp1 += f * svfBp1;
            return hp * env * 0.95f;
        }
        if (currentToneId == 19) {
            // 808 Finger Snap: crisp single resonant pop (1.15 kHz + 2.35 kHz).
            const float snapHit = (envTime < 0.002f) ? 0.4f : 1.0f;
            phase1 += 1150.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
            phase2 += 2350.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
            const float env = std::exp(-envTime * (68.0f / decayScale));
            if (env < 0.0004f) { active = false; return 0.0f; }
            const float n = nextWhiteNoise() * std::exp(-envTime * 190.0f) * 0.45f;
            return std::tanh(((std::sin(phase1 * kTwoPi) * 0.65f + std::sin(phase2 * kTwoPi) * 0.45f) * env + n)
                             * snapHit * 1.4f);
        }

        // 808 Hand Clap: four sawtooth micro-bursts plus an independent room tail.
        constexpr float burstSpacing = 0.0108f;
        constexpr float burstTotal = burstSpacing * 4.0f;
        float strikeEnv = 0.0f;
        if (envTime < burstTotal) {
            const float toothPhase = std::fmod(envTime, burstSpacing) / burstSpacing;
            strikeEnv = std::exp(-toothPhase * 4.3f);
        } else {
            strikeEnv = std::exp(-(envTime - burstTotal) * 28.0f);
        }
        const float tailEnv = (0.36f + ctrlParam * 0.34f) * std::exp(-envTime * (6.2f / decayScale));
        const float totalEnv = std::max(strikeEnv, tailEnv);
        if (envTime > burstTotal && totalEnv < 0.0004f) { active = false; return 0.0f; }

        const float n = nextWhiteNoise();
        // Filter 1: 1080 Hz resonant palm-cavity bandpass (Q ~ 5.5).
        const float f1 = 2.0f * std::sin(kPi * std::min(0.20f, (1080.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp1 = n - svfLp1 - 0.18f * svfBp1;
        svfBp1 += f1 * hp1;
        svfLp1 += f1 * svfBp1;

        // Filter 2: 3100 Hz crisp skin-snap bandpass weighted on the strikes.
        const float f2 = 2.0f * std::sin(kPi * std::min(0.22f, (3100.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp2 = n - svfLp2 - 0.32f * svfBp2;
        svfBp2 += f2 * hp2;
        svfLp2 += f2 * svfBp2;

        const float clapOut = svfBp1 * (strikeEnv * 1.15f + tailEnv * 0.95f)
                            + svfBp2 * (strikeEnv * 0.72f + tailEnv * 0.32f);
        return std::tanh(clapOut * 1.65f);
    }

    // 6. 808 COWBELL (540 Hz + 800 Hz Schmitt-trigger pulses + 680 Hz resonant BPF)
    if (cat == 11 || currentToneId == 25) {
        phase1 += 540.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 800.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float p1 = (phase1 < 0.48f) ? 1.0f : -0.92f;
        const float p2 = (phase2 < 0.48f) ? 1.0f : -0.92f;

        const float envStrike = 0.72f * std::exp(-envTime * 42.0f);
        const float envRing = 0.34f * std::exp(-envTime * (5.2f / decayScale));
        const float env = envStrike + envRing;
        if (env < 0.0004f) { active = false; return 0.0f; }

        const float raw = p1 * 0.58f + p2 * 0.42f;
        const float f = 2.0f * std::sin(kPi * std::min(0.20f, (680.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = raw - svfLp1 - 0.26f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;

        const float out = (svfBp1 * 1.15f + raw * 0.22f) * env;
        return std::tanh(out * (1.35f + ctrlParam * 0.45f));
    }

    // 7. 808 HI-HATS (cat 6) and 808 CYMBALS (cat 7)
    // CCRMA six-oscillator Schmitt-trigger bank.
    static constexpr float freqs808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    float metalSum = 0.0f;
    for (size_t i = 0; i < 6; ++i) {
        metalPhases[i] += (freqs808[i] * tuneRatio) * dt;
        if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
        metalSum += (metalPhases[i] < 0.48f) ? 1.0f : -0.92f;
    }
    // Non-linear diode intermodulation across the six square waves.
    const float intermod = std::tanh(metalSum * 0.65f) - metalSum * 0.25f;

    // Bandpass 1 @ 3440 Hz (lower cymbal/hat resonator).
    const float fBp1 = 2.0f * std::sin(kPi * std::min(0.21f, (3440.0f * tuneRatio) / static_cast<float>(sampleRate)));
    const float hp1 = intermod - svfLp1 - 0.28f * svfBp1;
    svfBp1 += fBp1 * hp1;
    svfLp1 += fBp1 * svfBp1;

    // Bandpass 2 / highpass @ 7100 Hz (upper sizzle network).
    const float fBp2 = 2.0f * std::sin(kPi * std::min(0.22f, (7100.0f * tuneRatio) / static_cast<float>(sampleRate)));
    const float hp2 = hp1 - svfLp2 - 0.32f * svfBp2;
    svfBp2 += fBp2 * hp2;
    svfLp2 += fBp2 * svfBp2;

    const bool isCymbal = (cat == 7 || currentToneId == 23 || currentToneId == 24 || channelIndex >= 9);
    const bool isOpen = (!isCymbal) && (channelIndex == 8 || currentToneId == 22);

    if (isCymbal) {
        // Parallel dual-VCA branches: fast crash wash plus swelling shimmer tail.
        const float envLow = std::exp(-envTime * (6.2f / decayScale));
        const float swell = 1.0f - std::exp(-envTime * 95.0f);
        const float tailRate = (currentToneId == 24 ? 3.6f : 1.45f) / decayScale;
        const float envHigh = swell * std::exp(-envTime * tailRate);
        if (envLow < 0.0004f && envHigh < 0.0004f) { active = false; return 0.0f; }

        const float toneBal = 0.35f + ctrlParam * 0.65f;
        const float cym = svfBp1 * envLow * (1.15f - toneBal * 0.45f)
                        + (hp2 * 0.85f + svfBp2 * 0.65f) * envHigh * (0.85f + toneBal * 0.65f);
        return std::tanh(cym * 1.45f);
    }

    const float decayRate = isOpen ? (4.2f / decayScale) : (28.0f / decayScale);
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0004f) { active = false; return 0.0f; }

    const float hh = (hp2 * 0.92f + svfBp2 * 0.68f + svfBp1 * 0.25f) * env * (0.95f + ctrlParam * 0.45f);
    return std::tanh(hh * 1.55f);
}

// ==============================================================================
// TR-909 ACB CIRCUIT & 6-BIT 32 kHz EPROM MODELS
// ==============================================================================
float AcbVoice::render909Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.4f);
    const float decayScale = 0.22f + decayParam * 2.15f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // 1. 909 BASS DRUM (triangle to diode-sine shaper, dual pitch sweep)
    if (cat == 1 || currentToneId == 157) {
        const bool isLowBass = (currentToneId == 29);
        const bool isAtkBass = (currentToneId == 28);
        const bool isBass2 = (currentToneId == 27 || isAtkBass);

        const float baseFreq = (currentToneId == 157 ? 55.0f : (isLowBass ? 46.0f : 51.5f)) * tuneRatio;
        const float fastEnv = std::exp(-envTime * 108.0f);
        const float medEnv = std::exp(-envTime * 24.5f);
        const float freq = baseFreq + ((isBass2 ? 225.0f : 180.0f) * fastEnv + 52.0f * medEnv) * tuneRatio;

        phase1 += freq * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;

        const float tri = 4.0f * std::abs(phase1 - 0.5f) - 1.0f;
        const float diodeDrive = 1.5707963f * (1.0f + 0.35f * fastEnv);
        const float shapedSine = std::sin(clampf(tri * diodeDrive, -1.5707963f, 1.5707963f));

        const float ampEnv = std::exp(-envTime * (2.65f / decayScale));
        if (ampEnv < 0.0004f) { active = false; return 0.0f; }

        // Attack circuit: bipolar 1.8 ms click plus a 1.6 kHz filtered noise burst.
        const float atkScale = (isAtkBass ? 1.38f : 1.0f) * (0.18f + ctrlParam * 1.25f);
        const float pulseClick = std::sin(envTime * kTwoPi * 1380.0f) * std::exp(-envTime * 560.0f);
        const float rawNoise = nextWhiteNoise() * std::exp(-envTime * 380.0f);
        const float cut = std::min(0.25f, 1650.0f / static_cast<float>(sampleRate));
        svfLp1 += cut * (rawNoise - svfLp1);
        const float click = (pulseClick * 0.65f + svfLp1 * 0.72f) * atkScale;

        return std::tanh((shapedSine * ampEnv + click) * 1.48f);
    }

    // 2. 909 SNARE DRUM (dual triangle-shaped VCOs, dual-slope filtered wires)
    if (cat == 2) {
        const bool isAtkSnare = (currentToneId == 32);
        const bool isSnare2 = (currentToneId == 31 || isAtkSnare);

        const float sweep = 1.0f + (isAtkSnare ? 0.95f : 0.78f) * std::exp(-envTime * 84.0f);
        phase1 += 186.0f * tuneRatio * sweep * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 344.0f * tuneRatio * sweep * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float bodyEnv1 = std::exp(-envTime * (15.5f / decayScale));
        const float bodyEnv2 = std::exp(-envTime * (21.0f / decayScale));
        const float snapEnv = 0.55f * std::exp(-envTime * 26.0f)
                            + 0.45f * std::exp(-envTime * (7.2f / decayScale));
        if (bodyEnv1 < 0.0004f && snapEnv < 0.0004f) { active = false; return 0.0f; }

        const float tri1 = 4.0f * std::abs(phase1 - 0.5f) - 1.0f;
        const float tri2 = 4.0f * std::abs(phase2 - 0.5f) - 1.0f;
        const float body = std::sin(tri1 * (kPi * 0.5f)) * bodyEnv1 * 0.68f
                         + std::sin(tri2 * (kPi * 0.5f)) * bodyEnv2 * (isSnare2 ? 0.46f : 0.36f);

        const float n = nextWhiteNoise();
        const float fSvf = 2.0f * std::sin(kPi * std::min(0.21f, (2850.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = n - svfLp1 - 0.45f * svfBp1;
        svfBp1 += fSvf * hp;
        svfLp1 += fSvf * svfBp1;

        const float snappy = (hp * 0.72f + svfBp1 * 0.58f) * snapEnv * (0.24f + ctrlParam * 1.38f);
        const float stick = std::sin(envTime * kTwoPi * 1120.0f) * std::exp(-envTime * 520.0f) *
                            (isAtkSnare ? 0.42f : 0.25f);
        return std::tanh((body * 0.92f + snappy * 0.82f + stick) * 1.32f);
    }

    // 3. 909 TOMS (dual-mode coupled resonators)
    if (cat == 3) {
        float baseFreq = 132.0f;
        if (currentToneId == 33) baseFreq = 90.0f;
        else if (currentToneId == 34) baseFreq = 132.0f;
        else if (currentToneId == 35) baseFreq = 186.0f;
        else if (channelIndex == 2) baseFreq = 90.0f;
        else if (channelIndex == 3) baseFreq = 132.0f;
        else if (channelIndex == 4) baseFreq = 186.0f;

        baseFreq *= tuneRatio;
        const float sweep = 1.0f + 0.82f * std::exp(-envTime * 36.0f) + 0.20f * std::exp(-envTime * 11.0f);
        phase1 += (baseFreq * sweep) * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += (baseFreq * 1.56f * (1.0f + 0.35f * std::exp(-envTime * 48.0f))) * dt;
        if (phase2 >= 1.0f) phase2 -= 1.0f;

        const float ampEnv = std::exp(-envTime * (5.6f / decayScale));
        const float shellEnv = std::exp(-envTime * (14.5f / decayScale));
        if (ampEnv < 0.0004f) { active = false; return 0.0f; }

        const float tri = 4.0f * std::abs(phase1 - 0.5f) - 1.0f;
        const float mainHead = std::sin(tri * (kPi * 0.5f)) * ampEnv;
        const float shellMode = std::sin(phase2 * kTwoPi) * shellEnv * 0.22f;

        // Low-passed membrane stick impact (@ 920 Hz) instead of raw white noise.
        const float rawClick = nextWhiteNoise() * std::exp(-envTime * 210.0f);
        const float cut = std::min(0.24f, 920.0f / static_cast<float>(sampleRate));
        svfLp1 += cut * (rawClick - svfLp1);
        const float impact = (svfLp1 * 0.65f + std::sin(envTime * kTwoPi * 680.0f) * std::exp(-envTime * 420.0f) * 0.28f)
                           * (0.18f + ctrlParam * 0.55f);

        return std::tanh((mainHead + shellMode + impact) * 1.22f);
    }

    // 4. 909 RIM SHOT (three-mode bridged-T pulse circuit)
    if (cat == 4) {
        const float snap = 1.0f + 0.25f * std::exp(-envTime * 480.0f);
        phase1 += 265.0f * tuneRatio * snap * dt;  if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 520.0f * tuneRatio * snap * dt;  if (phase2 >= 1.0f) phase2 -= 1.0f;
        phase3 += 1640.0f * tuneRatio * snap * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;

        const float env1 = std::exp(-envTime * (75.0f / decayScale));
        const float env2 = std::exp(-envTime * (105.0f / decayScale));
        const float env3 = std::exp(-envTime * (175.0f / decayScale));
        if (env1 < 0.0004f) { active = false; return 0.0f; }

        const float click = std::sin(envTime * kTwoPi * 2400.0f) * std::exp(-envTime * 800.0f) * 0.45f;
        const float mix = std::sin(phase1 * kTwoPi) * env1 * 0.48f
                        + std::sin(phase2 * kTwoPi) * env2 * 0.68f
                        + std::sin(phase3 * kTwoPi) * env3 * 0.95f
                        + click;
        return std::tanh(mix * (2.2f + ctrlParam * 1.4f));
    }

    // 5. 909 HAND CLAP (four rapid pulse bursts, dual-band filter, room tail)
    if (cat == 5) {
        constexpr float burstSpacing = 0.0096f;
        constexpr float burstTotal = burstSpacing * 4.0f;
        float strikeEnv = 0.0f;
        if (envTime < burstTotal) {
            const float b = std::fmod(envTime, burstSpacing) / burstSpacing;
            strikeEnv = std::exp(-b * 3.9f);
        } else {
            strikeEnv = std::exp(-(envTime - burstTotal) * 30.0f);
        }
        const float tailEnv = (0.42f + ctrlParam * 0.38f) * std::exp(-envTime * (6.6f / decayScale));
        if (envTime > burstTotal && std::max(strikeEnv, tailEnv) < 0.0004f) { active = false; return 0.0f; }

        const float n = nextWhiteNoise();
        // Band 1: 1220 Hz resonant palm-slap body.
        const float f1 = 2.0f * std::sin(kPi * std::min(0.20f, (1220.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp1 = n - svfLp1 - 0.20f * svfBp1;
        svfBp1 += f1 * hp1;
        svfLp1 += f1 * svfBp1;

        // Band 2: 3500 Hz bright 909 clap snap.
        const float f2 = 2.0f * std::sin(kPi * std::min(0.22f, (3500.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp2 = n - svfLp2 - 0.32f * svfBp2;
        svfBp2 += f2 * hp2;
        svfLp2 += f2 * svfBp2;

        const float clap = svfBp1 * (strikeEnv * 1.2f + tailEnv * 0.95f)
                         + svfBp2 * (strikeEnv * 0.85f + tailEnv * 0.38f);
        return std::tanh(clap * 1.75f);
    }

    // 6. 909 6-bit 32 kHz EPROM metallic hats and cymbals
    const bool isCrash = (currentToneId == 40 || (cat == 7 && channelIndex == 9));
    const bool isRide = (currentToneId == 41 || (cat == 7 && channelIndex == 10));
    const bool isOpen = (!isCrash && !isRide) && (channelIndex == 8 || currentToneId == 39);

    // Paiste 602 inharmonic modal ratios with cross-phase FM modulation.
    static constexpr float freqs909[6] = { 412.0f, 587.0f, 811.0f, 1108.0f, 1492.0f, 1978.0f };
    float metal = 0.0f;
    float prevMod = 0.0f;
    for (size_t i = 0; i < 6; ++i) {
        metalPhases[i] += (freqs909[i] * tuneRatio * (isCrash ? 2.65f : (isRide ? 2.85f : 3.25f))) * dt;
        if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
        const float modSig = std::sin(metalPhases[i] * kTwoPi + prevMod * 1.35f);
        prevMod = modSig;
        metal += modSig;
    }
    metal *= (1.0f / 6.0f);

    const float n = nextWhiteNoise();
    const float rawCymbal = metal * 0.72f + n * (isCrash ? 0.36f : 0.26f);

    // Authentic 32 kHz clock sample-and-hold and 6-bit (64-step) D/A quantiser.
    const float epromRate = 32000.0f * tuneRatio;
    snhPhase += epromRate * dt;
    if (snhPhase >= 1.0f) {
        snhPhase -= std::floor(snhPhase);
        snhHeld = std::round(clampf(rawCymbal, -1.0f, 1.0f) * 32.0f) * (1.0f / 32.0f);
    }

    const float decayRate = isCrash ? (1.35f / decayScale)
                          : (isRide ? (1.15f / decayScale)
                          : (isOpen ? (3.6f / decayScale) : (25.0f / decayScale)));
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0004f) { active = false; return 0.0f; }

    // Bandpass 1: mid-high metallic body (2.4 kHz crash/ride, 4.2 kHz hats).
    const float bodyHz = (isCrash ? 2250.0f : (isRide ? 2650.0f : 4200.0f)) * tuneRatio;
    const float f1 = 2.0f * std::sin(kPi * std::min(0.21f, bodyHz / static_cast<float>(sampleRate)));
    const float hp1 = snhHeld - svfLp1 - 0.30f * svfBp1;
    svfBp1 += f1 * hp1;
    svfLp1 += f1 * svfBp1;

    // Bandpass 2 / highpass: crisp Paiste sizzle (5.8 kHz .. 8.5 kHz).
    const float sizzleHz = (isRide ? 5800.0f : 7200.0f) * tuneRatio;
    const float f2 = 2.0f * std::sin(kPi * std::min(0.22f, sizzleHz / static_cast<float>(sampleRate)));
    const float hp2 = hp1 - svfLp2 - 0.35f * svfBp2;
    svfBp2 += f2 * hp2;
    svfLp2 += f2 * svfBp2;

    if (isRide) {
        // Pingy dual-mode bell (2420 Hz + 3780 Hz) plus metallic bow wash.
        phase1 += 2420.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 3780.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float bellEnv = std::exp(-envTime * (9.5f / decayScale));
        const float pingEnv = std::exp(-envTime * 48.0f);
        const float bell = (std::sin(phase1 * kTwoPi) * 0.65f + std::sin(phase2 * kTwoPi) * 0.45f) * bellEnv * 0.38f
                         + svfBp2 * pingEnv * 0.35f;
        const float rideOut = (svfBp1 * 0.48f + hp2 * 0.62f + svfBp2 * 0.45f) * env + bell;
        return std::tanh(rideOut * (1.25f + ctrlParam * 0.45f));
    }

    if (isCrash) {
        // Explosive initial splash plus a wide metallic wash.
        const float splashEnv = std::exp(-envTime * (5.2f / decayScale));
        const float crashOut = svfBp1 * splashEnv * 0.72f + (hp2 * 0.78f + svfBp2 * 0.58f) * env;
        return std::tanh(crashOut * (1.35f + ctrlParam * 0.45f));
    }

    // Closed / open hi-hat with a crisp Paiste stick chick.
    const float chickEnv = std::exp(-envTime * 165.0f);
    const float hhOut = (hp2 * 0.85f + svfBp2 * 0.65f + svfBp1 * (0.32f + chickEnv * 0.45f)) * env;
    return std::tanh(hhOut * (1.38f + ctrlParam * 0.45f));
}

// ==============================================================================
// TR-707, TR-626 AND PCM VINTAGE 12-BIT DRUM MODELS
// ==============================================================================
float AcbVoice::render707Or626Model(int cat, bool is707) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.22f + decayParam * 2.15f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    float raw = 0.0f;
    // Special 707 / 626 / PCM percussion categories (cowbell, tambourine, shaker, agogo, triangle).
    if (cat == 11) {
        const bool isTriangle = (currentToneId == 353 || currentToneId == 354);
        const float f1 = (isTriangle ? 3120.0f : 565.0f) * tuneRatio;
        const float f2 = (isTriangle ? 4780.0f : 845.0f) * tuneRatio;
        phase1 += f1 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += f2 * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float env = std::exp(-envTime * ((isTriangle ? 3.8f : 11.5f) / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        raw = (std::sin(phase1 * kTwoPi) * 0.65f + std::sin(phase2 * kTwoPi + 0.4f) * 0.5f) * env;
    } else if (cat == 10) {
        // 707 tambourine / 626 shaker and tambourine.
        const bool isShaker = (currentToneId >= 341 && currentToneId <= 343);
        const float env = isShaker
            ? ((1.0f - std::exp(-envTime * 220.0f)) * std::exp(-envTime * (32.0f / decayScale)))
            : ((0.55f * std::exp(-envTime * 65.0f) + 0.65f * std::exp(-envTime * (12.5f / decayScale))));
        if (envTime > 0.02f && env < 0.0004f) { active = false; return 0.0f; }
        phase1 += 5400.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 7850.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float jingle = isShaker ? 0.0f : (std::sin(phase1 * kTwoPi) + std::sin(phase2 * kTwoPi)) * 0.25f;
        const float n = nextWhiteNoise();
        const float f = 2.0f * std::sin(kPi * std::min(0.22f, 5200.0f / static_cast<float>(sampleRate)));
        const float hp = n - svfLp1 - 0.4f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        raw = (hp * 0.75f + jingle) * env;
    } else {
        // 909 acoustic hybrid core for 707/EDM/Studio PCM, 808 analog core for hip-hop/808 PCM.
        const bool useAnalogCore = (!is707) && (currentToneId == 184 || currentToneId == 185 || currentToneId == 187
                                             || (currentToneId >= 194 && currentToneId <= 197)
                                             || (currentToneId >= 230 && currentToneId <= 234)
                                             || (currentToneId >= 278 && currentToneId <= 280)
                                             || (currentToneId >= 312 && currentToneId <= 315)
                                             || (currentToneId >= 323 && currentToneId <= 340));
        raw = useAnalogCore ? render808Model(cat) : render909Model(cat);
    }

    // Authentic 25 kHz (TR-707) / 28 kHz (TR-626) sample-and-hold and companding DAC.
    const float dacClock = (is707 ? 25000.0f : 28000.0f) * tuneRatio;
    snhPhase += dacClock * dt;
    if (snhPhase >= 1.0f) {
        snhPhase -= std::floor(snhPhase);
        const float levels = is707 ? 96.0f : 64.0f;
        snhHeld = std::round(raw * levels) / levels;
    }
    return std::tanh(snhHeld * (1.15f + ctrlParam * 0.55f));
}

// ==============================================================================
// TR-727 LATIN PERCUSSION MODELS
// ==============================================================================
float AcbVoice::render727Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // Whistle / quijada / star chime (cat 12).
    if (cat == 12) {
        if (currentToneId == 68 || currentToneId == 69) {
            // Samba whistle (pea-ball LFO vibrato at 2.8 kHz + airy breath turbulence).
            const float dur = (currentToneId == 69) ? (0.55f * decayScale) : (0.18f * decayScale);
            if (envTime > dur) { active = false; return 0.0f; }
            const float vib = std::sin(envTime * kTwoPi * 29.0f) * 0.038f;
            phase1 += (2780.0f * tuneRatio * (1.0f + vib)) * dt;
            if (phase1 >= 1.0f) phase1 -= 1.0f;
            const float env = std::min(1.0f, envTime * 90.0f) * std::exp(-(envTime / dur) * 2.1f);
            const float breath = nextWhiteNoise();
            svfLp1 += 0.25f * (breath - svfLp1);
            return (std::sin(phase1 * kTwoPi) * 0.78f + (breath - svfLp1) * 0.16f) * env;
        }
        if (currentToneId == 71) {
            // Star chime (cascading bar chimes 2.2 kHz .. 8.5 kHz).
            const float env = std::exp(-envTime * (1.75f / decayScale));
            if (env < 0.0004f) { active = false; return 0.0f; }
            float chime = 0.0f;
            for (size_t i = 0; i < 6; ++i) {
                const float f = (2250.0f + static_cast<float>(i) * 710.0f + std::fmod(envTime * 950.0f, 1350.0f)) * tuneRatio;
                metalPhases[i] += f * dt;
                if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
                chime += std::sin(metalPhases[i] * kTwoPi);
            }
            return (chime * 0.17f) * env;
        }
        // Quijada (jawbone rattle).
        const float pulse = (std::fmod(envTime, 0.017f) < 0.0045f) ? 1.0f : 0.15f;
        const float env = std::exp(-envTime * (4.5f / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        phase1 += 1680.0f * tuneRatio * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 2540.0f * tuneRatio * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        return (std::sin(phase1 * kTwoPi) * 0.7f + std::sin(phase2 * kTwoPi) * 0.35f) * pulse * env;
    }

    // Cabasa (66) and maracas (67) (cat 10).
    if (cat == 10 || currentToneId == 66 || currentToneId == 67) {
        const bool isCabasa = (currentToneId == 66);
        const float attack = 1.0f - std::exp(-envTime * (isCabasa ? 190.0f : 280.0f));
        const float env = attack * std::exp(-envTime * ((isCabasa ? 26.0f : 36.0f) / decayScale));
        if (envTime > 0.02f && env < 0.0004f) { active = false; return 0.0f; }
        const float n = nextWhiteNoise();
        const float f = 2.0f * std::sin(kPi * std::min(0.22f, (isCabasa ? 3800.0f : 5600.0f) * tuneRatio /
                                                                static_cast<float>(sampleRate)));
        const float hp = n - svfLp1 - 0.32f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        return (isCabasa ? (svfBp1 * 0.8f + hp * 0.5f) : hp) * env * 1.15f;
    }

    // Agogo bells (cat 11).
    if (cat == 11) {
        const float f1 = ((currentToneId == 64) ? 885.0f : 665.0f) * tuneRatio;
        phase1 += f1 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += (f1 * 1.46f) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        phase3 += (f1 * 2.18f) * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;
        const float env = std::exp(-envTime * (10.5f / decayScale));
        const float clinkEnv = std::exp(-envTime * 85.0f);
        if (env < 0.0004f) { active = false; return 0.0f; }
        return (std::sin(phase1 * kTwoPi) * 0.72f
              + std::sin(phase2 * kTwoPi) * 0.38f
              + std::sin(phase3 * kTwoPi) * clinkEnv * 0.28f) * env;
    }

    // Bongos (57, 58), congas (59, 60, 61) and timbales (62, 63) (cat 8).
    float baseFreq = 220.0f;
    const bool isTimbale = (currentToneId == 62 || currentToneId == 63);
    if (currentToneId == 57) baseFreq = 405.0f;      // Hi bongo
    else if (currentToneId == 58) baseFreq = 270.0f; // Low bongo
    else if (currentToneId == 59) baseFreq = 318.0f; // Mute hi conga
    else if (currentToneId == 60) baseFreq = 295.0f; // Open hi conga
    else if (currentToneId == 61) baseFreq = 196.0f; // Low conga
    else if (currentToneId == 62) baseFreq = 256.0f; // Hi timbale
    else if (currentToneId == 63) baseFreq = 182.0f; // Low timbale

    baseFreq *= tuneRatio;
    const float pitchBend = 1.0f + (isTimbale ? 0.42f : 0.32f) * std::exp(-envTime * 48.0f);
    phase1 += baseFreq * pitchBend * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
    phase2 += baseFreq * (isTimbale ? 1.48f : 1.54f) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
    phase3 += baseFreq * 2.14f * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;

    const float decayRate = (currentToneId == 59 ? 25.0f : (isTimbale ? 7.2f : 8.8f)) / decayScale;
    const float env = std::exp(-envTime * decayRate);
    if (env < 0.0004f) { active = false; return 0.0f; }

    const float rawSlap = nextWhiteNoise() * std::exp(-envTime * (isTimbale ? 95.0f : 130.0f));
    svfLp1 += 0.35f * (rawSlap - svfLp1);
    const float slap = (isTimbale ? rawSlap : svfLp1) * (0.24f + ctrlParam * 0.45f);
    const float shell = std::sin(phase1 * kTwoPi) * 0.82f
                      + std::sin(phase2 * kTwoPi) * (isTimbale ? 0.38f : 0.22f)
                      + (isTimbale ? std::sin(phase3 * kTwoPi) * std::exp(-envTime * 28.0f) * 0.24f : 0.0f);
    return std::tanh(shell * env + slap);
}

// ==============================================================================
// TR-606 DRUMATIX TRANSISTOR RHYTHM MODELS (id 72..80)
// ==============================================================================
float AcbVoice::render606Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // 1. 606 bass drum (compact 58 Hz bridged-T, fast pitch chirp, transistor pulse)
    if (cat == 1) {
        const float f = 58.0f * tuneRatio * (1.0f + 1.55f * std::exp(-envTime * 58.0f));
        phase1 += f * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        const float env = std::exp(-envTime * (6.2f / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        const float click = std::sin(envTime * kTwoPi * 1250.0f) * std::exp(-envTime * 480.0f) *
                            (0.32f + ctrlParam * 0.55f);
        return std::tanh((std::sin(phase1 * kTwoPi) * env + click) * 1.35f);
    }

    // 2. 606 snare drum (dual resonators plus a crisp high-passed wire burst)
    if (cat == 2) {
        const float sweep = 1.0f + 0.35f * std::exp(-envTime * 110.0f);
        phase1 += 238.0f * tuneRatio * sweep * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += 445.0f * tuneRatio * sweep * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float toneEnv = std::exp(-envTime * (21.0f / decayScale));
        const float wireEnv = std::exp(-envTime * (12.5f / decayScale));
        if (toneEnv < 0.0004f && wireEnv < 0.0004f) { active = false; return 0.0f; }

        const float body = (std::sin(phase1 * kTwoPi) * 0.62f + std::sin(phase2 * kTwoPi) * 0.38f) * toneEnv;
        const float n = nextWhiteNoise();
        const float f = 2.0f * std::sin(kPi * std::min(0.22f, (3200.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = n - svfLp1 - 0.45f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        return std::tanh((body * 0.8f + (hp * 0.7f + svfBp1 * 0.5f) * wireEnv * (0.35f + ctrlParam * 1.15f)) * 1.28f);
    }

    // 3. 606 hi-hats (77 CH, 78 OH) and cymbals (79, 80)
    if (cat == 6 || cat == 7) {
        static constexpr float freqs606[6] = { 245.0f, 306.0f, 365.0f, 415.0f, 540.0f, 630.0f };
        float sum = 0.0f;
        for (size_t i = 0; i < 6; ++i) {
            metalPhases[i] += (freqs606[i] * tuneRatio * 1.15f) * dt;
            if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
            sum += (metalPhases[i] < 0.48f) ? 1.0f : -0.92f;
        }
        const float intermod = std::tanh(sum * 0.7f);
        const float f1 = 2.0f * std::sin(kPi * std::min(0.21f, (4800.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp1 = intermod - svfLp1 - 0.25f * svfBp1;
        svfBp1 += f1 * hp1;
        svfLp1 += f1 * svfBp1;

        const bool isCym = (cat == 7 || currentToneId >= 79);
        const bool isOpen = (!isCym) && (currentToneId == 78 || channelIndex == 8);
        const float decayRate = isCym ? ((currentToneId == 80 ? 3.8f : 1.7f) / decayScale)
                                      : (isOpen ? (4.5f / decayScale) : (28.0f / decayScale));
        const float env = std::exp(-envTime * decayRate);
        if (env < 0.0004f) { active = false; return 0.0f; }
        return std::tanh((hp1 * 0.85f + svfBp1 * 0.75f) * env * 1.35f);
    }

    return render808Model(cat);
}

// ==============================================================================
// CR-78 COMPURHYTHM MODELS (firmware v3.00 ids 159..173)
// ==============================================================================
float AcbVoice::renderCR78Model(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 1.5f);
    const float decayScale = 0.25f + decayParam * 2.0f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // CR-78 guiro (171)
    if (currentToneId == 171) {
        const float dur = 0.38f * decayScale;
        if (envTime > dur) { active = false; return 0.0f; }
        const float ratchet = 0.5f + 0.5f * std::sin(envTime * kTwoPi * 74.0f);
        phase1 += (1450.0f + envTime * 1150.0f) * tuneRatio * dt;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        return std::sin(phase1 * kTwoPi) * ratchet * (1.0f - envTime / dur) * 0.88f;
    }

    // CR-78 metal beat (172) / tambourine (170) / cowbell (169)
    if (currentToneId >= 169 && currentToneId <= 172) {
        static constexpr float crMetal[4] = { 1120.0f, 1680.0f, 2490.0f, 3310.0f };
        float sum = 0.0f;
        for (size_t i = 0; i < 4; ++i) {
            metalPhases[i] += crMetal[i] * tuneRatio * dt;
            if (metalPhases[i] >= 1.0f) metalPhases[i] -= 1.0f;
            sum += (metalPhases[i] < 0.5f) ? 1.0f : -1.0f;
        }
        const float f = 2.0f * std::sin(kPi * std::min(0.22f, (2650.0f * tuneRatio) / static_cast<float>(sampleRate)));
        const float hp = sum * 0.25f - svfLp1 - 0.3f * svfBp1;
        svfBp1 += f * hp;
        svfLp1 += f * svfBp1;
        const float env = std::exp(-envTime * (12.5f / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        return (svfBp1 * 0.9f + hp * 0.4f) * env * 1.25f;
    }

    // Warm LC-tank CR-78 bass/snare/bongo/hi-hat.
    return render808Model(cat) * 0.94f;
}

// ==============================================================================
// 2-OPERATOR FM DRUM ENGINE WITH MORPH MACRO (firmware v2.00 tones 86..156)
// ==============================================================================
float AcbVoice::renderFMModel(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 2.0f);
    const float decayScale = 0.2f + decayParam * 2.2f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    float baseCarrier = 110.0f;
    float modRatio = 1.414f + (currentToneId % 7) * 0.37f;
    float modIndexBase = 1.8f + ctrlParam * 6.5f; // CTRL = FM morph macro
    float pitchSweepAmt = 0.5f;
    float decayRate = 8.0f / decayScale;

    if (cat == 1) { // FM kicks (86..96)
        baseCarrier = (currentToneId == 91 || currentToneId == 93) ? 45.0f : 52.0f;
        modRatio = 1.0f + (currentToneId % 4) * 0.5f;
        pitchSweepAmt = (currentToneId == 90 || currentToneId == 95) ? 4.2f : 3.2f;
        decayRate = (currentToneId == 91 ? 2.8f : 4.2f) / decayScale;
    } else if (cat == 2) { // FM snares (97..107)
        baseCarrier = 180.0f;
        modRatio = 2.37f + (currentToneId % 5) * 0.41f;
        pitchSweepAmt = 1.25f;
        decayRate = 10.5f / decayScale;
    } else if (cat == 3 || cat == 8) { // FM toms / perc (108..112, 131..134)
        baseCarrier = (channelIndex == 2) ? 95.0f : (channelIndex == 3 ? 140.0f : 195.0f);
        modRatio = 1.732f;
        pitchSweepAmt = 1.45f;
        decayRate = 6.5f / decayScale;
    } else if (cat == 6 || cat == 7) { // FM hats / cymbals (118..130)
        baseCarrier = 720.0f;
        modRatio = 3.14159f;
        modIndexBase = 4.8f + ctrlParam * 8.5f;
        decayRate = (channelIndex == 7 || currentToneId <= 119) ? (24.0f / decayScale) : (3.2f / decayScale);
    } else if (cat == 13) { // FM riser / laser / sci-fi FX (148..155)
        baseCarrier = (currentToneId == 149) ? (180.0f + envTime * 950.0f) : 420.0f;
        modRatio = 2.618f;
        pitchSweepAmt = (currentToneId == 150) ? 5.5f : -0.6f;
        decayRate = 3.5f / decayScale;
    }

    const float pitchEnv = std::max(0.15f, 1.0f + pitchSweepAmt * std::exp(-envTime * 58.0f));
    const float cFreq = baseCarrier * tuneRatio * pitchEnv;
    const float mFreq = cFreq * modRatio;

    phase2 += mFreq * dt;
    if (phase2 >= 1.0f) phase2 -= 1.0f;

    const float modEnv = std::exp(-envTime * (decayRate * 1.75f));
    const float modSig = std::sin(phase2 * kTwoPi) * modIndexBase * modEnv;

    phase1 += cFreq * dt;
    if (phase1 >= 1.0f) phase1 -= 1.0f;

    const float ampEnv = std::exp(-envTime * decayRate);
    if (ampEnv < 0.0004f) { active = false; return 0.0f; }

    float fmOut = std::sin(phase1 * kTwoPi + modSig) * ampEnv;
    if (cat == 2 || cat == 5) fmOut += nextWhiteNoise() * modEnv * 0.35f;
    const float drive = (currentToneId == 95) ? (2.4f + ctrlParam * 2.5f) : 1.32f; // Gabber FM kick
    return std::tanh(fmOut * drive);
}

// ==============================================================================
// PRESET SYNTH / BASS / VOICE / OSC / CHORD / FX MODELS (tones 355..521)
// ==============================================================================
float AcbVoice::renderPCMOrSynthModel(int cat) {
    const float tuneRatio = std::pow(2.0f, (tuneParam - 0.5f) * 2.0f);
    const float decayScale = 0.25f + decayParam * 2.2f;
    const float dt = 1.0f / static_cast<float>(sampleRate);

    // 1. Voice / formant / vocoder tones (cat 14)
    if (cat == 14) {
        const float f0 = 130.8f * tuneRatio;
        phase1 += f0 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        const float glottal = (2.0f * phase1 - 1.0f) + 0.5f * std::sin(phase1 * kTwoPi * 2.0f);
        const float formant1 = 720.0f + (currentToneId % 5) * 280.0f;
        const float formant2 = 1250.0f + (currentToneId % 5) * 420.0f;
        const float f1 = 2.0f * std::sin(kPi * std::min(0.20f, formant1 / static_cast<float>(sampleRate)));
        const float f2 = 2.0f * std::sin(kPi * std::min(0.22f, formant2 / static_cast<float>(sampleRate)));
        svfBp1 += f1 * (glottal - svfLp1 - 0.18f * svfBp1); svfLp1 += f1 * svfBp1;
        svfBp2 += f2 * (glottal - svfLp2 - 0.22f * svfBp2); svfLp2 += f2 * svfBp2;
        const float env = std::exp(-envTime * (4.8f / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        return std::tanh((svfBp1 * 0.7f + svfBp2 * 0.5f) * env * 1.4f);
    }

    // 2. Raw oscillator waveforms (cat 16: sine, tri, square, pulse, super saw, noise)
    if (cat == 16) {
        const float env = std::exp(-envTime * (4.2f / decayScale));
        if (env < 0.0004f) { active = false; return 0.0f; }
        if (currentToneId >= 473) return nextWhiteNoise() * env * 0.75f;

        const bool isHigh = ((currentToneId & 1) == 0);
        const float f0 = (isHigh ? 220.0f : 110.0f) * tuneRatio;
        phase1 += f0 * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += (f0 * 1.012f) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
        phase3 += (f0 * 1.4983f) * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;

        float sig = 0.0f;
        if (currentToneId <= 454) sig = std::sin(phase1 * kTwoPi);
        else if (currentToneId <= 458) sig = 4.0f * std::abs(phase1 - 0.5f) - 1.0f;
        else if (currentToneId <= 464) sig = (phase1 < (0.25f + ctrlParam * 0.5f)) ? 0.85f : -0.85f;
        else sig = ((2.0f * phase1 - 1.0f) + (2.0f * phase2 - 1.0f) + (2.0f * phase3 - 1.0f)) * 0.35f;
        return std::tanh(sig * env * 1.2f);
    }

    // 3. Chords (cat 19), leads/pads (15), basses (17), mallets (18), FX (12, 13)
    float baseFreq = (cat == 17) ? 55.0f : (cat == 18 ? 261.63f : 220.0f);
    baseFreq *= tuneRatio;

    const bool isMajor = (currentToneId == 506 || currentToneId == 508 || currentToneId == 509 ||
                          currentToneId == 517 || currentToneId == 520);
    const float thirdRatio = isMajor ? 1.2599f : 1.1892f;
    phase1 += baseFreq * dt; if (phase1 >= 1.0f) phase1 -= 1.0f;
    phase2 += (baseFreq * thirdRatio) * dt; if (phase2 >= 1.0f) phase2 -= 1.0f;
    phase3 += (baseFreq * 1.4983f) * dt; if (phase3 >= 1.0f) phase3 -= 1.0f;
    phase4 += (baseFreq * 1.7818f) * dt; if (phase4 >= 1.0f) phase4 -= 1.0f; // 7th

    const float env = std::exp(-envTime * ((cat == 17 ? 3.2f : 4.2f) / decayScale));
    if (env < 0.0004f) { active = false; return 0.0f; }

    const float saw1 = 2.0f * phase1 - 1.0f;
    const float saw2 = 2.0f * phase2 - 1.0f;
    const float saw3 = 2.0f * phase3 - 1.0f;
    const float saw4 = 2.0f * phase4 - 1.0f;
    const float raw = (cat == 19)
        ? ((saw1 + saw2 + saw3 + saw4 * 0.7f) * 0.28f)
        : (std::sin(phase1 * kTwoPi) * 0.72f + saw1 * 0.32f);

    const float cutoff = std::min(0.42f, (380.0f + ctrlParam * 4400.0f * env) / static_cast<float>(sampleRate));
    svfLp1 += cutoff * (raw - svfLp1);
    return std::tanh(svfLp1 * env * 1.38f);
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
