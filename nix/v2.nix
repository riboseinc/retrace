{ pkgs ? import <nixpkgs> { }
, lib ? pkgs.lib
, stdenv ? pkgs.stdenv
}:

stdenv.mkDerivation rec {
  pname = "retrace-v2";
  version = "unstable";

  src = ./..;

  buildInputs = with pkgs; [
    openssl
  ];

  nativeBuildInputs = with pkgs; [
    cmake
    pkg-config
    ninja
  ];

  cmakeFlags = [
    "-DRETRACE_BUILD_V2=ON"
    "-DRETRACE_BUILD_TESTS=OFF"
  ];
}
