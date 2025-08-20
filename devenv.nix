# https://devenv.sh/

{ pkgs, lib, config, inputs, ... }:
{
  packages = with pkgs; [
    llvmPackages_latest.clang-tools
    xmake
    valgrind
    gdb
  ];

  languages.cplusplus.enable = true;
  languages.nix.enable = true;
}
