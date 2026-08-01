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
- **`dlsym(RTLD_NEXT, ...)` works** on Bionic for standard symbols.

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
- Some Bionic-specific symbols may not be in the prototype registry.
  File an issue if you find a gap.
