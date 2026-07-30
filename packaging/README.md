# Packaging

Distribution channels for retrace, beyond building from source.

## Homebrew (macOS / Linuxbrew)

```sh
$ brew tap riboseinc/retrace https://github.com/riboseinc/retrace
$ brew install retrace
$ retrace --help
```

Or install directly from this repo:

```sh
$ brew install --HEAD \
    https://raw.githubusercontent.com/riboseinc/retrace/main/packaging/homebrew/Formula/retrace.rb
```

The formula builds the latest `main` (`--HEAD`) or a release tag.
The cookbook and the `retrace-logpp` pretty-printer are installed
into `$(brew --prefix)/share/retrace/`.

## vcpkg (Windows / cross-platform)

Planned. Track via [#4780](https://github.com/riboseinc/retrace/issues).

## Nixpkgs

`flake.nix` at the repo root builds the library + a wrapper script:

```sh
$ nix build github:riboseinc/retrace
$ nix run github:riboseinc/retrace -- --help
```

See [flake.nix](../../flake.nix) and the [Nix documentation](../../nix/).

## Debian / Ubuntu APT

Planned. The CMake `install` target already produces a standard
FHS layout (`/usr/lib`, `/usr/bin`, `/usr/include/retrace`) which
packaging tools like `cpack` or `debhelper` can consume. Track
via [#4781](https://github.com/riboseinc/retrace/issues).

## Docker

Planned. Target images:

- `ghcr.io/riboseinc/retrace:linux-x86_64` — for CI use.
- `ghcr.io/riboseinc/retrace:linux-aarch64` — for cross-arch testing.

Track via [#4782](https://github.com/riboseinc/retrace/issues).

## Release artifacts

Every tagged release produces binary artifacts for 8 platforms,
attached to the [releases page](https://github.com/riboseinc/retrace/releases):

- Linux x86_64 + aarch64 (glibc)
- Linux x86_64 + aarch64 (musl / Alpine)
- macOS x86_64 + arm64
- Windows x86_64 + arm64
- OHOS aarch64 (signed)

Plus the source tarball.
