{
  description = "a versatile security vulnerability / bug discovery tool";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      rec {
        packages.v1 = pkgs.callPackage ./nix/default.nix { };
        packages.v2 = pkgs.callPackage ./nix/v2.nix { };
        packages.v2wrapper = pkgs.callPackage ./nix/v2wrapper.nix { };
        defaultPackage = packages.v1;
      });
}
