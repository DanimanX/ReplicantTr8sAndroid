#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include <oboe/Oboe.h>

#include "engine.h"

namespace rt8 {

class AudioDriver final : public oboe::AudioStreamCallback {
public:
    explicit AudioDriver(Engine* engine);
    ~AudioDriver() override;

    bool start();
    void stop();
    bool isRunning() const { return running_.load(std::memory_order_relaxed); }
    void renderSilentIfIdle(int frames);

    struct Stats {
        double latencyMs = 0.0;
        float cpuLoad = 0.0f;
        int xRuns = 0;
        int sampleRate = 48000;
        int framesPerBurst = 0;
        int bufferFrames = 0;
        int api = 0;      // 1 = AAudio, 2 = OpenSL ES
        int sharing = 0;  // 1 = Exclusive, 2 = Shared
    };
    // Safe to call from the UI thread. It refreshes the stream timestamp and
    // xrun counters, and grows the buffer if the device is underrunning.
    Stats stats();

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData,
                                          int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    bool openLocked();
    void closeLocked();
    void watchdogLoop();
    void refreshMetrics();

    Engine* engine_ = nullptr;

    mutable std::mutex mutex_;
    std::shared_ptr<oboe::AudioStream> stream_;
    std::atomic<bool> running_ { false };
    std::atomic<bool> stopping_ { false };
    // True only while the host (JNI/service) wants audio. The watchdog is not
    // allowed to resurrect a stream after an explicit stop().
    std::atomic<bool> desiredRunning_ { false };
    // Set while the Oboe callback owns the engine, so the silent ticker can
    // never render into the same voice/FX state concurrently.
    std::atomic<bool> callbackActive_ { false };

    std::thread watchdog_;
    std::mutex restartMutex_;
    std::condition_variable restartCv_;
    bool restartPending_ = false;
    bool quit_ = false;

    std::mutex metricsMutex_;
    std::atomic<double> latencyMs_ { 0.0 };
    std::atomic<float> cpuLoad_ { 0.0f };
    std::atomic<int> xRuns_ { 0 };
    std::atomic<int> sampleRate_ { 48000 };
    std::atomic<int> framesPerBurst_ { 0 };
    std::atomic<int> bufferFrames_ { 0 };
    std::atomic<int> api_ { 0 };
    std::atomic<int> sharing_ { 0 };
    std::atomic<int64_t> lastMetricUpdateNanos_ { 0 };
};

} // namespace rt8
