{ pkgs ? import <nixpkgs> { }
, lib ? pkgs.lib
, stdenv ? pkgs.stdenv
}:

(pkgs.callPackage ./v2.nix { }).overrideAttrs (oldAttrs: {
  cmakeFlags = oldAttrs.cmakeFlags ++ [ "-DRETRACE_BUILD_CLI=ON" ];
})
