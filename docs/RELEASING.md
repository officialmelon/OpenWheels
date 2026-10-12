# Releasing

`.github/workflows/release.yml` builds every package and publishes the GitHub release:

| Job | Runner | Package | |
|---|---|---|---|
| Windows | windows-2022 (VS 2022, Win32) | `OpenWheels-windows.zip` | required |
| Android | ubuntu-24.04 (JDK 17, NDK r27) | `OpenWheels-release.apk` | required |
| Linux | ubuntu-24.04 | `OpenWheels-linux-x86_64.tar.gz` | required |
| macOS | macos-15-intel (x86_64 prebuilts) | `OpenWheels-macos.zip` (ad-hoc or Developer ID signed) | best effort |
| iOS | macos-14 | `OpenWheels-ios-unsigned.ipa` (sideload), or `OpenWheels-ios.ipa` when signed | best effort |

The release is published when the three required jobs succeed; the macOS and iOS packages are
attached when theirs do. The notes are the version's section of `CHANGELOG.md` plus a downloads
list, and `SHA256SUMS.txt` is attached.

## Making a release

1. Add a `## vX.Y.Z` section to `CHANGELOG.md`, bump `versionCode` / `versionName` in
   `android/app/build.gradle` and `CFBundleShortVersionString` / `CFBundleVersion` in
   `src/platform/mac/Info.plist` and `src/platform/ios/Info.plist`, merge to `main`.
2. Either push a tag (`git tag vX.Y.Z && git push origin vX.Y.Z`): the release is published when
   the build finishes; or run **Actions -> Release -> Run workflow** with the tag: the release is
   created as a draft (untick "draft" to publish it directly). Without a tag the workflow only
   builds, and the packages are kept as workflow artifacts.

## Game files

The repository never contains Happy Wheels assets, so the workflow takes them from a previous
OpenWheels release: the `game-data` job downloads the newest release's `OpenWheels-windows.zip`
(or the release named in the `data_release` input, or a zip at the URL in the `OW_GAME_DATA_URL`
secret) and `tools/release/extract_game_data.py` lays out `assets/`, `generated/`, `ios/` and the
`.tsv` tables. Missing restored-character portraits are drawn by `tools/assets/restored_portraits.py`.
The builds then package that data instead of regenerating it (`package_windows.ps1 -NoBuild -Data`,
`package_linux.sh --data`, Gradle `-POW_PREBUILT_DATA`), so no `libMyGame.so`, SWF or FFDec is needed
in CI.

## Signing

Signing keys live in repository secrets (Settings -> Secrets and variables -> Actions), never in
the repository. Every secret is optional except the Android ones for a published release.

### Android

With the release key the APK is the real app (`org.openwheels.game`) and installs over earlier
releases. Every OpenWheels release has been signed with the same key (`CN=OpenWheels`, certificate
SHA-256 `38:0D:75:80:D4:68:BF:AB:2F:F7:1C:E4:7C:3C:D8:8D:6D:D0:64:E8:63:1E:E8:7D:7B:77:13:9F:85:AE:06:05`,
`OW_ANDROID_CERT_SHA256` in the workflow); the job checks the APK against it. A published release
(tag push, or a run with a tag and "draft" unticked) fails without the key. Without it, and for
runs without a tag, the APK is a debug-signed beta (`org.openwheels.game.beta`, "OpenWheels beta")
that installs alongside the release.

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | `base64 -w0 your.keystore` |
| `ANDROID_KEYSTORE_PASSWORD` | the keystore password |
| `ANDROID_KEY_ALIAS` | the key alias |
| `ANDROID_KEY_PASSWORD` | the key password |

To check a keystore is the release key: `keytool -list -v -keystore your.keystore` and compare the
SHA-256 fingerprint above.

### macOS

Without secrets the app is ad-hoc signed: it runs, but as it isn't notarized macOS asks the first
time (right-click the app, Open). With an Apple Developer ID it is signed with the hardened runtime,
and notarized and stapled when the notary secrets are set too.

| Secret | Value |
|---|---|
| `MACOS_CERT_P12_BASE64` | `base64 -i DeveloperID.p12` (a "Developer ID Application" certificate with its key) |
| `MACOS_CERT_PASSWORD` | the .p12 password |
| `MACOS_NOTARY_APPLE_ID` | the Apple ID used to notarize |
| `MACOS_NOTARY_PASSWORD` | an app-specific password for that Apple ID |
| `MACOS_NOTARY_TEAM_ID` | the developer team ID |

### iOS

Without secrets the job attaches `OpenWheels-ios-unsigned.ipa`; AltStore, Sideloadly and similar
tools sign it with your Apple ID when they install it. With a certificate and a provisioning
profile the app is signed (bundle ID and entitlements from the profile) as `OpenWheels-ios.ipa`.

| Secret | Value |
|---|---|
| `IOS_CERT_P12_BASE64` | `base64 -i cert.p12` (an Apple Distribution or Development certificate with its key) |
| `IOS_CERT_PASSWORD` | the .p12 password |
| `IOS_PROVISION_PROFILE_BASE64` | `base64 -i OpenWheels.mobileprovision` |

### Windows

Without secrets `OpenWheels.exe` is not Authenticode-signed and SmartScreen shows "unknown
publisher". With a code-signing certificate it is signed and timestamped.

| Secret | Value |
|---|---|
| `WINDOWS_CERT_PFX_BASE64` | `base64 -w0 cert.pfx` |
| `WINDOWS_CERT_PASSWORD` | the .pfx password |
