#include <jni.h>

#include <memory>
#include <string>

#include "engine/audio_driver.h"
#include "engine/engine.h"

namespace {

struct NativeContext {
    std::unique_ptr<rt8::Engine> engine;
    std::unique_ptr<rt8::AudioDriver> driver;
};

NativeContext* context(jlong handle) {
    return reinterpret_cast<NativeContext*>(handle);
}

rt8::Engine* engine(jlong handle) {
    NativeContext* ctx = context(handle);
    return ctx != nullptr ? ctx->engine.get() : nullptr;
}


} // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeCreate(JNIEnv*, jobject) {
    auto ctx = std::make_unique<NativeContext>();
    ctx->engine = std::make_unique<rt8::Engine>();
    ctx->engine->prepare(48000.0, 512);
    ctx->driver = std::make_unique<rt8::AudioDriver>(ctx->engine.get());
    return reinterpret_cast<jlong>(ctx.release());
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    delete context(handle);
}

JNIEXPORT jboolean JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeStartAudio(JNIEnv*, jobject, jlong handle) {
    NativeContext* ctx = context(handle);
    if (ctx == nullptr || !ctx->driver) return JNI_FALSE;
    return ctx->driver->start() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeStopAudio(JNIEnv*, jobject, jlong handle) {
    NativeContext* ctx = context(handle);
    if (ctx != nullptr && ctx->driver) ctx->driver->stop();
}

JNIEXPORT jboolean JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeIsAudioRunning(JNIEnv*, jobject, jlong handle) {
    NativeContext* ctx = context(handle);
    if (ctx == nullptr || !ctx->driver) return JNI_FALSE;
    return ctx->driver->isRunning() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeRenderSilent(JNIEnv*, jobject, jlong handle,
                                                                     jint frames) {
    NativeContext* ctx = context(handle);
    if (ctx == nullptr) return;
    if (ctx->driver) ctx->driver->renderSilentIfIdle(frames);
    else if (ctx->engine) ctx->engine->renderSilent(frames);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetAudioStats(JNIEnv* env, jobject, jlong handle,
                                                                      jfloatArray floats, jintArray ints) {
    NativeContext* ctx = context(handle);
    if (ctx == nullptr || !ctx->driver) return;
    const auto stats = ctx->driver->stats();
    if (floats != nullptr && env->GetArrayLength(floats) >= 2) {
        const jfloat values[2] = { static_cast<jfloat>(stats.latencyMs), static_cast<jfloat>(stats.cpuLoad) };
        env->SetFloatArrayRegion(floats, 0, 2, values);
    }
    if (ints != nullptr && env->GetArrayLength(ints) >= 6) {
        const jint values[6] = { stats.xRuns, stats.sampleRate, stats.framesPerBurst,
                                 stats.bufferFrames, stats.api, stats.sharing };
        env->SetIntArrayRegion(ints, 0, 6, values);
    }
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetParams(JNIEnv* env, jobject, jlong handle,
                                                                  jfloatArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return;
    const jsize len = env->GetArrayLength(out);
    if (len < 96) return;
    jfloat buffer[96] {};
    e->getParams(buffer);
    env->SetFloatArrayRegion(out, 0, 96, buffer);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetStatus(JNIEnv* env, jobject, jlong handle,
                                                                  jintArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return;
    if (env->GetArrayLength(out) < 64) return;
    jint buffer[64] {};
    e->getStatus(buffer);
    env->SetIntArrayRegion(out, 0, 64, buffer);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetScope(JNIEnv* env, jobject, jlong handle,
                                                                 jfloatArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return;
    if (env->GetArrayLength(out) < 128) return;
    jfloat buffer[128] {};
    e->getScope(buffer);
    env->SetFloatArrayRegion(out, 0, 128, buffer);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetMeters(JNIEnv* env, jobject, jlong handle,
                                                                  jfloatArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return;
    if (env->GetArrayLength(out) < 13) return;
    jfloat buffer[13] {};
    e->getMeters(buffer);
    env->SetFloatArrayRegion(out, 0, 13, buffer);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetPatternRow(JNIEnv* env, jobject, jlong handle,
                                                                      jint variation, jint track,
                                                                      jbyteArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return;
    if (env->GetArrayLength(out) < 32) return;
    uint8_t buffer[32] {};
    e->getPatternRow(variation, track, buffer);
    env->SetByteArrayRegion(out, 0, 32, reinterpret_cast<const jbyte*>(buffer));
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetPlaying(JNIEnv*, jobject, jlong handle,
                                                                   jboolean play) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setPlaying(play == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetTempo(JNIEnv*, jobject, jlong handle,
                                                                 jdouble bpm) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setTempo(static_cast<double>(bpm));
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetShuffle(JNIEnv*, jobject, jlong handle,
                                                                   jfloat value) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setShuffle(value);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetAccent(JNIEnv*, jobject, jlong handle,
                                                                  jfloat value) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setAccent(value);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetVariation(JNIEnv*, jobject, jlong handle,
                                                                     jint variation) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setVariation(variation);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetFill(JNIEnv*, jobject, jlong handle,
                                                                jboolean fill) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setFill(fill == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeToggleFill(JNIEnv*, jobject, jlong handle) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->toggleFill();
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetSelectedChannel(JNIEnv*, jobject, jlong handle,
                                                                           jint channel) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setSelectedChannel(channel);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetChannelParam(JNIEnv*, jobject, jlong handle,
                                                                        jint channel, jint param,
                                                                        jfloat value) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setChannelParam(channel, param, value);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetMasterParam(JNIEnv*, jobject, jlong handle,
                                                                       jint param, jfloat value) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setMasterParam(param, value);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetTone(JNIEnv*, jobject, jlong handle,
                                                                jint channel, jint toneId) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setTone(channel, toneId);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetMute(JNIEnv*, jobject, jlong handle,
                                                                jint channel, jboolean muted) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setMute(channel, muted == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetSolo(JNIEnv*, jobject, jlong handle,
                                                                jint channel, jboolean soloed) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setSolo(channel, soloed == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeLoadKit(JNIEnv*, jobject, jlong handle,
                                                                jint index) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->loadKit(index);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeLoadPattern(JNIEnv*, jobject, jlong handle,
                                                                    jint index, jboolean loadKit) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->loadPattern(index, loadKit == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeCopyVariation(JNIEnv*, jobject, jlong handle,
                                                                      jint src, jint dst) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->copyVariation(src, dst);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeClearVariation(JNIEnv*, jobject, jlong handle,
                                                                       jint variation) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->clearVariation(variation);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetStep(JNIEnv*, jobject, jlong handle,
                                                               jint variation, jint track, jint step,
                                                               jint velocity, jint flags) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setStep(variation, track, step, velocity, flags);
}

JNIEXPORT jint JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeGetStep(JNIEnv*, jobject, jlong handle,
                                                               jint variation, jint track, jint step) {
    rt8::Engine* e = engine(handle);
    return e != nullptr ? e->getStep(variation, track, step) : 0;
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeTrigger(JNIEnv*, jobject, jlong handle,
                                                                jint channel, jfloat velocity,
                                                                jboolean flam) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->trigger(channel, velocity, flam == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetScatterActive(JNIEnv*, jobject, jlong handle,
                                                                         jboolean active) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setScatterActive(active == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetScatterType(JNIEnv*, jobject, jlong handle,
                                                                       jint type) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setScatterType(type);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativePushMidi(JNIEnv*, jobject, jlong handle,
                                                                 jint status, jint data1, jint data2,
                                                                 jlong timestampNanos) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr)
        e->pushMidiMessage(static_cast<uint8_t>(status), static_cast<uint8_t>(data1),
                           static_cast<uint8_t>(data2), static_cast<int64_t>(timestampNanos));
}

JNIEXPORT jint JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativePollMidiOut(JNIEnv* env, jobject, jlong handle,
                                                                    jbyteArray out) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || out == nullptr) return 0;
    const jsize len = env->GetArrayLength(out);
    const int maxRecords = len / 16;
    if (maxRecords <= 0) return 0;
    jbyte* bytes = env->GetByteArrayElements(out, nullptr);
    if (bytes == nullptr) return 0;
    const int count = e->pollMidiOut(reinterpret_cast<uint8_t*>(bytes), maxRecords);
    env->ReleaseByteArrayElements(out, bytes, 0);
    return count;
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetMidiOutEnabled(JNIEnv*, jobject, jlong handle,
                                                                          jboolean enabled) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setMidiOutEnabled(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetClockSend(JNIEnv*, jobject, jlong handle,
                                                                     jboolean enabled) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setClockSend(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetClockReceive(JNIEnv*, jobject, jlong handle,
                                                                        jboolean enabled) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setClockReceive(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetTransportSend(JNIEnv*, jobject, jlong handle,
                                                                         jboolean enabled) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setTransportSend(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeSetMidiChannel(JNIEnv*, jobject, jlong handle,
                                                                       jint channel) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->setMidiChannel(channel);
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeLoadFirmware(JNIEnv* env, jobject, jlong handle,
                                                                     jbyteArray data) {
    rt8::Engine* e = engine(handle);
    if (e == nullptr || data == nullptr) return env->NewStringUTF("Engine is not ready.");
    const jsize len = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (bytes == nullptr) return env->NewStringUTF("Unable to read firmware bytes.");

    std::string error;
    const bool ok = e->loadFirmware(reinterpret_cast<const uint8_t*>(bytes),
                                    static_cast<size_t>(len), error);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
    if (!ok && error.empty()) error = "Firmware could not be parsed.";
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT void JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeClearFirmware(JNIEnv*, jobject, jlong handle) {
    rt8::Engine* e = engine(handle);
    if (e != nullptr) e->clearFirmware();
}

JNIEXPORT jboolean JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeIsFirmwareLoaded(JNIEnv*, jobject, jlong handle) {
    rt8::Engine* e = engine(handle);
    return (e != nullptr && e->isFirmwareLoaded()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeFirmwareName(JNIEnv* env, jobject, jlong handle) {
    rt8::Engine* e = engine(handle);
    return env->NewStringUTF(e != nullptr ? e->firmwareName().c_str() : "None");
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeKitName(JNIEnv* env, jobject, jlong handle,
                                                                jint index) {
    rt8::Engine* e = engine(handle);
    return env->NewStringUTF(e != nullptr ? e->kitName(index).c_str() : "INIT");
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativePatternName(JNIEnv* env, jobject, jlong handle,
                                                                    jint index) {
    rt8::Engine* e = engine(handle);
    return env->NewStringUTF(e != nullptr ? e->patternName(index).c_str() : "INIT");
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativePatternCode(JNIEnv* env, jobject, jlong handle,
                                                                    jint index) {
    rt8::Engine* e = engine(handle);
    return env->NewStringUTF(e != nullptr ? e->patternCode(index).c_str() : "--");
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeToneName(JNIEnv* env, jobject, jlong handle,
                                                                jint index) {
    rt8::Engine* e = engine(handle);
    return env->NewStringUTF(e != nullptr ? e->toneName(index).c_str() : "Tone");
}

JNIEXPORT jint JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeKitToneId(JNIEnv*, jobject, jlong handle,
                                                                 jint kitIndex, jint channel) {
    rt8::Engine* e = engine(handle);
    return e != nullptr ? e->kitToneId(kitIndex, channel) : 0;
}

JNIEXPORT jstring JNICALL
Java_com_danimanx_replicanttr8s_audio_NativeEngine_nativeVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("ReplicantTR8S Android engine 1.0.1");
}

} // extern "C"
