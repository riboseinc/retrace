# SPDX-License-Identifier: BSD-2-Clause
#
# Fedora RPM spec for retrace (TODO.complete/38).
# Build: rpmbuild -ba retrace.spec
# Install: dnf install retrace-2.3.2-1.*.rpm

Name:           retrace
Version:        2.3.2
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
* Wed Aug 13 2026 Ribose Inc <opensource@ribose.com> - 2.3.2-1
- Unit tests hardened: CHECK macro replaces side-effecting asserts

* Tue Aug 12 2026 Ribose Inc <opensource@ribose.com> - 2.3.1-1
- Audit policy MECE refactor + unit tests for policy/normalize

* Sat Aug 09 2026 Ribose Inc <opensource@ribose.com> - 2.3.0-1
- Initial Fedora packaging
