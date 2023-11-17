{
  description = "bell - Audio utilities used in cspot and euphonium project";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-23.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
      };

      packages = {
        default = pkgs.callPackage ./package.nix {
          stdenv = pkgs.llvmPackages_16.stdenv;
        };
        # lib = llvm.stdenv.mkDerivation {
        #   name = "bell";
        #   src = ./.;
        #   nativeBuildInputs = with pkgs; [ cmake ninja ];
        #   buildInputs = with pkgs; [ unstable.mbedtls avahi avahi-compat ];
        #   enableParallelBuilding = true;
        # };
      };
      devShells = {
        default = pkgs.mkShell.override {stdenv = pkgs.llvmPackages_16.stdenv;} {
          packages = with pkgs; [
            # dev tools
            cmake
            ninja
            catch2

            # deps
            mbedtls
            portaudio
            avahi
          ];
        };
      };
    in {
      inherit devShells packages;
      checks = packages;
    });
}
