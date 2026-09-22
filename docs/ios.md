# iOS

retrace does not preload on stock iOS: AMFI and code signing
make `DYLD_INSERT_LIBRARIES` dead for any signed app. The
honest scope is observer-only, with three supported surfaces
of decreasing convenience.

## The iOS simulator (supported today)

Simulator binaries are ordinary macOS Mach-O — the regular
macOS preload backend works unchanged:

```sh
# build the macos dylib, then run the simulator slice
DYLD_INSERT_LIBRARIES=/path/to/libretrace.dylib \
RETRACE_JSON_CONFIG=conf.json \
xcrun simctl spawn <device> ./app
```

Use this for iOS-development debugging: the JSON trace, the
fuzzing actions, and the jail all behave as on macOS. SIP
rules are identical to the macOS case (system binaries need
`csrutil disable`).

## Jailbroken devices (frida-bridge)

On a jailbroken device, injection exists through the same
mechanism frida uses. retrace's frida-bridge drives the
engine from userspace without a preload:

- the bridge script is loaded into the target process by
  frida-server,
- calls are routed to the engine through the bridge, and the
  evidence rides the supervisor protocol to a retraced daemon
  (on your workstation, over USB/Wi-Fi).

Setup and limitations are documented in the bridge's README;
treat the throughput as debugging-grade, not farm-grade.

## Stock devices (network observer)

For stock devices the observation point moves off-device: run
the tool's protocol decoders as a local proxy
(`decode_http`/`decode_dns` shapes) and point the device's
proxy at it. You get the network-truth layer of a trace (no
libc calls, no jail semantics) — still useful for exfiltration
and C2 surveys.

## What is not possible

- A preload library on stock devices (no injection path).
- Kernel-truth converters: the iOS kernel tracing surface
  (sysdiagnose) is neither scriptable nor realtime; there is
  no converter for it.
- App Store distribution of anything on this page — every
  lane is a development/research tool.
