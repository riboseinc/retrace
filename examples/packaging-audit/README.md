# packaging-audit: declared surface vs observed behavior

Claims-vs-truth at the PACKAGING layer (TODO.trace-profile/19):
a snap or flatpak DECLARES what it may touch; retrace observes
what it actually touches; the difference is a confinement
violation.

| Step | Tool | Result |
|---|---|---|
| 1. declared surface | snap2inside / flatpak2inside | inside.json |
| 2. observed behavior | profile capture | profile.json |
| 3. grade | profile --inside | violations list |
| 4. enforce / export | profile jail / harden | jail.json / compose.yaml |

```sh
./run-posix.sh [build-dir]
```

Expected output: the app's read of `/etc/hosts` is reported as a
violation (the demo snap grants only `home` + `network`), and the
harden step emits the container policy derived from the profile.

Real-world notes (honest):
- snap `personal-files`/`system-files` plugs are NOT mapped --
  they are declared in snapd slot config, not snapcraft.yaml;
  they appear in `notes_unmapped_interfaces` instead of silently
  vanishing.
- flatpak v1 accepts JSON manifests only (the YAML form is
  refused with a message); unmapped finish-args are noted.
- AppImages: dynamically-linked bundles -- `--appimage-extract`
  then profile the extracted `AppRun` like any binary (the
  quickstart flow); no converter needed.
- Docker: run the image's entrypoint under retrace inside the
  container (Alpine/musl supported), then harden.
