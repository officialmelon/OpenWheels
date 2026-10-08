# iOS build

OpenWheels runs on iPhone and iPad (iOS 11+, arm64) with the same game code and touch controls as
Android. The iOS layer is `src/platform/ios/` (`AppController`, `RootViewController`, `main.m`,
`Info.plist`, launch screen), modelled on cocos2d-x 3.17.2's iOS template.

## Building

On a Mac with Xcode and CMake:

```sh
tools/fetch_engine.sh
OW_IOS_TEAM=<your Apple team id> tools/build.sh --ios      # device build, signed with your team
tools/build.sh --ios-simulator                              # simulator build (x86_64 prebuilts)
open build-ios/OpenWheels.xcodeproj                         # or sign, run and archive from Xcode
```

Install on your own device from Xcode (a free Apple ID works for personal builds), or sideload the
built `.app` / an exported `.ipa` with your tool of choice. The bundle id is
`org.openwheels.game`; change `PRODUCT_BUNDLE_IDENTIFIER` in `CMakeLists.txt` if it clashes.

## Game files

As everywhere, no game files are shipped. The app finds the player's own Android `assets/` folder
in either place:

* **Bundled at build time:** when `binary/HappyWheels_Android/HW_Android/assets/` exists (or
  `-DOW_ANDROID_ASSETS=<dir>`), CMake adds it to the app as `assets/`. The generated tables and art
  (sound table, UI text, restored characters, browser items) are written into the bundle by the
  same post-build generators as on PC.
* **Copied onto the device:** the app enables iTunes / Finder file sharing. Connect the device,
  open it in Finder (Files tab) and drop the `assets` folder (and optionally an `ios` folder with
  the original iOS app's files, for the level editor's art) onto OpenWheels. The app shows a
  message at start-up while the files are missing.

## Platform behaviour

* Landscape only, full screen, home indicator auto-hidden, system edge gestures deferred (thumbs
  rest on the screen edges while driving), multi-touch on.
* Rotation, Split View and Stage Manager resizes re-apply the original's fixed-height design
  resolution, like `nativeSurfaceResized` on Android.
* "Open in OpenWheels" for `.happywheels` files (AirDrop, Files, Mail) imports them into Your
  Levels, like `--open` on PC.
* Send to Nearby and ghost racing ask for local network access the first time.
