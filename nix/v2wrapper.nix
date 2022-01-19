{ pkgs ? import <nixpkgs> { }
, lib ? pkgs.lib
, stdenv ? pkgs.stdenv
}:

(pkgs.callPackage ./v2.nix { }).overrideAttrs (oldAttrs: {
  configureFlags = oldAttrs.configureFlags ++ [ "--enable-v2_wrapper" ];
})
