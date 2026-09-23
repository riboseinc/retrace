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
runtime). The general helper APIs (`retrace_attach_process`,
config validation) are available as on every platform.

### Capturing the target's own calls (v2.109.0+)

Because the trampolines are hidden symbols (the bionic
weak-override law), `LD_PRELOAD` alone does not redirect the
executable's own libc calls. Set the opt-in gate before the run:

```sh
RETRACE_ANDROID_REBIND=1 RETRACE_JSON_CONFIG=conf.json \
  LD_PRELOAD=libretrace.so ./target
```

After the engine finishes booting, retrace walks
`/proc/self/maps` (bionic's `dl_iterate_phdr` is not reliable
under emulation), finds the executable's dynamic section, and
rewrites the executable's PLT/GOT slots for every trampolined
symbol to point at the exported `__retrace_wrap_<func>`
aliases. From that moment the target's own libc calls are
captured with full arguments, and denial actions (sandbox,
`addr_deny`, `modify_*`) apply to them like any other call.

Details worth knowing:

- **Only the executable's calls are captured.** The engine
  routes by caller: calls made from retrace itself or from
  system libraries are exempt, so the trace stays the target's
  story.
- **Loader-support functions stay out of scope** (the
  `pthread_once` RELRO control, the `dlopen` family, the
  `exit` family) — rebinding them breaks the loader, not the
  target.
- **Off is byte-identical.** Without `RETRACE_ANDROID_REBIND=1`
  no slot is touched and behavior matches v2.104 semantics —
  the gate exists because with the rebind active the loader's
  own boot-time calls also reach the engine, which is noisy and
  can exit the process before `main` on some targets.
- Resolving real implementations never goes through `dlsym`
  (the linker-lock reentrancy trap): the resolver reads
  `/proc/self/maps` and the mapped ELFs' dynamic sections
  directly.

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
- Target-call capture is opt-in (`RETRACE_ANDROID_REBIND=1`, see
  above); without the gate only the engine's own dispatches are
  captured.
- Some Bionic-specific symbols may not be in the prototype registry.
  File an issue if you find a gap.
