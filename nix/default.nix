{ pkgs ? import <nixpkgs> { }
, lib ? pkgs.lib
, stdenv ? pkgs.stdenv
}:

stdenv.mkDerivation rec {
  pname = "retrace";
  version = "unstable";

  src = ./..;

  buildInputs = with pkgs; [
    openssl
  ];

  configureFlags = [
    "--disable-silent-rules"
    "--enable-tests"
    "--with-cmocka=${pkgs.cmocka}"
  ];

  nativeBuildInputs = with pkgs; [
    autoreconfHook
    pkg-config
  ];

  checkInputs = with pkgs; [
    cmocka
  ];

  doCheck = true;

  outputs = [ "out" "lib" "dev" ];

  meta = with lib; {
    homepage = "https://github.com/riboseinc/retrace";
    description = "a versatile security vulnerability / bug discovery tool";
    license = licenses.bsd2;
    platforms = platforms.all;
    maintainers = with maintainers; [ ribose-jeffreylau ];
  };
}
