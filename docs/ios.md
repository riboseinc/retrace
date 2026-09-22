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

### Installing on-device

Build the .deb on any host and install with your package
manager (Cydia/Sileo/Zebra):

```sh
frida-bridge/packaging/build-deb.sh .   # dpkg-deb, or ar+tar
# copy retrace-frida_<ver>_iphoneos-arm.deb to the device, then:
# dpkg -i retrace-frida_<ver>_iphoneos-arm.deb
```

The payload installs `/usr/share/retrace/retrace-frida.js`.
TrollStore (no package manager): extract the .deb (it is an
`ar` archive: `ar x` then `tar xzf data.tar.gz`) or just copy
`retrace-frida.js` anywhere readable, e.g.
`/var/jb/usr/share/retrace/`, and point `frida -l` at it.

Then capture:

```sh
frida -l /usr/share/retrace/retrace-frida.js -n YourApp > trace.json
echo ']' >> trace.json   # close the JSON array
```

Treat the throughput as debugging-grade, not farm-grade.

## Stock devices (network observer)

For stock devices the observation point moves off-device:
`retrace-netobserve` is a local forward proxy that records
observed HTTP traffic in the retrace trace shape (the same
record text the `decode_http` action emits), so the evidence
grades with every retrace tool:

```sh
retrace-netobserve 0.0.0.0 8080 > nettrace.jsonl
# on the device: Settings > Wi-Fi > Proxy > Manual:
#   host = <your host>, port = 8080
```

Plain HTTP is inspected (request line + upstream host); HTTPS
is a CONNECT tunnel — opaque bytes, but the TARGET is
recorded (network truth, not content). You get the
network-truth layer of a trace (no libc calls, no jail
semantics) — still useful for exfiltration and C2 surveys.

## What is not possible

- A preload library on stock devices (no injection path).
- Kernel-truth converters: the iOS kernel tracing surface
  (sysdiagnose) is neither scriptable nor realtime; there is
  no converter for it.
- App Store distribution of anything on this page — every
  lane is a development/research tool.
