# https://devenv.sh/
{ pkgs, ... }:
{
  packages = with pkgs; [
    llvmPackages_latest.clang-tools
    xmake
    gdb
  ];

  languages.nix.enable = true;
  languages.cplusplus.enable = true;
}
