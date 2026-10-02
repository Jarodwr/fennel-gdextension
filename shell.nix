{ pkgs ? import <nixpkgs> { } }:
pkgs.mkShell {
  packages = with pkgs; [
    lua5_4   # build fennel.lua from source (make -C vendor/fennel fennel.lua)
    scons    # gdextension build system
    python3  # code generation scripts
  ];

  shellHook = ''
    export PYTHON_BIN=python3
  '';
}
