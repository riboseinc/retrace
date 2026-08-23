# SPDX-License-Identifier: BSD-2-Clause
#
# Nix derivation for retrace (TODO.complete/38).
# Build: nix-build -E 'with import <nixpkgs> {}; callPackage ./default.nix {}'
# Install: nix-env -iA retrace -f default.nix

{ lib, stdenv, cmake, ninja, openssl, python3 }:

stdenv.mkDerivation rec {
  pname = "retrace";
  version = "2.31.0";

  src = ./.;

  nativeBuildInputs = [ cmake ninja ];

  buildInputs = [ openssl ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_SHARED_LIBS=ON"
    "-DRETRACE_BUILD_TESTS=OFF"
    "-DRETRACE_BUILD_EXAMPLES=OFF"
  ];

  meta = with lib; {
    description = "Userspace libc interceptor for security discovery";
    homepage = "https://github.com/riboseinc/retrace";
    license = licenses.bsd2;
    platforms = platforms.linux ++ platforms.darwin;
    maintainers = [{
      name = "Ribose Inc";
      email = "opensource@ribose.com";
    }];
  };
}
