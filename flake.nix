{
  description = "Nexilis library for game server development";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          name = "nexilis-devshell";
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.gcc
            pkgs.boost
            pkgs.openssl
            pkgs.gtest
            pkgs.expect
            pkgs.cppcheck
            pkgs.clang-tools
            (pkgs.python3.withPackages (ps: with ps; [
              python-dotenv
              flake8
            ]))
          ];

          shellHook = ''
            echo "Building Nexilis from source..."
            cd ../nexilis/
            rm -rf build install
            mkdir -p build
            cd build
            cmake .. -DCMAKE_INSTALL_PREFIX=$(pwd)/../install
            make
            make install
            cd ../..

            export NEXILIS_PREFIX=$(pwd)/nexilis/nexilis/install
            export CMAKE_PREFIX_PATH=$NEXILIS_PREFIX:$CMAKE_PREFIX_PATH
            export LD_LIBRARY_PATH=$NEXILIS_PREFIX/lib:$LD_LIBRARY_PATH

            echo "Welcome to the Nexilis Dev Shell"
            echo "NEXILIS_PREFIX set to $NEXILIS_PREFIX"
            python scripts/pre_commit/env.py
          '';
        };

        packages = {
          lib = pkgs.stdenv.mkDerivation {
            pname = "nexilis-library";
            version = "0.0.1";
            src = ../nexilis;

            nativeBuildInputs = [ pkgs.cmake ];
            buildInputs = [ pkgs.boost pkgs.openssl ];

            # Force rebuild without using CMake cache.
            dontUseCmakeBuildDir = true;

            configurePhase = ''
              mkdir -p build
              cd build
              cmake .. -DCMAKE_INSTALL_PREFIX=$out
            '';

            buildPhase = ''
              make
            '';

            installPhase = ''
              make install
            '';
          };

          cpp-tests = pkgs.stdenv.mkDerivation {
            pname = "nexilis-tests";
            version = "0.0.1";
            src = ../tests/nexilis;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.patchelf
            ];

            buildInputs = [
              pkgs.boost
              pkgs.openssl
              pkgs.gtest
              self.packages.${system}.lib
            ];

            cmakeFlags = [
              "-DCMAKE_PREFIX_PATH=${self.packages.${system}.lib}"
            ];

            configurePhase = ''
              mkdir -p build
              cd build
              cmake ..
            '';

            buildPhase = "make";

            installPhase = ''
              mkdir -p $out/bin
              cp nexilis_tests $out/bin/
              patchelf --set-rpath ${self.packages.${system}.lib}/lib ./nexilis_tests
            '';
          };
      };
  });
}
