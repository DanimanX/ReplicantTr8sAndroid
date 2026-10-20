# Build & verification

## Toolchain

| Tool | Version used |
| --- | --- |
| JDK | 21 (Temurin/OpenJDK) |
| Gradle | 8.11.1 (wrapper) |
| Android Gradle Plugin | 8.7.3 |
| Kotlin | 2.0.21 |
| Compose BOM | 2024.12.01 |
| compileSdk / targetSdk | 35 |
| minSdk | 26 |
| NDK | 27.0.12077973 |
| CMake | 3.22.1 |
| Oboe | 1.9.3 |
| ABI | `arm64-v8a`, `x86_64` |

## Android build

```bash
export ANDROID_HOME=$HOME/Android/Sdk
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk   # or your JDK 21

./gradlew :app:assembleDebug
./gradlew :app:testDebugUnitTest
```

APK: `app/build/outputs/apk/debug/app-debug.apk`.

Install on a connected device:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.danimanx.replicanttr8s/.MainActivity
```

## Host DSP tests

The engine is platform-independent apart from the Oboe driver. It builds on Linux/macOS with CMake:

```bash
cmake -S host -B build-host -DCMAKE_BUILD_TYPE=Release
cmake --build build-host -j
./build-host/engine_tests
```

`engine_tests` checks:

- silence before transport starts
- audible drums after play
- MIDI note 36 triggers the bass drum
- channel filter rejection
- MIDI clock at 48 pulses per second at 120 BPM
- variation copy/clear
- no non-finite samples

Render a 48-second demo with the engine:

```bash
./build-host/render_demo assets/replicant_tr8s_android_demo.wav
```

## Firmware parser test (bring your own firmware)

The repository does not contain firmware. To verify the parser locally:

```bash
mkdir -p .firmware
unzip -p /path/to/tr8s_sys_v300.zip tr8s_sys_v300/TR8S_UP.bin > .firmware/TR8S_UP.bin
./build-host/firmware_test .firmware/TR8S_UP.bin
```

Expected output includes `128 kits`, factory kit names, a non-empty pattern name and `factory pattern peak` > 0. `.firmware/` is git-ignored.

## Emulator setup (used for screenshots)

```bash
sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0" \
           "ndk;27.0.12077973" "cmake;3.22.1" \
           "emulator" "system-images;android-35;google_apis;x86_64"

avdmanager create avd -n phone35 -k "system-images;android-35;google_apis;x86_64" -d pixel_7
$ANDROID_HOME/emulator/emulator -avd phone35 -no-window -no-snapshot \
    -no-boot-anim -gpu swiftshader_indirect -no-audio -accel on
```

Screenshots were captured with:

```bash
adb shell screencap -p /sdcard/shot.png
adb pull /sdcard/shot.png screenshots/shot.png
```

Phone viewport: `1080x2400 @ 420 dpi`. Tablet portrait: `1600x2560 @ 240 dpi`. Tablet landscape: `2560x1600 @ 240 dpi`.

The emulator reports AAudio **shared** mode and a 960-frame burst; the app's latency badge is therefore around 70–80 ms in the emulator. Real hardware with exclusive AAudio is the target.

## Promo video

The promo uses the real screenshots and the native engine demo:

```bash
./build-host/render_demo assets/replicant_tr8s_android_demo.wav
python3 tools/make_promo.py
```

Outputs:

- `assets/replicant_tr8s_android_promo_1080p.mp4` (1920×1080, 30 fps, H.264/AAC, 48 s)
- `assets/replicant_tr8s_android_poster.png`

## CI

`.github/workflows/android.yml` installs JDK 21, Android SDK/NDK/CMake, then runs:

- `./gradlew :app:assembleDebug :app:testDebugUnitTest`
- host CMake build + `engine_tests`

The firmware test is intentionally skipped in CI because no firmware is shipped.

A green run also publishes the debug APK as the `replicant-tr8s-android-debug` workflow artifact.

## Release executable (tagged builds)

`.github/workflows/release.yml` builds the installable release APK and AAB and
publishes them to a GitHub Release whenever a version tag is pushed:

```bash
git tag 1.0.0
git push origin 1.0.0
```

It can also be started manually with `gh workflow run release.yml -f version=1.0.0`.
Assets: `ReplicantTR8S-Android-<version>.apk`, the matching `.aab`, and
`SHA256SUMS.txt`. The APK is signed with the optional `REPLICANT_KEYSTORE_BASE64`
secret, or with the CI debug key when no secret is configured.

Full details, including local signing and verification commands, are in
[RELEASING.md](RELEASING.md).
The workflow configures the runner's preinstalled SDK directly because
`android-actions/setup-android` now fails while installing the removed `tools` package.
