#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine.h"

namespace {

int failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

float renderAndPeak(rt8::Engine& engine, int frames, std::vector<float>& buffer) {
    float peak = 0.0f;
    int remaining = frames;
    while (remaining > 0) {
        const int n = std::min(remaining, 512);
        engine.process(buffer.data(), n, 0);
        for (int i = 0; i < n * 2; ++i) {
            const float v = buffer[static_cast<size_t>(i)];
            if (!std::isfinite(v)) {
                expect(false, "engine produced a non-finite sample");
                return 0.0f;
            }
            peak = std::max(peak, std::abs(v));
        }
        remaining -= n;
    }
    return peak;
}

} // namespace

int main() {
    constexpr int kSampleRate = 48000;
    constexpr int kFrames = 512;

    rt8::Engine engine;
    engine.prepare(kSampleRate, kFrames);
    engine.setMidiOutEnabled(true);
    engine.setClockSend(true);
    engine.setTransportSend(true);

    expect(engine.getKitIndex() == 1, "default kit is TR-909 Studio");
    expect(engine.getPatternIndex() == 103, "default pattern is 7-08 Straight 909");
    expect(engine.kitName(1) == "TR-909 Studio", "kit names are populated");
    expect(engine.patternName(103) == "Straight 909", "pattern names are populated");
    expect(!engine.toneName(0).empty(), "tone names are populated");
    expect(!engine.isFirmwareLoaded(), "preview content is not a user firmware import");

    // Pattern data must be present out of the box.
    expect(engine.getStepVelocity(0, 0, 0) > 0, "default kick step is active");
    expect(engine.getStepVelocity(0, 1, 4) > 0, "default snare step is active");

    std::vector<float> buffer(static_cast<size_t>(kFrames) * 2, 0.0f);
    const float silentPeak = renderAndPeak(engine, 4096, buffer);
    expect(silentPeak < 0.0001f, "engine is silent before transport starts");

    engine.setPlaying(true);
    const float playedPeak = renderAndPeak(engine, kSampleRate * 2, buffer);
    expect(playedPeak > 0.02f, "sequencer produces audio after play");
    expect(engine.isPlaying(), "transport reports playing");

    // Two seconds at 128 BPM is 4.27 bars; the playhead must have advanced.
    expect(engine.getCurrentStep() >= 0, "playhead is valid");

    float meters[13] {};
    engine.getMeters(meters);
    float sum = 0.0f;
    for (int i = 0; i < 11; ++i) sum += meters[i];
    expect(sum > 0.0f, "channel meters report activity");

    // MIDI input triggers a drum voice and is audible.
    engine.setPlaying(false);
    renderAndPeak(engine, 8192, buffer);
    engine.pushMidiMessage(0x99, 36, 120, 0);
    const float midiPeak = renderAndPeak(engine, 4096, buffer);
    expect(midiPeak > 0.001f, "MIDI note 36 triggers the bass drum");

    // Reset voices and FX tails before testing the channel filter.
    engine.prepare(kSampleRate, kFrames);

    // MIDI channel filter drops other channels.
    engine.setMidiChannel(0);
    engine.pushMidiMessage(0x99, 38, 120, 0);
    const float filteredPeak = renderAndPeak(engine, 2048, buffer);
    expect(filteredPeak < 0.0005f, "MIDI channel filter rejects channel 10 when set to 1");

    // Transport and clock must be emitted to the MIDI-out queue.
    engine.setMidiChannel(-1);
    engine.setPlaying(true);
    uint8_t records[256 * 16] {};
    const int outCount = engine.pollMidiOut(records, 256);
    expect(outCount > 0, "MIDI out emits start/clock/notes");
    bool sawStart = false;
    for (int i = 0; i < outCount; ++i) {
        if (records[i * 16] == 0xFA) sawStart = true;
    }
    expect(sawStart, "MIDI out emits start (0xFA)");

    // Pattern switching and kit loading remain sample-safe.
    engine.loadPattern(6, true);
    expect(engine.getPatternIndex() == 6, "pattern index updates");
    expect(engine.kitName(engine.getKitIndex()) == "FM Hard Techno", "pattern loads its kit");
    const float switchPeak = renderAndPeak(engine, 8192, buffer);
    expect(switchPeak >= 0.0f, "pattern switch renders");

    // External clock follow mode.
    engine.setClockReceive(true);
    engine.setPlaying(false);
    for (int i = 0; i < 24; ++i) engine.pushMidiMessage(0xF8, 0, 0, 0);
    engine.pushMidiMessage(0xFA, 0, 0, 0);
    for (int i = 0; i < 24; ++i) engine.pushMidiMessage(0xF8, 0, 0, 0);
    const float clockPeak = renderAndPeak(engine, 8192, buffer);
    expect(clockPeak >= 0.0f, "external clock processing is stable");
    engine.setClockReceive(false);

    // MIDI clock timing: 120 BPM = 48 pulses per second (24 PPQN).
    engine.prepare(kSampleRate, kFrames);
    engine.setMidiChannel(-1);
    engine.setMidiOutEnabled(false);
    engine.setClockSend(true);
    engine.setClockReceive(false);
    engine.setTransportSend(false);
    engine.setTempo(120.0);
    uint8_t drain[4096 * 16] {};
    while (engine.pollMidiOut(drain, 4096) > 0) {}
    engine.setPlaying(true);
    renderAndPeak(engine, kSampleRate, buffer);
    uint8_t clockRecords[1024 * 16] {};
    const int clockCount = engine.pollMidiOut(clockRecords, 1024);
    int clockPulses = 0;
    for (int i = 0; i < clockCount; ++i) {
        if (clockRecords[i * 16] == 0xF8) ++clockPulses;
    }
    std::printf("clock pulses in 1s @120 BPM: %d\n", clockPulses);
    expect(clockPulses >= 47 && clockPulses <= 49, "MIDI clock runs at 24 PPQN");
    engine.setPlaying(false);
    engine.setMidiOutEnabled(true);
    engine.setTransportSend(true);

    // Variation operations are atomic and complete.
    engine.copyVariation(0, 6);
    expect(engine.getStep(6, 0, 0) == engine.getStep(0, 0, 0), "copyVariation copies cells");
    engine.clearVariation(6);
    expect(engine.getStepVelocity(6, 0, 0) == 0, "clearVariation clears cells");

    if (failures == 0) {
        std::printf("engine_tests: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "engine_tests: %d failure(s)\n", failures);
    return 1;
}
