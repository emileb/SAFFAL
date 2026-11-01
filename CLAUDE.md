# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**SAFFAL** (Storage Access Framework File Access Library) is an Android library that provides a unified file access interface for both regular filesystem paths and SAF-protected directories. It enables apps to read/write files using both traditional file APIs and modern SAF APIs transparently.

## Modules

- **`saffal/`** — The core library (Android Library module, published artifact)
- **`app/`** — Example application demonstrating library usage

## Build Commands

```bash
# Build debug AAR (library)
./gradlew :saffal:assembleDebug

# Build full project
./gradlew assembleDebug

# Run unit tests
./gradlew test

# Run instrumentation tests (requires connected device/emulator)
./gradlew connectedAndroidTest

# Lint
./gradlew lint

# Clean
./gradlew clean
```

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
- Build uses legacy **ndk-build** (not CMake). Config in `Android.mk` / `Application.mk`. NDK version: 24. C++ STL: `c++_static`. Platform: Android 16 min.

### Key Design Decisions

- FileSAF checks if a path falls under the configured SAF root; if yes, all operations go through DocumentNode/SAF APIs; otherwise standard File I/O is used.
- Native code hooks libc via `dlsym` so existing C/C++ code calling standard POSIX file APIs transparently goes through SAF for protected paths.
- SAF URI permissions are persisted via SharedPreferences to survive app restarts.

## NDK Build

The active NDK makefile is `Android.mk`; `Android_dummy.mk` is a stub used when native compilation should be skipped. The library compiles to `libsaffal.so`.
