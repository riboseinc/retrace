%global reldate 20170610
%global origname retrace
%define debug_package %{nil}

Name:           retrace
Version:        0.9.0
Release:        1
Summary:        Linux ELF tracing library and Utility

License:        Ribose Inc
URL:            https://github.com/riboseinc/%{origname}

Source0:        %{origname}-%{version}.tar.gz

BuildRequires:  ncurses-devel

%description
"retrace" is Linux (shared object) and macOS (dynamic library) that displays
C library calls and has the ability to redirect function inputs and outputs

BuildArch:      noarch

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make install DEST_DIR=$RPM_BUILD_ROOT

%files

%{_bindir}/%{origname}.sh
%{_libdir}/%{origname}.so
