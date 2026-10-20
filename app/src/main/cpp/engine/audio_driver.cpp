#include "audio_driver.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace rt8 {

namespace {
constexpr const char* kTag = "ReplicatTr8sAudio";

int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class CallbackScope {
public:
    explicit CallbackScope(std::atomic<bool>& flag) : flag_(flag) {
        flag_.store(true, std::memory_order_release);
    }
    ~CallbackScope() { flag_.store(false, std::memory_order_release); }

private:
    std::atomic<bool>& flag_;
};
} // namespace

AudioDriver::AudioDriver(Engine* engine) : engine_(engine) {
    watchdog_ = std::thread([this] { watchdogLoop(); });
}

AudioDriver::~AudioDriver() {
    {
        std::lock_guard<std::mutex> lock(restartMutex_);
        quit_ = true;
    }
    restartCv_.notify_all();
    if (watchdog_.joinable()) watchdog_.join();
    stop();
}

bool AudioDriver::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    desiredRunning_.store(true, std::memory_order_relaxed);
    if (running_.load(std::memory_order_relaxed)) return true;
    return openLocked();
}

void AudioDriver::renderSilentIfIdle(int frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_.load(std::memory_order_relaxed)) return;
    if (callbackActive_.load(std::memory_order_acquire)) return;
    if (engine_ != nullptr) engine_->renderSilent(frames);
}

void AudioDriver::stop() {
    // Clear the intent first: a callback or watchdog entry that lands while
    // the stream is closing must not schedule a restart.
    desiredRunning_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(restartMutex_);
        restartPending_ = false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_.store(true, std::memory_order_relaxed);
    closeLocked();
    stopping_.store(false, std::memory_order_relaxed);
}

void AudioDriver::closeLocked() {
    if (stream_) {
        stream_->requestStop();
        stream_->close();
        stream_.reset();
    }
    running_.store(false, std::memory_order_relaxed);
}

bool AudioDriver::openLocked() {
    closeLocked();

    struct Attempt {
        oboe::AudioApi api;
        oboe::SharingMode sharing;
        int sampleRate;
    };
    // Native rate first: forcing 48 kHz on a device that runs at a different
    // rate forces a resampler and prevents AAudio from taking the exclusive
    // MMAP fast path, which is where the low latency actually comes from.
    const Attempt attempts[] = {
        {oboe::AudioApi::AAudio, oboe::SharingMode::Exclusive, 0},
        {oboe::AudioApi::AAudio, oboe::SharingMode::Exclusive, 48000},
        {oboe::AudioApi::AAudio, oboe::SharingMode::Shared, 0},
        {oboe::AudioApi::AAudio, oboe::SharingMode::Shared, 48000},
        {oboe::AudioApi::Unspecified, oboe::SharingMode::Shared, 0},
        {oboe::AudioApi::OpenSLES, oboe::SharingMode::Shared, 48000},
    };

    for (const auto& attempt : attempts) {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(attempt.sharing);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(oboe::ChannelCount::Stereo);
        builder.setSampleRate(attempt.sampleRate);
        builder.setUsage(oboe::Usage::Game);
        builder.setContentType(oboe::ContentType::Music);
        builder.setAudioApi(attempt.api);
        builder.setChannelConversionAllowed(true);
        builder.setFormatConversionAllowed(true);
        builder.setCallback(this);

        std::shared_ptr<oboe::AudioStream> stream;
        const oboe::Result openResult = builder.openStream(stream);
        if (openResult != oboe::Result::OK || !stream) {
            __android_log_print(ANDROID_LOG_WARN, kTag,
                                "open failed (api=%d, sharing=%d, sr=%d): %s",
                                static_cast<int>(attempt.api), static_cast<int>(attempt.sharing),
                                attempt.sampleRate, oboe::convertToText(openResult));
            continue;
        }

        if (stream->getChannelCount() != 2 || stream->getFormat() != oboe::AudioFormat::Float) {
            __android_log_print(ANDROID_LOG_WARN, kTag,
                                "unexpected stream format: channels=%d format=%d",
                                stream->getChannelCount(), static_cast<int>(stream->getFormat()));
            stream->close();
            continue;
        }

        const bool exclusive = stream->getSharingMode() == oboe::SharingMode::Exclusive;
        const int burst = std::max(1, stream->getFramesPerBurst());
        // Exclusive mode accepts a one-burst buffer (the lowest stable size).
        // Shared mode clamps to at least two bursts, so ask for that directly.
        const int targetFrames = burst * (exclusive ? 1 : 2);
        const auto bufferResult = stream->setBufferSizeInFrames(targetFrames);
        if (bufferResult) {
            __android_log_print(ANDROID_LOG_INFO, kTag, "buffer set to %d frames (burst %d)",
                                bufferResult.value(), burst);
        } else {
            __android_log_print(ANDROID_LOG_WARN, kTag, "setBufferSizeInFrames(%d) failed: %s",
                                targetFrames, oboe::convertToText(bufferResult.error()));
        }

        engine_->prepare(stream->getSampleRate(), std::max(512, burst * 4));

        const oboe::Result startResult = stream->requestStart();
        if (startResult != oboe::Result::OK) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "requestStart failed: %s",
                                oboe::convertToText(startResult));
            stream->close();
            continue;
        }

        // Some HALs reset the requested size while starting; re-assert it once
        // the stream is running and remember what the device actually granted.
        stream->setBufferSizeInFrames(targetFrames);

        stream_ = std::move(stream);
        running_.store(true, std::memory_order_relaxed);
        sampleRate_.store(stream_->getSampleRate(), std::memory_order_relaxed);
        framesPerBurst_.store(stream_->getFramesPerBurst(), std::memory_order_relaxed);
        bufferFrames_.store(stream_->getBufferSizeInFrames(), std::memory_order_relaxed);
        api_.store(stream_->getAudioApi() == oboe::AudioApi::AAudio ? 1 : 2,
                   std::memory_order_relaxed);
        sharing_.store(stream_->getSharingMode() == oboe::SharingMode::Exclusive ? 1 : 2,
                       std::memory_order_relaxed);
        latencyMs_.store(0.0, std::memory_order_relaxed);
        lastMetricUpdateNanos_.store(0, std::memory_order_relaxed);

        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "stream started: api=%d sharing=%d sr=%d burst=%d buffer=%d",
                            api_.load(), sharing_.load(), sampleRate_.load(),
                            framesPerBurst_.load(), bufferFrames_.load());
        return true;
    }

    __android_log_print(ANDROID_LOG_ERROR, kTag, "no audio stream could be opened");
    return false;
}

oboe::DataCallbackResult AudioDriver::onAudioReady(oboe::AudioStream* stream, void* audioData,
                                                   int32_t numFrames) {
    if (audioData == nullptr || engine_ == nullptr || numFrames <= 0)
        return oboe::DataCallbackResult::Continue;

    CallbackScope callbackScope(callbackActive_);

    // Keep the real-time callback minimal: no timestamps, xrun or buffer
    // queries here. Those run from stats() on the UI thread instead, because
    // even a short binder/HAL query inside the callback can miss the deadline.
    const int64_t renderStart = nowNanos();
    const double outputLatencyMs = latencyMs_.load(std::memory_order_relaxed);
    const int64_t eventClock = renderStart + static_cast<int64_t>(outputLatencyMs * 1.0e6);
    engine_->process(static_cast<float*>(audioData), numFrames, eventClock);

    const int64_t end = nowNanos();
    const int sampleRate = sampleRate_.load(std::memory_order_relaxed);
    const double budget = static_cast<double>(numFrames) / std::max(1, sampleRate);
    const float instant = budget > 0.0
                              ? static_cast<float>(static_cast<double>(end - renderStart) / 1e9 / budget)
                              : 0.0f;
    cpuLoad_.store(cpuLoad_.load(std::memory_order_relaxed) * 0.9f + instant * 0.1f,
                   std::memory_order_relaxed);

    return oboe::DataCallbackResult::Continue;
}

void AudioDriver::refreshMetrics() {
    std::shared_ptr<oboe::AudioStream> stream;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stream = stream_;
    }
    if (!stream || !running_.load(std::memory_order_relaxed)) return;

    std::lock_guard<std::mutex> metricsLock(metricsMutex_);
    const int64_t now = nowNanos();
    if (now - lastMetricUpdateNanos_.load(std::memory_order_relaxed) < 250'000'000LL) return;
    lastMetricUpdateNanos_.store(now, std::memory_order_relaxed);

    if (const auto latency = stream->calculateLatencyMillis())
        latencyMs_.store(latency.value(), std::memory_order_relaxed);

    const int burst = std::max(1, stream->getFramesPerBurst());
    if (const auto xruns = stream->getXRunCount()) {
        const int total = xruns.value();
        const int previous = xRuns_.exchange(total, std::memory_order_relaxed);
        if (total > previous) {
            // The device could not keep up with the current buffer. Grow it one
            // burst at a time (up to four) instead of letting the crackle
            // continue. Recovery is preferable to a permanently glitching
            // stream.
            const int current = stream->getBufferSizeInFrames();
            const int next = std::min(burst * 4, current + burst);
            if (next > current) {
                const auto result = stream->setBufferSizeInFrames(next);
                __android_log_print(ANDROID_LOG_WARN, kTag,
                                    "underrun detected (%d total); buffer %d -> %d frames: %s",
                                    total, current, next,
                                    result ? "ok" : oboe::convertToText(result.error()));
            }
        }
    }

    sampleRate_.store(stream->getSampleRate(), std::memory_order_relaxed);
    framesPerBurst_.store(burst, std::memory_order_relaxed);
    bufferFrames_.store(stream->getBufferSizeInFrames(), std::memory_order_relaxed);
}

void AudioDriver::onErrorAfterClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    if (stopping_.load(std::memory_order_relaxed)) return;
    if (!desiredRunning_.load(std::memory_order_relaxed)) return;
    __android_log_print(ANDROID_LOG_ERROR, kTag, "audio stream disconnected: %s",
                        oboe::convertToText(error));
    running_.store(false, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(restartMutex_);
        restartPending_ = true;
    }
    restartCv_.notify_one();
}

void AudioDriver::watchdogLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(restartMutex_);
        restartCv_.wait(lock, [this] { return quit_ || restartPending_; });
        if (quit_) break;
        restartPending_ = false;
        lock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!desiredRunning_.load(std::memory_order_relaxed)) continue;
        if (!stopping_.load(std::memory_order_relaxed))
            start();
    }
}

AudioDriver::Stats AudioDriver::stats() {
    refreshMetrics();

    Stats s;
    s.latencyMs = latencyMs_.load(std::memory_order_relaxed);
    s.cpuLoad = cpuLoad_.load(std::memory_order_relaxed);
    s.xRuns = xRuns_.load(std::memory_order_relaxed);
    s.sampleRate = sampleRate_.load(std::memory_order_relaxed);
    s.framesPerBurst = framesPerBurst_.load(std::memory_order_relaxed);
    s.bufferFrames = bufferFrames_.load(std::memory_order_relaxed);
    s.api = api_.load(std::memory_order_relaxed);
    s.sharing = sharing_.load(std::memory_order_relaxed);
    return s;
}

} // namespace rt8
