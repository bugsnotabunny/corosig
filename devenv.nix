# https://devenv.sh/
{ pkgs, ... }: {
  packages = with pkgs; [
    llvmPackages_21.clang-tools
    llvmPackages_19.libstdcxxClang
    doxygen
    xmake
    gdb
  ];
}
