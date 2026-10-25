// Sweeps every TR-8S v3.00 tone id (0..521) through the ACB voice and checks
// that each one renders finite, audible audio at both host sample rates.
// This is the regression net for the ported circuit models: a broken model
// usually shows up as a NaN, a silent voice or an unbounded peak.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

#include "acb_voice.h"
#include "firmware.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// Channel index a tone normally lives on, so models that depend on channelIndex
// (hats, cymbals, toms) are exercised the way the kits use them.
int channelForCategory(int cat) {
    switch (cat) {
        case 1: return 0;
        case 2: return 1;
        case 3: return 2;
        case 4: return 5;
        case 5: return 6;
        case 6: return 7;
        case 7: return 9;
        case 8: return 2;
        case 9: return 5;
        case 10: return 7;
        case 11: return 10;
        case 12: return 3;
        default: return 0;
    }
}

struct SweepResult {
    int silent = 0;
    int nonFinite = 0;
    int unbounded = 0;
    float peak = 0.0f;
    int silentFirst = -1;
};

SweepResult sweep(rt8::AcbVoice& voice, const rt8::FirmwareBank& bank, int toneId) {
    SweepResult r;
    voice.setChannelIndex(channelForCategory(bank.tones[toneId].category));
    voice.setToneId(static_cast<uint16_t>(toneId));
    voice.trigger(0.9f, 0.5f, false);
    for (int i = 0; i < 8192; ++i) {
        float l = 0.0f;
        float rr = 0.0f;
        voice.renderNextSample(l, rr);
        if (!std::isfinite(l) || !std::isfinite(rr)) {
            ++r.nonFinite;
            break;
        }
        const float p = std::max(std::abs(l), std::abs(rr));
        if (p > r.peak) r.peak = p;
    }
    if (r.nonFinite == 0) {
        if (r.peak < 1.0e-4f) {
            r.silent = 1;
            r.silentFirst = toneId;
        }
        if (r.peak > 1.0f) r.unbounded = 1;
    }
    return r;
}

} // namespace

int main() {
    rt8::FirmwareBank bank;
    for (int i = 0; i < rt8::kNumTones; ++i) {
        auto& tone = bank.tones[i];
        tone.id = static_cast<uint16_t>(i);
        rt8::ToneMetaInfo meta;
        if (rt8::toneMetaForId(i, meta)) {
            std::snprintf(tone.name, sizeof(tone.name), "%s", meta.name);
            tone.category = meta.category;
            tone.engineType = meta.engineType;
        } else {
            std::snprintf(tone.name, sizeof(tone.name), "Tone %03d", i);
            tone.category = rt8::inferToneCategory(i);
        }
    }

    int silentTotal = 0;
    int nonFiniteTotal = 0;
    int unboundedTotal = 0;
    float globalPeak = 0.0f;
    int firstSilent = -1;

    static constexpr double rates[] = {44100.0, 48000.0};
    for (double rate : rates) {
        rt8::AcbVoice voice;
        voice.setBank(&bank);
        voice.prepare(rate);
        for (int id = 0; id < rt8::kNumTones; ++id) {
            const SweepResult r = sweep(voice, bank, id);
            silentTotal += r.silent;
            nonFiniteTotal += r.nonFinite;
            unboundedTotal += r.unbounded;
            if (r.silent && firstSilent < 0) firstSilent = id;
            if (r.peak > globalPeak) globalPeak = r.peak;
            if (r.nonFinite > 0)
                std::fprintf(stderr, "  tone %03d (%s) produced a non-finite sample at %.0f Hz\n",
                             id, bank.tones[id].name, rate);
        }
    }

    std::printf("tone sweep: %d tones x 2 rates, peak %.3f\n", rt8::kNumTones, globalPeak);
    if (firstSilent >= 0)
        std::printf("first silent tone: %03d (%s)\n", firstSilent, bank.tones[firstSilent].name);

    expect(nonFiniteTotal == 0, "every tone renders finite samples");
    expect(unboundedTotal == 0, "every tone stays inside the mixer ceiling");
    expect(silentTotal == 0, "every tone produces audio when triggered");
    expect(globalPeak > 0.05f, "tone sweep reaches a usable level");

    // A tone at full mixer level must never exceed the tanh output ceiling.
    rt8::AcbVoice loud;
    loud.setBank(&bank);
    loud.prepare(48000.0);
    loud.setChannelIndex(0);
    loud.setToneId(0);
    loud.setLevel(1.0f);
    loud.trigger(1.45f, 1.0f, false);
    float loudPeak = 0.0f;
    for (int i = 0; i < 48000; ++i) {
        float l = 0.0f;
        float r = 0.0f;
        loud.renderNextSample(l, r);
        if (!std::isfinite(l) || !std::isfinite(r)) {
            expect(false, "accented kick stays finite");
            break;
        }
        loudPeak = std::max({loudPeak, std::abs(l), std::abs(r)});
    }
    expect(loudPeak > 0.05f, "accented kick is audible");

    // Flam retriggers the voice and the choke path decays it to silence.
    rt8::AcbVoice flam;
    flam.setBank(&bank);
    flam.prepare(48000.0);
    flam.setChannelIndex(7);
    flam.setToneId(21);
    flam.trigger(0.9f, 0.0f, true);
    int flamTailTransitions = 0;
    bool wasActive = true;
    for (int i = 0; i < 4096; ++i) {
        float l = 0.0f;
        float r = 0.0f;
        flam.renderNextSample(l, r);
        if (wasActive && !flam.isActive()) ++flamTailTransitions;
        wasActive = flam.isActive();
    }
    expect(flamTailTransitions == 0, "flam keeps the voice alive for its tail");

    flam.trigger(0.9f, 0.0f, false);
    flam.choke();
    int chokedSamples = 0;
    while (flam.isActive() && chokedSamples < 48000) {
        float l = 0.0f;
        float r = 0.0f;
        flam.renderNextSample(l, r);
        ++chokedSamples;
    }
    expect(!flam.isActive(), "choke closes the hi-hat");
    expect(chokedSamples > 0 && chokedSamples < 400, "choke is a short analog VCA ramp");

    if (failures == 0) {
        std::printf("tone_sweep: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "tone_sweep: %d failure(s)\n", failures);
    return 1;
}
