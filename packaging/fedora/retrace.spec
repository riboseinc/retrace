# SPDX-License-Identifier: BSD-2-Clause
#
# Fedora RPM spec for retrace (TODO.complete/38).
# Build: rpmbuild -ba retrace.spec
# Install: dnf install retrace-2.10.0-1.*.rpm

Name:           retrace
Version:        2.29.0
Release:        1%{?dist}
Summary:        Userspace libc interceptor for security discovery

License:        BSD-2-Clause
URL:            https://github.com/riboseinc/retrace
Source0:        %{url}/archive/v%{version}/retrace-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  openssl-devel

Requires:       openssl-libs

%description
retrace intercepts libc calls in dynamically-linked binaries by
preloading a shared library. It can log, modify, or fault every
intercepted call. Features include network function interception,
HTTP/DNS protocol decoders, per-return-address routing, filter
action, OTLP/JSON export, and a Python config builder.

%prep
%autosetup

%build
%cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DRETRACE_BUILD_TESTS=OFF \
    -DRETRACE_BUILD_EXAMPLES=OFF
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.adoc CHANGELOG.md
%{_libdir}/libretrace.so.*
%{_bindir}/retrace
%{_bindir}/retrace-to-otlp
%{_includedir}/retrace/

%changelog
* Sun Aug 23 2026 Ribose Inc <opensource@ribose.com> - 2.29.0-1
- snap2inside: personal-files/system-files plug read/write lists mapped (TODO 19 follow-up)
* Sun Aug 23 2026 Ribose Inc <opensource@ribose.com> - 2.28.0-1
- fuzz_str action: dictionary-driven string fuzzing (TODO 25)
* Sun Aug 23 2026 Ribose Inc <opensource@ribose.com> - 2.27.0-1
- scripted Windows ETW kernel truth: etw-capture.ps1 + retrace-etw2retrace (TODO 24)
* Sun Aug 23 2026 Ribose Inc <opensource@ribose.com> - 2.26.0-1
- close TODO 07: the Windows env "mystery" was a reader share-mode bug; RETRACE_WIN_DIAG crutch removed
* Sun Aug 23 2026 Ribose Inc <opensource@ribose.com> - 2.24.0-1
- libFuzzer harness template example
* Sat Aug 22 2026 Ribose Inc <opensource@ribose.com> - 2.21.0-1
- fuzzing workbench: retrace-fuzz-report (clustering + reproducers), RETRACE_FUZZ_SEED determinism
* Sat Aug 22 2026 Ribose Inc <opensource@ribose.com> - 2.20.0-1
- packaging-layer audit + compose hardening export
* Sat Aug 22 2026 Ribose Inc <opensource@ribose.com> - 2.19.0-1
- jail depth: read-only detonation, deception mode (decoy dir), env NAME policy, clock pinning
* Sat Aug 22 2026 Ribose Inc <opensource@ribose.com> - 2.18.0-1
- prototype-driven jail denial returns (NULL for pointers); diff accepts profile docs; reports.md + platforms.md; quickstart example on 4 platforms
* Sat Aug 22 2026 Ribose Inc <opensource@ribose.com> - 2.17.0-1
- dtrace2retrace (macOS) + truss2retrace (FreeBSD) kernel-truth converters; arm64 allowlist hook decoder; Windows jail denial test
* Fri Aug 21 2026 Ribose Inc <opensource@ribose.com> - 2.16.0-1
- Windows env/net hooks (getenv, ws2_32 connect/send/recv), ntdll data ops (NtWriteFile/NtReadFile/NtQueryDirectoryFile), arm64 runtime test un-gate
* Fri Aug 21 2026 Ribose Inc <opensource@ribose.com> - 2.15.0-1
- capture on Windows (delegates to retrace-win-run), jail from a profile doc, armasm64 wrapper (MSVC-arm64 live hooks), aggregation credit fix
* Thu Aug 20 2026 Ribose Inc <opensource@ribose.com> - 2.14.0-1
- trace-profile workstream: capture/diff/validate subcommands, profile schema, expanded ucrt hook set, arm64 wrapper (gas)
* Thu Aug 20 2026 Ribose Inc <opensource@ribose.com> - 2.13.0-1
- Windows: first live hooks (fopen ucrt + opt-in ntdll set), PE-section registry, retrace-win-run launcher, retrace.dll
* Thu Aug 20 2026 Ribose Inc <opensource@ribose.com> - 2.12.0-1
- Profiles: retrace-profile claims-vs-truth risk profiler + sandbox allow_paths jail; retrace-strace2retrace kernel-truth converter; offline tools build on Windows
* Thu Aug 20 2026 Ribose Inc <opensource@ribose.com> - 2.11.2-1
- fopen$DARWIN_EXTSN interposition (macOS _DARWIN_C_SOURCE builds)

* Thu Aug 20 2026 Ribose Inc <opensource@ribose.com> - 2.11.1-1
- Ring-logger exit-drain regression fix; escape-hunting example

* Wed Aug 19 2026 Ribose Inc <opensource@ribose.com> - 2.11.0-1
- v2 core engine builds and links on Windows (portability shim)

* Wed Aug 19 2026 Ribose Inc <opensource@ribose.com> - 2.10.0-1
- RETRACE_LOGGER_FMT=jsonl streaming output; tools read both formats

* Wed Aug 19 2026 Ribose Inc <opensource@ribose.com> - 2.9.0-1
- retrace-procmon2retrace (procmon CSV producer); correlation
  criteria: pid scoping, time window, probe classification

* Wed Aug 19 2026 Ribose Inc <opensource@ribose.com> - 2.7.0-1
- pid/tid on every log entry; retrace-correlate escape tool

* Tue Aug 19 2026 Ribose Inc <opensource@ribose.com> - 2.6.1-1
- Public config-validate API; CLI validate un-stubbed

* Mon Aug 18 2026 Ribose Inc <opensource@ribose.com> - 2.6.0-1
- Public registry introspection API; CLI list-* un-stubbed

* Mon Aug 18 2026 Ribose Inc <opensource@ribose.com> - 2.5.4-1
- CI job timeouts on every workflow

* Tue Aug 18 2026 Ribose Inc <opensource@ribose.com> - 2.5.3-1
- Ring logger capacity 64->1024 default; RETRACE_LOGGER_RING_CAP env override

* Mon Aug 17 2026 Ribose Inc <opensource@ribose.com> - 2.5.2-1
- Property tests for the audit policy matcher

* Mon Aug 17 2026 Ribose Inc <opensource@ribose.com> - 2.5.1-1
- ring-logger contention benchmark (1/2/4/8 producer threads)

* Mon Aug 17 2026 Ribose Inc <opensource@ribose.com> - 2.5.0-1
- Public API matches implementation (ADR-0014): trim phantom decls, implement version fns, surface-guard test

* Sun Aug 16 2026 Ribose Inc <opensource@ribose.com> - 2.4.6-1
- architecture/development/faq docs refreshed for the v2.3.x-v2.4.x era

* Sat Aug 15 2026 Ribose Inc <opensource@ribose.com> - 2.4.5-1
- audit PDF: findings-page off-by-one + cover double-escape fixes; 13 tests

* Sat Aug 15 2026 Ribose Inc <opensource@ribose.com> - 2.4.4-1
- trace-diff stats.c extraction + 10 z-score tests; two-pass variance fix

* Sat Aug 15 2026 Ribose Inc <opensource@ribose.com> - 2.4.3-1
- trace-diff lcs.c extraction (MECE) + 17 LCS alignment tests

* Fri Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.4.2-1
- audit format.c extraction (MECE) + 11 formatter tests

* Fri Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.4.1-1
- audit scan.c extraction (MECE) + 11 scan-engine unit tests

* Fri Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.4.0-1
- Native process attach via ptrace (retrace attach <pid>) + backends listing

* Thu Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.3.8-1
- Tutorials 23-27 covering the v2.3.0 tool ecosystem

* Thu Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.3.7-1
- parson: NULL-input safety + duplicate-static cleanup + budget regression tests

* Thu Aug 14 2026 Ribose Inc <opensource@ribose.com> - 2.3.6-1
- trace-diff threshold.c extraction (MECE) + 15 unit tests

* Wed Aug 13 2026 Ribose Inc <opensource@ribose.com> - 2.3.5-1
- Reference docs for capture_buffer action and CALL_HASH / LOGGER_RING env vars

* Wed Aug 13 2026 Ribose Inc <opensource@ribose.com> - 2.3.4-1
- README.adoc + cli.md updated for v2.3.x tools and fuzz-replay subcommand

* Wed Aug 13 2026 Ribose Inc <opensource@ribose.com> - 2.3.3-1
- Cookbook recipes for every v2.3.0 tool + tools.md overview

* Wed Aug 13 2026 Ribose Inc <opensource@ribose.com> - 2.3.2-1
- Unit tests hardened: CHECK macro replaces side-effecting asserts

* Tue Aug 12 2026 Ribose Inc <opensource@ribose.com> - 2.3.1-1
- Audit policy MECE refactor + unit tests for policy/normalize

* Sat Aug 09 2026 Ribose Inc <opensource@ribose.com> - 2.3.0-1
- Initial Fedora packaging
