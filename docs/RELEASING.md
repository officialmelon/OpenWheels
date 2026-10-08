# Releasing

`.github/workflows/release.yml` builds every package and publishes the GitHub release:

| Job | Runner | Package | |
|---|---|---|---|
| Windows | windows-2022 (VS 2022, Win32) | `OpenWheels-windows.zip` | required |
| Android | ubuntu-24.04 (JDK 17, NDK r27) | `OpenWheels-release.apk` | required |
| Linux | ubuntu-24.04 | `OpenWheels-linux-x86_64.tar.gz` | required |
| macOS | macos-15-intel (x86_64 prebuilts) | `OpenWheels-macos.zip` (unsigned) | best effort |
| iOS | macos-14 | `OpenWheels-ios-unsigned.ipa` (sideload) | best effort |

The release is published when the three required jobs succeed; the macOS and iOS packages are
attached when theirs do. The notes are the version's section of `CHANGELOG.md` plus a downloads
list, and `SHA256SUMS.txt` is attached.

## Making a release

1. Add a `## vX.Y.Z` section to `CHANGELOG.md` (and bump `versionCode` / `versionName` in
   `android/app/build.gradle`), merge to `main`.
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

## Android signing

Without secrets the APK is signed with a debug key and will not install over a release signed with
your key. To sign with your release key, add these repository secrets (Settings -> Secrets and
variables -> Actions):

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | `base64 -w0 your.keystore` |
| `ANDROID_KEYSTORE_PASSWORD` | the keystore password |
| `ANDROID_KEY_ALIAS` | the key alias |
| `ANDROID_KEY_PASSWORD` | the key password |
