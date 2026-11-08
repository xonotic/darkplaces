# Xonotic for Android

Native Android port of the DarkPlaces engine running Xonotic, with game controller support.

## Install

1. Download `xonotic-android.apk` from the Releases page (or the latest **Android APK** workflow artifact).
2. Allow "Install unknown apps" for your browser/file manager and open the APK.
3. Launch **Xonotic**. On first run, tap **Download game data** (about 1.2 GB, resumable; Wi-Fi recommended).
   The launcher fetches only the `Xonotic/data/*.pk3` files out of the official
   `xonotic-0.8.6.zip` using HTTP range requests.
   Alternative: copy the `.pk3` files from a desktop install's `data/` folder to
   `Android/data/org.xonotic.android/files/data/`.

Requirements: Android 7.0+ (API 24), OpenGL ES 3.0 GPU. ABIs: arm64-v8a, armeabi-v7a, x86_64.

## Controller

Any controller Android/SDL recognizes as a gamepad (Xbox, PlayStation, 8BitDo, Switch Pro, Android TV pads; USB or Bluetooth).

| Input | Action |
|---|---|
| Left stick | Move |
| Right stick | Look |
| RT / LT | Primary / secondary fire |
| A | Jump |
| B | Crouch |
| X | Reload |
| Y | Last weapon |
| RB / LB | Next / previous weapon |
| L3 | Grappling hook |
| R3 | Zoom |
| D-pad up / Back | Scoreboard |
| D-pad down | Drop weapon |
| D-pad left | Chat |
| D-pad right | Vote yes |
| Start | Menu |

In menus: stick/D-pad navigate, A selects, B goes back.
Defaults live in `assets/android.cfg` (re-copied every launch). Personal overrides go in
`Android/data/org.xonotic.android/files/data/autoexec.cfg`, e.g. `joy_x360_sensitivityyaw -3`.

## On-screen controls

Shown automatically in a match when no physical gamepad is connected.

- Left side: floating thumbstick (move). Touch anywhere on the lower-left to grab it.
- Right side: drag on empty space to aim.
- Buttons: FIRE, ALT, JUMP, CRCH, RLD, HOOK, ZOOM, WPN+/WPN-, LAST; top-left MENU, TAB (scores), CHAT.
- Top-right PAD button cycles AUTO (hide when gamepad connected) / ON / OFF; choice is saved.
- In menus and console the overlay steps aside and taps go to the game as mouse clicks.

Buttons drive an SDL virtual game controller (`app/jni/android_touchpad.c`), so they use the same
`X360_*` binds as a real pad; rebinding the pad rebinds the touch buttons too. Aim speed follows `sensitivity`.

## Build

```sh
cd android
./gradlew assembleRelease   # needs JDK 17, Android SDK 34, NDK 26.3.11579264, CMake 3.22.1
```

CMake fetches SDL 2.30.9, libjpeg-turbo, libogg, libvorbis and FreeType at configure time.
Output: `app/build/outputs/apk/release/app-release.apk`.

Signing: release builds use `app/xonotic-sideload.keystore` (password `android`) so sideloaded
updates install over each other. That key is public — for Play Store/other distribution set
`XON_KEYSTORE`, `XON_KEYSTORE_PASS`, `XON_KEY_ALIAS`, `XON_KEY_PASS`.

## Engine changes for Android

- `glquake.h`: GLES builds now get desktop/ARB/ES3 enum fallbacks (`#ifndef`-guarded) and `GLAPIENTRY`.
- `android/app/jni/gles_compat.c`: `glMapBuffer` → `glMapBufferRange`; `*ARB` debug-output → ES 3.2 core.
- `vid_sdl.c`: fix const-assignment in mobile fullscreen path.
- `gl_textures.c`: stale KTX loader gated behind `DP_KTX` (header not in tree).

## Known limits

- No player ID/crypto (`d0_blind_id` not bundled) and no curl autodownload of server maps yet.
- Default graphics tuned low (`r_viewscale 0.75`, no realtime shadows/bloom); raise in Options on fast phones.
- Engine is DarkPlaces master; data is Xonotic 0.8.6 release.
