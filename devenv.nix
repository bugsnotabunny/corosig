# https://devenv.sh/
{ pkgs, ... }: {
  packages = with pkgs; [
    llvmPackages_latest.clang-tools
    xmake
    gdb
  ];

  languages.cplusplus.enable = true;
  languages.nix.enable = true;
  languages.lua.enable = true;
}
