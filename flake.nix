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
        # v1 was removed in Phase 9 (ADR-0011). v2 is the only build.
        packages.v2 = pkgs.callPackage ./nix/v2.nix { };
        packages.v2wrapper = pkgs.callPackage ./nix/v2wrapper.nix { };
        defaultPackage = packages.v2;
      });
}
