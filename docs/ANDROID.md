# Android build

`tools/build_android.ps1` builds an APK from the `android/` Gradle project (Gradle 9.1, AGP 8.13,
NDK r27, target SDK 35, min SDK 21) on the unmodified cocos2d-x 3.17.2 Android prebuilts.
Application id `org.openwheels.game`; ABIs arm64-v8a, armeabi-v7a and x86 by default.

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_android.ps1                 # debug APK, all ABIs
powershell -ExecutionPolicy Bypass -File tools\build_android.ps1 -Abis x86 -Run  # quick emulator build
```

The script finds the Android SDK (`ANDROID_HOME`, `ANDROID_SDK_ROOT` or
`%LOCALAPPDATA%\Android\Sdk`), installs missing SDK packages (platform 35, build-tools 35.0.0,
CMake 3.22.1, NDK 27.3.13750724) with `sdkmanager`, checks for JDK 17+, fetches the engine with
`tools/fetch_engine.ps1` if needed, runs Gradle, and copies the result to
`build/android/OpenWheels-<config>.apk`. Options: `-Config Debug|Release`, `-Abis`, `-Assets`,
`-GameLib`, `-IosApp`, `-BuildRoot` (or `OW_BUILD_ROOT`, to put the multi-GB build tree on
another drive), `-Install`, `-Run`, `-Clean`. Requires Python 3 with `unicorn`, as for Windows.

## Game files

No game files are in the repo. Gradle stages them from the player's own copies (defaults in
`android/gradle.properties` and `android/app/build.gradle`, overridable with `-P<name>=<path>`)
into the APK's `assets/`:

| Property | Default | Use |
|---|---|---|
| `OW_GAME_ASSETS` | `binary/HappyWheels_Android/HW_Android/assets` | the Android game's asset tree (required) |
| `OW_GAME_LIB` | `binary/HappyWheels_Android/config.arm64_v8a/lib/arm64-v8a/libMyGame.so` | source of `soundlist.tsv` / `gametext.tsv` (required) |
| `OW_GAME_RES` | `binary/HappyWheels_Android/HW_Android/res` | launcher icon (optional) |
| `OW_IOS_APP` | `binary/HappyWheels_iOS/Payload/happywheels.app` | level-editor art and text (optional) |
| `OW_FLASH_SWF`, `OW_FLASH_CHARACTERS`, `OW_FLASH_SOUNDS`, `OW_FFDEC_DIR` | under `binary/flash/` | browser-game art for restored characters, browser-level items and sounds, kid gore (optional) |

The optional generators run as Gradle tasks (`owGenerateKidGore`,
`owGenerateRestoredCharacters`, `owGenerateFlashItems`, `owGenerateFlashSounds`) and are skipped
when their inputs are missing.

## Release signing

Release builds are signed with the keystore described by a properties file outside the repo
(`storeFile`, `storePassword`, `keyAlias`, `keyPassword`), given by the `OW_KEYSTORE_PROPERTIES`
environment variable or `-PowKeystoreProperties=<file>`. Without it, release builds fall back
to the debug key.

## Platform layer

`src/platform/android/`: the native entry point (asset search paths, iOS bundle files in
`assets/ios/`, frame-size updates on resize or fold) and `AppActivity` (immersive landscape,
display cutouts kept clear, letterboxing below 3:2 so no menu button is cut off,
`configChanges` so rotation and folding keep the GL context). There are no ads, store,
analytics or network requirements; the original's first-run terms alert (which covered its
ad and analytics SDKs) is pre-accepted. Android-specific changes elsewhere are marked
`// ANDROID (port):`.

Tested on emulators: Pixel 7 at 20:9 and 21:9, Pixel Tablet at 16:10 and 4:3.
