# 07 — Packaging & distribution

**Status**: [~] in progress (vcpkg.json manifest committed)
**Layer**: 7 (distribution)
**Depends on**: 01, 05
**Blocks**: nothing (terminal layer)

## Goal

Make retrace trivially consumable via every major channel:

- `find_package(retrace CONFIG)` in any CMake project
- vcpkg: `vcpkg install retrace` (port in tree, manifest mode)
- Conan: `conan install retrace/2.0.0@`
- System packages: Debian, RPM, Homebrew, Arch, FreeBSD ports
- Source tarball on GitHub Releases

## Why

Without proper packaging, downstream consumers (the library use case) must
shell out to Autotools and hand-roll include paths. That's friction the
modernization is meant to eliminate.

## Architecture

### CMake package config

`cmake/retrace-config.cmake.in` exports targets `retrace::core`,
`retrace::backend-preload-elf`, `retrace::backend-preload-macho`, `retrace::cli`,
`retrace::config-json`, `retrace::config-text`. Downstream consumers do:

```cmake
find_package(retrace 2.0 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE retrace::core)
```

### vcpkg

- Manifest-mode `vcpkg.json` at repo root (committed, see below)
- Overlay port in `packaging/vcpkg/port/retrace/` for the upstream vcpkg registry PR
- Triplets: `x64-linux`, `arm64-linux`, `x64-osx`, `arm64-osx`, `x64-windows`,
  `x64-windows-static`, `arm64-windows`, `x64-mingw-static`

### Conan

- `packaging/conan/conanfile.py` recipe
- Publish to conan-center (separate PR upstream)

### System packages

- `packaging/debian/` — `*.deb` control files
- `packaging/rpm/` — update existing `rpm/retrace.spec` to use CMake build
- `packaging/homebrew/retrace.rb` — formula
- `packaging/arch/PKGBUILD`
- FreeBSD port: submit to `ports/devel/retrace`

### Source tarball

- `scripts/build-release-tarball.sh` produces `retrace-<version>.tar.gz`
  containing only source (no vcpkg contents, no prebuilt binaries)
- `release.yml` workflow attaches tarball + `.sha256` to GitHub Release

## vcpkg.json manifest (committed)

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "retrace",
  "version": "2.0.1",
  "description": "Versatile security vulnerability / bug discovery tool for ELF and Mach-O binaries.",
  "homepage": "https://github.com/riboseinc/retrace",
  "license": "BSD-2-Clause",
  "dependencies": [
    "openssl",
    { "name": "cmocka", "host": true }
  ],
  "features": {
    "tests": {
      "description": "Build unit tests",
      "dependencies": [ "cmocka" ]
    },
    "cli": {
      "description": "Build the retrace CLI"
    }
  }
}
```

## Tasks

### [P0] CMake package export
- [x] `vcpkg.json` manifest at root
- [ ] `cmake/retrace-config.cmake.in` template
- [ ] `cmake/retrace-config-version.cmake.in`
- [ ] `install(EXPORT retraceTargets FILE retraceTargets.cmake NAMESPACE retrace::)`
- [ ] `pkg-config` file `cmake/retrace.pc.in`

### [P0] vcpkg overlay port
- [ ] `packaging/vcpkg/port/retrace/vcpkg.json`
- [ ] `packaging/vcpkg/port/retrace/portfile.cmake`
- [ ] Test overlay locally: `vcpkg install --overlay-ports=packaging/vcpkg/port retrace`
- [ ] Submit upstream PR to microsoft/vcpkg (after release)

### [P1] Conan recipe
- [ ] `packaging/conan/conanfile.py`
- [ ] Test locally: `conan create packaging/conan retrace/2.0.0@`
- [ ] Submit to conan-center (separate PR)

### [P1] RPM spec modernization
- [ ] Update `rpm/retrace.spec` to use CMake (`%cmake` / `%cmake_build`)
- [ ] Drop `BuildRequires: ncurses-devel` if unused
- [ ] Fix `BuildArch: noarch` (incorrect — retrace is arch-specific)

### [P1] Debian packaging
- [ ] `packaging/debian/control`
- [ ] `packaging/debian/rules` (calls CMake)
- [ ] `packaging/debian/libretrace-dev.install`
- [ ] `packaging/debian/retrace.install`

### [P1] Homebrew formula
- [ ] `packaging/homebrew/retrace.rb`
- [ ] Submit to homebrew-core (separate PR)

### [P2] Arch PKGBUILD
- [ ] `packaging/arch/PKGBUILD`

### [P2] Source tarball
- [ ] `scripts/build-release-tarball.sh`
- [ ] Make sure Autotools artifacts (`configure`, `Makefile.in`, etc.) are
  excluded from the tarball — the consumer re-runs CMake

### [P2] FreeBSD port
- [ ] Submit to `ports/devel/retrace` via bugzilla PR

## Library target names (canonical)

Downstream consumers see these CMake targets / pkg-config modules:

| CMake target | pkg-config | What it links |
|--------------|------------|---------------|
| `retrace::core` | `retrace-core` | engine, prototype, action, script |
| `retrace::backend-preload-elf` | `retrace-preload-elf` | Linux LD_PRELOAD backend |
| `retrace::backend-preload-macho` | `retrace-preload-macho` | macOS DYLD backend |
| `retrace::backend-preload-bsd` | `retrace-preload-bsd` | FreeBSD/OpenBSD/NetBSD backend |
| `retrace::config-json` | `retrace-config-json` | JSON config source |
| `retrace::config-text` | `retrace-config-text` | Text config source |
| `retrace::cli` | (binary) | The `retrace` executable |

## Acceptance criteria

- External project: `cmake -B build && cmake --build build` succeeds with
  `find_package(retrace 2.0 CONFIG REQUIRED)`.
- `vcpkg install retrace` works using only the overlay port (before upstream PR).
- `pkg-config --cflags --libs retrace-core` returns valid flags.
- `tar tzf retrace-2.0.0.tar.gz | grep -E '\.(in|lo|o)$'` returns nothing
  (no build artifacts in tarball).

## Open questions

- Static vs shared default? Lean shared (matches existing `libretrace.so`),
  with a `BUILD_SHARED_LIBS=OFF` opt-in for static.
- Should the vcpkg port ship with v1 or v2 enabled by default? v2 — it's
  the future per TODO 10.
