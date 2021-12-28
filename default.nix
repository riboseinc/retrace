{ pkgs ? import <nixpkgs> { }
}:
let
  inherit (pkgs) stdenv lib pkg-config openssl autoreconfHook;
in
stdenv.mkDerivation {
  pname = "retrace";
  version = "unstable";

  src = ./.;

  buildInputs = [ openssl ];

  nativeBuildInputs = [
    autoreconfHook
    pkg-config
  ];

  outputs = [ "out" "lib" "dev" ];
}
