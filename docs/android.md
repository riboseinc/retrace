# Android

retrace supports Android via cross-compilation. The resulting
`libretrace.so` runs on any Android device that supports
`LD_PRELOAD` (debug builds via `wrap.sh`, or rooted devices via
Magisk).

## Cross-compile

Requires the [Android NDK](https://developer.android.com/ndk).

```sh
$ cmake -B build-android -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake \
    -DANDROID_NDK=$ANDROID_NDK_HOME \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-29 \
    -DRETRACE_BUILD_TESTS=OFF

$ cmake --build build-android
```

Output: `build-android/src/v2/libretrace.so`

## Deploy to device

```sh
$ adb push build-android/src/v2/libretrace.so /data/local/tmp/
$ adb shell
# On a debug build with wrap.sh:
$ LD_PRELOAD=/data/local/tmp/libretrace.so /data/local/tmp/your-binary
```

For debug builds, add a `wrap.sh` to your app:

```sh
#!/system/bin/sh
LD_PRELOAD=/data/local/tmp/libretrace.so exec "$@"
```

## Bionic libc notes

Android uses Bionic libc (not glibc or musl). Key differences:

- **No `__isoc99_scanf` variants.** retrace's `RETRACE_HAVE_ISOC99_SCANF`
  is auto-set to 0; the musl shim handles scanf format parsing.
- **No `libpthread.so`.** pthreads are in libc. retrace handles this
  (the `dlopen("libpthread.so.0")` call returns NULL and is ignored).
- **Weak definitions override.** On bionic, a weak definition in a
  preloaded `.so` beats libc's strong one — exported trampolines would
  interpose the dynamic linker's and libc's own calls mid-load, before
  the engine finished init. retrace therefore emits its trampolines
  weak AND hidden on Android (`RETRACE_ANDROID_WEAK`): nothing
  interposes at load time.
- **No unresolved-symbol tolerance.** Bionic refuses to load a `.so`
  with unresolvable symbols (glibc resolves them lazily and ignores
  failures). The Android link uses a version script that keeps the
  export surface exact.
- **No loader calls before init.** retrace's real-implementation
  resolver on bionic parses `/proc/self/maps` and the mapped ELFs'
  dynamic sections directly (GNU-hash lookup, no `dlsym`) — resolving
  a real impl must never reenter the linker lock.

## Tracing semantics on Android (v2.104.0+)

The library loads, initializes, reads its JSON config, and emits
its trace on bionic (verified end to end under the emulator
runtime). Because the trampolines are hidden, `LD_PRELOAD` does
not redirect the target's own libc calls yet: the working lane
today is self-interposition — the engine's internal calls route
through the trampolines and are captured with full arguments.
Redirecting the target's calls (post-init PLT/GOT rewriting) is
tracked as future work; the general helper APIs
(`retrace_attach_process`, config validation) are available as
on every platform.

## Use cases

- **Fuzz Android native libraries** — intercept malloc in JNI code
  to find OOM crashes
- **Trace network calls** — log every `connect`/`send`/`recv` to
  see what data your app sends
- **Audit file access** — verify which files a proprietary APK reads
- **Sandbox untrusted code** — block access to sensitive paths at
  runtime (see [cookbook/20-sandbox.md](cookbook/20-sandbox.md))

## Limitations

- Requires debug build or root. Non-debug, non-root apps cannot
  use `LD_PRELOAD`.
- Java/Kotlin code is NOT intercepted (retrace only sees JNI/native
  libc calls).
- Target-call interposition is not yet active (see "Tracing
  semantics" above) — v1 captures the engine's own dispatches.
- Some Bionic-specific symbols may not be in the prototype registry.
  File an issue if you find a gap.
