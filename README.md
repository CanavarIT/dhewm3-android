# dhewm3-android (GL4ES)

Experimental **native Android** port of [dhewm3](https://github.com/dhewm/dhewm3) (Doom 3 GPL source port).

> **Rendering backend: [GL4ES](https://github.com/ptitSeb/gl4es)**  
> Desktop OpenGL (fixed-function + ARB programs) is translated to **OpenGL ES 2.0**.  
> This is **not** a native GLES renderer yet — GL4ES is intentional for the first working port.

Repository: https://github.com/CanavarIT/dhewm3-android

---

## What this project is

| Component | Role |
|-----------|------|
| **dhewm3** | Doom 3 engine (id Tech 4) |
| **SDL2** | Window, input, Android activity glue |
| **GL4ES** | OpenGL → GLES2 translation |
| **OpenAL Soft** | Audio (OpenSL ES backend on Android) |
| **Java overlay** | Touch controls (stick, look, fire, skip, …) via JNI |

### What we built (short history)

1. NDK + CMake integration of dhewm3 as `libdoom3.so`
2. Asset extraction from APK `assets/base/` into app internal storage
3. GL4ES context path in `glimp.cpp` (GLES2 context for Android)
4. Java `TouchControlsView` + JNI → engine input queues
5. Mobile performance preset (resolution cap, no stencil shadows, texture downsize)

---

## Requirements

- **Android Studio** or **AndroidIDE** (Termux-based)
- **NDK** r26+ (tested with NDK 29)
- **CMake** 3.22+ (3.31 OK)
- Device/emulator: **arm64-v8a**, API 29+
- A legal copy of **Doom 3** game data (`base/*.pk4`)

### Not in this repo

- `*.pk4` / commercial game files  
- Prebuilt `.so` / APK  
- `dhewm3`, `SDL2`, `gl4es`, `openal-soft` sources (cloned by script)

---

## Quick start

### 1. Clone

```bash
git clone https://github.com/CanavarIT/dhewm3-android.git
cd dhewm3-android
```

### 2. Fetch dependencies

```bash
chmod +x scripts/*.sh
./scripts/fetch_deps.sh
./scripts/apply_patches.sh
```

This clones into `app/src/main/cpp/`:

- `dhewm3/`
- `SDL2/` (release-2.30.9)
- `gl4es/`
- `openal-soft/`

Then overlays Android-specific files from `patches/`.

### 3. Add game data

Put Doom 3 `base` paks into:

```text
app/src/main/assets/base/
  pak000.pk4
  pak001.pk4
  ...
```

On first launch the app extracts them to internal storage and sets `fs_basepath` / `fs_savepath`.

### 4. Build

1. Open the project in Android Studio / AndroidIDE  
2. Set **NDK version** in `app/build.gradle` to the one you have installed  
3. Build **Debug** or **Release** (`arm64-v8a`)  
4. Install the APK  

Release builds are strongly recommended for playable FPS.

---

## Controls (touch)

| Control | Action |
|---------|--------|
| Left stick | Move (WASD) |
| Finger on free area | Look |
| FIRE | Attack (mouse1) |
| JUMP / CROUCH / E | Jump / crouch / use |
| − / + | Prev / next weapon |
| ❚❚ | Pause (Esc) |
| SKIP | Skip intro/cinematic (secondary, top-left) |

UI code: `app/src/main/java/com/classic/doom3/TouchControlsView.java`  
(Java-only changes → fast rebuild, no full NDK).

---

## Performance (GL4ES mobile preset)

Startup cvars in `android_main.cpp` (approximate):

- Render resolution: long side capped at **960**
- `r_shadows 0` (stencil shadows are expensive on mobile GPUs)
- Texture downsize limits
- No MSAA

Tune in `sys/android/android_main.cpp`:

```cpp
const int kMaxLongSide = 960;  // 720 = weak devices, 1280 = stronger
```

Expect large FPS variance by scene complexity under GL4ES (translation cost).

---

## Project layout

```text
app/src/main/
  java/com/classic/doom3/   # MainActivity, touch overlay, JNI bridge
  java/org/libsdl/app/      # SDL activity (from SDL2 android project)
  cpp/CMakeLists.txt        # Android NDK root (vendors SDL2/GL4ES/OpenAL/dhewm3)
  cpp/dhewm3/               # (fetched)
  cpp/SDL2/                 # (fetched)
  cpp/gl4es/                # (fetched)
  cpp/openal-soft/          # (fetched)
patches/                    # Android engine overlays applied by script
scripts/fetch_deps.sh
scripts/apply_patches.sh
```

---

## Known limitations

- **GL4ES only** — not native GLES; future work may integrate a GLES2 backend (e.g. boom3/d3es-style)
- arm64-v8a only in default `abiFilters`
- RoE (`d3xp`) off by default in CMake
- No game data shipped

---

## License

See [LICENSE](LICENSE).  
Engine and libraries keep their upstream licenses (GPL / zlib / MIT / LGPL).  
**You must own Doom 3 to use the game data.**

---

## Credits

- [dhewm3](https://github.com/dhewm/dhewm3) team  
- [ptitSeb/gl4es](https://github.com/ptitSeb/gl4es)  
- [libsdl-org/SDL](https://github.com/libsdl-org/SDL)  
- [kcat/openal-soft](https://github.com/kcat/openal-soft)  
- id Software — original Doom 3  
