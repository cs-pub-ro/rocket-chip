{
  description = "rocket-chip";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # Add a new input specifically for newer packages
    newerNixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable-small"; # More up-to-date
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, newerNixpkgs, flake-utils }@inputs:
    let
      overlay = import ./overlay.nix;
    in
    flake-utils.lib.eachDefaultSystem
      (system:
      let
        pkgs = import nixpkgs { inherit system; overlays = [ overlay ]; };
        newerPkgs = import newerNixpkgs { inherit system; };
        
        llvmPackages19 = newerPkgs.llvmPackages_19;
        
        # List of LLVM tools to wrap with -19 suffix
        llvmTools = [
          "clang"
          "clang++"
          "llvm-config"
          "llvm-ar"
          "llvm-as"
          "llvm-dis"
          "llvm-link"
          "llvm-nm"
          "llvm-objcopy"
          "llvm-objdump"
          "llvm-ranlib"
          "llvm-readelf"
          "llvm-size"
          "llvm-strip"
          "ld.lld"
          "lld"
          "opt"
        ];
        
        # Function to create wrapper for a tool
        createLLVMWrapper = toolName: let
          # Determine which package contains the tool
          toolPackage = if (toolName == "clang" || toolName == "clang++") then llvmPackages19.clang
                       else if (toolName == "ld.lld" || toolName == "lld") then llvmPackages19.lld
                       else llvmPackages19.llvm;
          # Handle special case for clang++
          actualToolName = if toolName == "clang++" then "clang++" else toolName;
        in pkgs.writeShellScriptBin "${toolName}-19" ''
          exec "${toolPackage}/bin/${actualToolName}" "$@"
        '';
        
        # Create all LLVM wrappers
        llvmWrappers = map createLLVMWrapper llvmTools;
        
        deps = with pkgs; [
          jdk17
          git
          gnumake autoconf automake
          mill
          dtc
          verilator cmake ninja
          python3
          python3Packages.pip
          pkgsCross.riscv64-embedded.buildPackages.gcc
          pkgsCross.riscv64-embedded.buildPackages.gdb
          pkgs.pkgsCross.riscv64-embedded.riscv-pk
          openocd
          circt
          spike riscvTests
        ];
      in
        {
          legacyPackages = pkgs;
          devShell = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            buildInputs = deps ++ [ 
              llvmPackages19.clang 
              llvmPackages19.llvm 
              llvmPackages19.lld 
            ] ++ llvmWrappers;
            SPIKE_ROOT = "${pkgs.spike}";
            RISCV_TESTS_ROOT = "${pkgs.riscvTests}";
            RV64_TOOLCHAIN_ROOT = "${pkgs.pkgsCross.riscv64-embedded.buildPackages.gcc}";
            JAVA_HOME = "${pkgs.jdk17.home}";
            shellHook = ''
              # Tells pip to put packages into $PIP_PREFIX instead of the usual locations.
              # See https://pip.pypa.io/en/stable/user_guide/#environment-variables.
              export PIP_PREFIX=$(pwd)/venv/pip_packages
              export PYTHONPATH="$PIP_PREFIX/${pkgs.python3.sitePackages}:$PYTHONPATH"
              export PATH="$PIP_PREFIX/bin:$PATH"
              unset SOURCE_DATE_EPOCH
              pip3 install importlib-metadata typing-extensions riscof==1.25.2 pexpect
              export ROCKETCHIP=$(pwd)
            '';
          };
        }
      )
    // { inherit inputs; overlays.default = overlay; };
}