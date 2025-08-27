# https://devenv.sh/
{ pkgs, ... }: {
  packages = with pkgs; [ llvmPackages_latest.clang-tools xmake valgrind gdb ];

  languages.cplusplus.enable = true;
  languages.nix.enable = true;
  languages.lua.enable = true;
}
