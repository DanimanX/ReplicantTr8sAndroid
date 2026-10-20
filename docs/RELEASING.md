# Releasing

The Android executable is built and published by GitHub Actions. No local
machine needs the Android SDK for a release.

## Trigger a release

Push a version tag:

```bash
git tag 1.0.0
git push origin 1.0.0
```

`v1.0.0` works too. The tag can point at any commit on `main`.

Manual dispatch (useful for a re-run or a draft):

```bash
gh workflow run release.yml -f version=1.0.0 -f draft=true
```

The workflow:

1. Resolves the version from the tag (or the dispatch input) and derives
   `versionCode = MAJOR*10000 + MINOR*100 + PATCH` (so `1.0.0` → `10000`).
2. Runs `:app:testDebugUnitTest`.
3. Builds `:app:assembleRelease` and `:app:bundleRelease` with
   `-PreplicantVersionName` / `-PreplicantVersionCode`.
4. Verifies the APK with `apksigner verify` and `aapt2 dump badging`, checking the
   package name and version.
5. Publishes a GitHub Release with the APK, AAB and `SHA256SUMS.txt`.

Assets are named:

```
ReplicantTR8S-Android-<version>.apk
ReplicantTR8S-Android-<version>.aab
SHA256SUMS.txt
```

## Signing

By default the release APK is signed with the CI debug key, which is enough to
install it and keeps the workflow self-contained.

For a stable production signature, add these repository secrets:

| Secret | Value |
| --- | --- |
| `REPLICANT_KEYSTORE_BASE64` | `base64 -w0 release.keystore` |
| `REPLICANT_KEYSTORE_PASSWORD` | keystore password |
| `REPLICANT_KEY_ALIAS` | key alias |
| `REPLICANT_KEY_PASSWORD` | key password |

The workflow decodes the keystore to `$RUNNER_TEMP`, exports
`REPLICANT_KEYSTORE`/`REPLICANT_KEYSTORE_PASSWORD`/`REPLICANT_KEY_ALIAS`/
`REPLICANT_KEY_PASSWORD`, and `app/build.gradle.kts` uses them when all four are
present. Without them it falls back to the debug signing config.

Local release build with the same knobs:

```bash
./gradlew :app:assembleRelease :app:bundleRelease \
  -PreplicantVersionName=1.0.0 -PreplicantVersionCode=10000
```

To sign locally:

```bash
export REPLICANT_KEYSTORE=/absolute/path/release.keystore
export REPLICANT_KEYSTORE_PASSWORD=...
export REPLICANT_KEY_ALIAS=...
export REPLICANT_KEY_PASSWORD=...
./gradlew :app:assembleRelease -PreplicantVersionName=1.0.0 -PreplicantVersionCode=10000
```

## Verify a published release

```bash
gh release download 1.0.0 --repo DanimanX/ReplicantTr8sAndroid --dir dist
cd dist && sha256sum -c SHA256SUMS.txt
$ANDROID_HOME/build-tools/35.0.0/apksigner verify --verbose ReplicantTR8S-Android-1.0.0.apk
$ANDROID_HOME/build-tools/35.0.0/aapt2 dump badging ReplicantTR8S-Android-1.0.0.apk | head -3
```

Expected badging includes:

```
package: name='com.danimanx.replicanttr8s' versionCode='10000' versionName='1.0.0'
native-code: 'arm64-v8a' 'x86_64'
```

Install on a device:

```bash
adb install -r ReplicantTR8S-Android-1.0.0.apk
```

## Versioning

- Tags are semver (`MAJOR.MINOR.PATCH`, optional `-suffix`).
- `versionCode` must increase for Play uploads; the formula above gives
  `1.0.0` → 10000, `1.0.1` → 10001, `1.1.0` → 10100.
- The release notes are generated from commits since the previous tag.

## Notes

- The APK is a single bundle for `arm64-v8a` and `x86_64`; 32-bit devices are not
  supported.
- No firmware is included. The app runs its built-in preview kits and can import
  a user-supplied TR-8S v3.00 image at runtime.
- Store distribution should use the AAB and a real release keystore.
