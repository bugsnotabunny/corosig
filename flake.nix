{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = inputs:
    let
      supportedSystems =
        [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSupportedSystem = f:
        inputs.nixpkgs.lib.genAttrs supportedSystems
        (system: f { pkgs = import inputs.nixpkgs { inherit system; }; });
    in {
      devShells = forEachSupportedSystem ({ pkgs }: {

        default = pkgs.mkShell.override {
          stdenv = pkgs.llvmPackages_21.libcxxStdenv;
        } {
          packages = with pkgs; [
            llvmPackages_latest.clang-tools
            cmake
            valgrind
            gdb
            libunwind
            foonathan-memory
          ];
          shellHook =
            let libPath = pkgs.lib.makeLibraryPath (with pkgs; [ libunwind ]);
            in ''
              export LD_LIBRARY_PATH=${libPath}:$LD_LIBRARY_PATH
            '';
        };
      });
    };
}
