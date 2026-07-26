# 11 — Documentation

**Status**: [ ] pending
**Layer**: cross-cutting
**Depends on**: 02, 05, 06
**Blocks**: nothing

## Goal

Provide three documentation tracks, each with its own audience:

1. **API reference** — for library consumers (Doxygen, generated from headers)
2. **User guide** — for CLI users (AsciiDoctor → HTML + PDF + manpages)
3. **Architecture** — for contributors (ADRs, this TODO.roadmap, CONTRIBUTING)

## Why

Current docs are sparse:

- `README.adoc` covers basic v1 usage.
- `READMEv2.adoc` covers v2 basics but admits "early documentation".
- `Developers Guide.md` is from 2017, references Travis CI.
- No API reference for library consumers.
- No CONTRIBUTING.
- No architecture document.

## File layout

```
docs/
├── adr/                           # Architecture Decision Records (TODO 12)
│   ├── 0001-cmake-as-primary-build.md
│   ├── 0002-layered-architecture.md
│   └── ...
├── api/                           # Doxygen source
│   ├── Doxyfile
│   └── mainpage.dox
├── user/
│   ├── book.adoc                  # master AsciiDoctor document
│   ├── installation.adoc
│   ├── quickstart.adoc
│   ├── config-json.adoc
│   ├── config-text.adoc
│   ├── backends.adoc
│   ├── actions.adoc
│   ├── examples.adoc
│   └── migrating-from-v1.adoc    # TODO 10
├── man/                           # manpage AsciiDoctor sources
│   ├── retrace.1.adoc
│   ├── retrace-run.1.adoc
│   ├── retrace-script.1.adoc
│   └── ...
├── dev/
│   ├── CONTRIBUTING.adoc          # was: Developers Guide.md
│   ├── architecture.adoc          # the layer diagram from TODO 00
│   ├── coding-style.adoc
│   └── testing.adoc
└── images/                        # diagrams (graphviz sources + rendered PNG/SVG)
```

## Doxygen API reference

```bash
# Generate
cmake --build build --target retrace-docs
# Output: build/docs/api/html/index.html
```

Every public header in `include/retrace/` has Doxygen comments on every
function and type. Internal headers (in `src/*/internal/`) are excluded
via `EXCLUDE` in `Doxyfile`.

## User guide

Single AsciiDoctor book that compiles to:

- HTML (multi-page): published to `https://retrace.readthedocs.io` via
  `.readthedocs.yaml`
- PDF: attached to GitHub Release
- man pages: extracted from `docs/man/` and installed by CMake

Sample chapters:
1. Quickstart (10 lines to first trace)
2. Installation (vcpkg, conan, system pkg, source)
3. Configuration: JSON format
4. Configuration: text format (v1 compat)
5. Built-in actions (reference)
6. Built-in backends
7. Writing custom actions (library API)
8. Writing custom backends
9. CLI reference
10. Recipes (TLS interception, malloc fuzzing, env fuzzing, …)
11. Migration from v1

## Tasks

### [P0] Doxygen
- [ ] `docs/api/Doxyfile` configured
- [ ] Document every public function in `include/retrace/*.h`
- [ ] Document every public type
- [ ] `mainpage.dox` with overview + links to user guide
- [ ] CMake target `retrace-docs` builds the HTML

### [P0] CONTRIBUTING
- [ ] Convert `Developers Guide.md` to `docs/dev/CONTRIBUTING.adoc`
- [ ] Drop Travis CI references
- [ ] Add "how to run tests" pointing at TODO 08
- [ ] Add "how to add a new function" pointing at TODO 02
- [ ] Add "how to add a new action" pointing at TODO 02
- [ ] Add "how to add a new backend" pointing at TODO 03

### [P0] README refresh
- [ ] `README.adoc` updated with new build instructions
- [ ] Badges updated to new workflow filenames (see TODO 09)
- [ ] Add "as a library" section pointing at user guide chapter 1

### [P1] User guide skeleton
- [ ] `docs/user/book.adoc` master with all chapter stubs
- [ ] `quickstart.adoc` (port existing README examples)
- [ ] `config-json.adoc` (port READMEv2.adoc)
- [ ] `actions.adoc` (reference for every built-in action)
- [ ] `backends.adoc` (reference for every backend)
- [ ] `migrating-from-v1.adoc` (TODO 10)

### [P1] Manpages
- [ ] `docs/man/retrace.1.adoc`
- [ ] `docs/man/retrace-run.1.adoc`
- [ ] One per subcommand (TODO 06)
- [ ] CMake installs to `${CMAKE_INSTALL_MANDIR}/man1/`

### [P1] ReadTheDocs
- [ ] `.readthedocs.yaml` configured for AsciiDoctor + CMake-built Doxygen
- [ ] Builds on every push to main; publishes to readthedocs

### [P2] Diagrams
- [ ] Layer diagram (graphviz, source in `docs/images/`)
- [ ] Backend dispatch sequence diagram
- [ ] Config-source data flow

### [P2] Examples refresh
- [ ] `examples/*/README.md` updated with new CLI invocation
- [ ] All examples tested in CI as smoke tests (TODO 08)

### [P2] Changelog
- [ ] `CHANGELOG.adoc` following Keep-a-Changelog format
- [ ] Release process document in `docs/dev/release.adoc`

## Acceptance criteria

- `cmake --build build --target retrace-docs` produces no warnings.
- Every public function has a Doxygen comment with at least one usage example.
- User guide renders to HTML without errors via `asciidoctor docs/user/book.adoc`.
- `man -l man/retrace.1` renders cleanly.
- A new contributor can land a working PR within an hour using only
  `CONTRIBUTING.adoc` + this TODO.roadmap.

## Open questions

- Host on GitHub Pages or ReadTheDocs? Lean ReadTheDocs for the versioning.
- Translate the user guide? Lean no for v2.0 — English only, translation is a
  separate effort.
