{ lib, stdenv
, fetchFromGitHub
, cmake
, pkg-config
, freeglut
, liblapack
, libusb1
, openblas
, zlib
}:

let

keepDebugInfo = stdenv: stdenv //
  { mkDerivation = args: stdenv.mkDerivation (args // {
      dontStrip = true;
      NIX_CFLAGS_COMPILE = toString (args.NIX_CFLAGS_COMPILE or "") + " -g -ggdb -Og";
    });
  };
stdenvDebug = keepDebugInfo stdenv;
openblas-debug = openblas.override { stdenv = stdenvDebug; };
in

stdenvDebug.mkDerivation rec {
  pname = "libsurvive";
  version = "0.3";

  src = fetchFromGitHub {
    owner = "cntools";
    repo = pname;
    # rev = "v${version}";
    rev = "e5dee4d548af5cfcefd1ce87755f3a3a0860db57";
    sha256 = "1siymr44vsrkcq8qrnbxxrd1nq2v4ymcsvhzym9g3a9kv542h76d";
    # sha256 = "e5dee4d548af5cfcefd1ce87755f3a3a0860db57";
  };

  nativeBuildInputs = [ cmake pkg-config ];

  buildInputs = [
    freeglut
    liblapack
    libusb1
    openblas-debug
    zlib
  ];

  dontStrip = true;

  meta = with lib; {
    description = "Open Source Lighthouse Tracking System";
    homepage = "https://github.com/cntools/libsurvive";
    license = licenses.mit;
    maintainers = with maintainers; [ expipiplus1 prusnak ];
    platforms = platforms.linux;
  };
}
