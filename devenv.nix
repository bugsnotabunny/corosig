# https://devenv.sh/
{ pkgs, ... }: {
  packages = with pkgs; [ llvmPackages_21.clang-tools xmake gdb ];

  languages.nix.enable = true;
  languages.cplusplus.enable = true;
}
