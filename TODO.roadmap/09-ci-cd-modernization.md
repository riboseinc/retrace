# 09 — CI/CD modernization

**Status**: [~] in progress (consolidated workflows written, old ones removed)
**Layer**: cross-cutting
**Depends on**: 01 (CMake), 08 (tests)
**Blocks**: nothing

## Goal

Replace the six 2017-era `.github/workflows/*.yml` files (which targeted
EOL'd runners like `ubuntu-18.04`, `macos-10.15`) and the Cirrus FreeBSD job
with a modern, consolidated matrix covering every supported (OS × arch)
pair, using CMake as the build and CTest as the test runner.

The pattern is informed by `~/src/claricle/libemf2svg/.github/workflows/`,
consolidated where setups share enough to merge (Linux+macOS+Windows MSVC
in one workflow; Alpine in another; MSYS in another).

## Why

- `ubuntu-18.04` is gone from GHA — old workflows failed.
- `macos-10.15`, `macos-13` are gone — old workflows failed.
- `actions/checkout@v2`, `actions/cache@v2` are deprecated; v4 is current.
- No ARM coverage (no Apple Silicon macOS, no Linux aarch64, no Windows ARM64).
- No Alpine / musl coverage — retrace is a security tool, musl is critical.
- Cirrus FreeBSD works but used the old Autotools path.
- No caching of vcpkg deps.
- No concurrency control — pushes queued serially.

## Workflow file map (consolidated)

| File | Scope | Why separate |
|------|-------|--------------|
| `build.yml`              | Linux x64+arm64, macOS Intel+arm64, Windows MSVC x64+arm64, + coverage job | All share shell + flow; only pkg manager differs |
| `alpine.yml`             | Alpine x64 + arm64 | Container-based, apk, qemu for arm64 |
| `msys.yml`               | MinGW64 + UCRT64   | msys2 shell, gcc toolchain, mingw-static triplet |
| `checkpatch.yml`         | Linux checkpatch.pl | Style-only, fast |
| `coverity.yml`           | Daily Coverity scan | Schedule + secrets |
| `nix.yml`                | Nix flake build    | Different build system entirely |
| `release.yml`            | Source tarball on tag | Triggered by tag push |
| `.cirrus.yml`            | FreeBSD 13 + 14   | Different CI system entirely |

**7 GHA workflow files + 1 Cirrus file.** Was 6 GHA + 1 Cirrus before
modernization, but covering a much smaller matrix.

## Supported runner images (as of 2026-07)

Per GitHub docs (verified 2026-07-26):

| OS | Arch | Runner images |
|----|------|---------------|
| Linux | x64 | `ubuntu-latest`, `ubuntu-24.04`, `ubuntu-22.04`, `ubuntu-26.04` (preview) |
| Linux | arm64 | `ubuntu-24.04-arm`, `ubuntu-22.04-arm`, `ubuntu-26.04-arm` (preview) |
| Windows | x64 | `windows-latest`, `windows-2025`, `windows-2025-vs2026` (preview), `windows-2022` |
| Windows | arm64 | `windows-11-arm`, `windows-11-vs2026-arm` (preview) |
| macOS | Intel | `macos-15-intel`, `macos-26-intel` |
| macOS | arm64 (M1) | `macos-latest`, `macos-14`, `macos-15`, `macos-26` |

The `retrace-build.yml` matrix exercises every GA runner plus marked preview
runners with `experimental: true` (continue-on-error).

## Tasks

### [P0] Consolidated build workflow
- [x] `.github/workflows/build.yml` — Linux x64 (3 versions) + Linux arm64 (3 versions) + macOS Intel (2) + macOS arm64 (3) + Windows x64 (3) + Windows arm64 (2) = 16-entry matrix
- [x] Per-OS setup steps gated by `if: runner.os == ...`
- [x] Coverage as additional job in same workflow
- [x] vcpkg cache for Windows jobs
- [x] Concurrency group cancels in-progress on same branch

### [P0] Alpine workflow
- [x] `.github/workflows/alpine.yml`
- [x] x86_64 native container (`alpine:3.21`)
- [x] aarch64 via `docker/setup-qemu-action` + `arm64v8/alpine:3.21`
- [ ] Verify musl + clang combo produces working `LD_PRELOAD` library

### [P0] MSYS2 workflow
- [x] `.github/workflows/msys.yml`
- [x] Matrix: `mingw64`, `ucrt64`
- [x] vcpkg `x64-mingw-static` triplet

### [P0] FreeBSD (Cirrus)
- [x] `.cirrus.yml` rewritten to use CMake build
- [x] FreeBSD 13.4 + 14.2

### [P0] Quality workflows
- [x] `.github/workflows/checkpatch.yml`
- [x] `.github/workflows/coverity.yml` (daily, quarterly cache key)

### [P0] Nix workflow
- [x] `.github/workflows/nix.yml`

### [P0] Release pipeline
- [x] `.github/workflows/release.yml` (source tarball on tag)
- [x] `scripts/build-release-tarball.sh`
- [ ] First real release tag: `v2.1.0` after Phase 2 lands

### [P0] Old workflow removal
- [x] `ubuntu.yml`, `macos.yml`, `windows.yml`, `nix.yml`, `checkpatch.yml`, `coverity.yml` removed
- [x] Old `.cirrus.yml` content replaced in place (no `.new` coexistence)

### [P1] Caching
- [x] vcpkg binary cache (keyed on `VCPKG_REF` pin, bumped quarterly)
- [ ] ccache cache for compiler outputs (`~/.ccache` POSIX, `%LOCALAPPDATA%\ccache` Windows)

### [P1] vcpkg pinning
- [x] `VCPKG_REF: 4f95fba7a7d1101bb8acdeb51e4609686449701e` pinned across all workflows
- [ ] Document quarterly bump procedure in `docs/dev/release.adoc`

### [P2] Composite action for shared steps
- [ ] `.github/actions/setup-build/action.yml` — composite that does checkout + per-OS deps + vcpkg bootstrap
- [ ] Each platform workflow shrinks to ~15 lines using the composite

### [P2] Release pipeline (binary artifacts)
- [ ] Cross-compile for each target triplet
- [ ] Attach prebuilt shared libs to GitHub Release (in addition to source tarball)

## vcpkg pinning

To avoid vcpkg breakages, every workflow pins `VCPKG_REF` to a known-good commit:

```yaml
env:
  VCPKG_REF: 4f95fba7a7d1101bb8acdeb51e4609686449701e  # update quarterly
```

The pin is bumped in a single PR that updates every workflow at once.

## Migration approach: replacement, not coexistence

Per user directive 2026-07-26: modernization replaces existing config in
place. Old workflows (`ubuntu.yml`, `macos.yml`, etc.) are deleted, not
preserved "during transition". This contrasts with the typical "never delete
files" rule — for modernization work, replacement IS the work.

See `feedback_modernization_replaces.md` in agent memory.

## Acceptance criteria

- Every workflow in the matrix runs green on `main`.
- `git rebase main` on a feature branch produces zero CI surprises.
- Total CI wall time per push < 15 minutes (concurrency + caching).
- A new platform/arch is added by editing one matrix `include:` entry.

## Open questions

- Do we keep Nix flake? Yes — `retrace-nix.yml` is the only Nix exercise.
- Do we ship binary artifacts in releases? Lean yes for v2.1, after the
  cross-compilation story is solid.
- Should `retrace-coverity.yml` move into `retrace-build.yml` as a scheduled
  job? Lean no — separate workflow file is clearer for scheduled/secret-scoped jobs.
