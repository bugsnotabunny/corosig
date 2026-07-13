# https://devenv.sh/
{ pkgs, ... }: {
  packages = with pkgs; [
    llvmPackages_latest.clang-tools
    llvmPackages_latest.bintools
    llvmPackages_latest.libstdcxxClang
    doxygen
    xmake
    gdb
  ];
}
