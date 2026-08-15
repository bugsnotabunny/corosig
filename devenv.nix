# https://devenv.sh/
{
  profiles = {
    noCaches.module = {
      cachix.enable = false;
    };

    caches.module = {
      cachix = {
        enable = true;
        pull = [
          "nix-community"
          "devenv"
          "numtide"
        ];
      };
    };

    gcc.module = { pkgs, ... }: {
      packages = with pkgs; [
        llvmPackages_latest.bintools
        gccStdenv
        xmake
      ];
    };

    clang.module = { pkgs, ... }: {
      packages = with pkgs; [
        llvmPackages_latest.bintools
        llvmPackages_latest.libcxxClang
        xmake
      ];
    };

    codestyle.module = { pkgs, ... }: {
      packages = with pkgs; [
        llvmPackages_latest.clang-tools
        doxygen
        xmake
      ];
    };

    debuggers.module = { pkgs, ... }: {
      packages = with pkgs; [
        gdb
        llvmPackages_latest.lldb
      ];
    };

    dev = {
      extends = [
        "caches"
        "gcc"
        "clang"
        "codestyle"
        "debuggers"
      ];
    };
  };
}
