{
  lib,
  stdenv,
  cmake,
  mbedtls,
  avahi,
  ninja,
  catch2,
  enableTests ? true,
}:
stdenv.mkDerivation {
  name = "bell";

  src = lib.sourceByRegex ./. [
    "^main.*"
    "^test.*"
    "^external.*"
    "CMakeLists.txt"
  ];

  nativeBuildInputs = [cmake ninja];
  buildInputs = [mbedtls avahi];
  checkInputs = [catch2];

  doCheck = enableTests;
  cmakeFlags = lib.optional enableTests "-DBELL_DISABLE_TESTS=OFF";
}
