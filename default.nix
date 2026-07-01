
{ pkgs ? import <nixpkgs> {} }:

let
  lib = pkgs.lib;

  pname = "pbacific-basilisk";
  version = "0.0.1";

  package = pkgs.stdenv.mkDerivation {
    inherit pname version;
    src = ./.;

    nativeBuildInputs = with pkgs; [ 
      cmake
      gawk
      bison
      flex      
      zlib
      readline
      libpng
    ];

    buildInputs = with pkgs; [ 
    ];

    configurePhase = ''
      cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$out
    '';

    buildPhase = ''cmake --build build'';
    
    installPhase = ''cmake --install build'';
  };

  shell = pkgs.mkShell {
    inputsFrom = [ package ];
    packages = with pkgs; [ 
      gdb 
      strace
      doxygen
      ninja
    ];
  };

in
# nix-shell sets IN_NIX_SHELL, nix-build doesn't.
if lib.inNixShell then shell else package
