# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**SAFFAL** (Storage Access Framework File Access Library) is an Android library that provides a unified file access interface for both regular filesystem paths and SAF-protected directories. It enables apps to read/write files using both traditional file APIs and modern SAF APIs transparently.

## Modules

- **`saffal/`** — The core library (Android Library module, published artifact)
- **`saftest/`** — Regression test app (see below)
- **`app/`** — Old support-library example; not in `settings.gradle` any more (doesn't build with AGP 8)

## Build Commands

The OpenTouch apps include only `saffal/`; the root Gradle files here are for the standalone build (AGP 8.13, Gradle 8.14, JDK ≤ 21).

```bash
# Build debug AAR (library)
./gradlew :saffal:assembleDebug

# Build the regression test app (also builds libsaffal.so from saffal/src/main/jni)
./gradlew :saftest:assembleDebug
```

## Regression test (`saftest/`)

`saftest/src/main/jni/saftest.cpp` runs the same file operations three times: through real libc on a normal folder (the expected results), through SAFFAL's exports on a SAF folder, and through SAFFAL's exports on a normal folder (pass-through). Every result that differs from libc's is reported: return value, `errno` (preset to 0), data read, file type and size, folder listings, `getcwd`. It covers missing paths, `fopen` modes, two readers of one file, raw `open` flags and `__open_2`, `mkdir`/`remove`/`rename`, `opendir`/`scandir`, relative paths after `chdir` (including misses, relative `chdir`, `getcwd` buffers and another thread), a launch-like `chdir` into a game folder with `./` paths, a Q2PRO-style search loop that stops on anything but ENOENT, more files than the fd cache holds, and threaded reads. Built against SAFFAL from before `944fe38`, the search checks report "stopped by errno=0": the Q2REPRO colormap.pcx failure. The suite repeats under the SAFFAL settings of the launcher (no native caches), the game process (native caches) and the game with prefetch.

Run it after any change to the native or Java SAF code:

1. Install `saftest/build/outputs/apk/debug/saftest-debug.apk`, create a folder (`adb shell mkdir /sdcard/SAFTest`) and pick it once with "Pick SAF folder" (on Android 10 the picker hides internal storage until "Show internal storage" in its menu).
2. `adb shell am start -n com.opentouchgaming.saftest/.MainActivity --ez autorun true`, wait for `SAFTEST FINISHED` in `adb logcat -s SAFTEST`.
3. The report is in `/sdcard/Android/data/com.opentouchgaming.saftest/files/saftest_report.txt`: one `SAFTEST mode=...` summary per mode and a `FAIL` line per difference. Pass-through should always show 0. Compare SAF failures against the known differences below.

Known differences, left unfixed on purpose. A run with only these shows 15 / 13 / 15 failures in launcher / game / game+prefetch:

- `fopen` `"a+"` and `"r+"` fail with EINVAL, and `open(O_RDWR)` truncates.
  - Java opens `"wa"` (write-only; ContentResolver has no `"rwa"`) or `"r"`, so `fdopen` rejects the descriptor, which then leaks.
  - `O_RDWR` maps to `"w"`.
  - Opening `"rw"` would fix all three, but needs testing across providers.
- `mkdir` with a missing parent creates the parents (Java `mkdirs()`). A game might rely on this on SAF only.
- The fd cache serves old contents after `remove` or `rename` outside `user_files`.
- `scandir` ignores the sort callback (the test sorts results itself).

## Architecture

### Java Layer (`saffal/src/main/java/com/opentouchgaming/saffal/`)

- **`FileSAF`** — Extends `java.io.File`. Dual-mode: delegates to standard `File` I/O for real paths, or uses SAF DocumentFile API for SAF-protected paths. Main surface area of the library.
- **`UtilsSAF`** — Singleton. Manages the SAF tree root (URI + base path), handles document tree selection intent, persists URI permissions in SharedPreferences, and converts between filesystem paths and SAF document paths.
- **`DocumentNode`** — Represents a node in the SAF document tree hierarchy. Caches document metadata (size, modified date). Handles child creation/deletion and stream I/O.
- **`FileJNI`** — JNI bridge. Exposes Java-side file operations (`fopen`, `fclose`, `mkdir`, `exists`, `delete`, `rename`, `opendir`) to be called from native code when intercepting file system calls.

### Native Layer (`saffal/src/main/jni/`)

- **`FileSAF.cpp`** — Intercepts libc file functions using `dlsym()`. Redirects calls for SAF-managed paths up to Java via JNI.
- **`FileJNI.cpp`** — JNI initialization and method dispatch.
- **`FileCache.cpp`** — Caches file descriptors (max 256). Thread-safe via pthreads.
- **`Utils.cpp`** — C++ utility functions.
- Build uses legacy **ndk-build** (not CMake). Config in `Android.mk` / `Application.mk`. NDK 27, C++ STL `c++_static`, minSdk 21.

### Key Design Decisions

- FileSAF checks if a path falls under the configured SAF root; if yes, all operations go through DocumentNode/SAF APIs; otherwise standard File I/O is used.
- Native code hooks libc via `dlsym` so existing C/C++ code calling standard POSIX file APIs transparently goes through SAF for protected paths.
- SAF URI permissions are persisted via SharedPreferences to survive app restarts.

## NDK Build

The active NDK makefile is `Android.mk`; `Android_dummy.mk` is a stub used when native compilation should be skipped. The library compiles to `libsaffal.so`.
