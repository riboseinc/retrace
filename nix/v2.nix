{ pkgs ? import <nixpkgs> { }
, lib ? pkgs.lib
, stdenv ? pkgs.stdenv
}:

(pkgs.callPackage ./default.nix { }).overrideAttrs (oldAttrs: {
  configureFlags = oldAttrs.configureFlags ++ [ "--enable-v2" ];
  makeFlags = [ "V=1" ];
})
