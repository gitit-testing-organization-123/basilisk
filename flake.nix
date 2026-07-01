{
  description = "PacIFiC Basilisk CMake package";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };

          mkBasilisk = args: pkgs.callPackage ./basilisk.nix args;

          basilisk = mkBasilisk { };
          basilisk-glsl = mkBasilisk {
            glslSupport = true;
          };
          basilisk-cuda = mkBasilisk {
            cudaSupport = true;
          }; 
          basilisk-hip = mkBasilisk {
            hipSupport = true;
          };
        in
        {
          default = basilisk;
          inherit
            basilisk
            basilisk-glsl
            basilisk-cuda
            basilisk-hip
            ;
        });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = with pkgs; [
              gdb
              strace
              doxygen
              openmpi
              python3
              swig
              glfw
              libGL
              cudaPackages.cuda_cudart
              cudaPackages.cuda_nvrtc
              cudaPackages.cuda_nvcc
              rocmPackages.clr
              rocmPackages.hipcc
              gfortran
              netcdffortran
              netcdf
              hdf5
              curl
              zlib
            ];

            shellHook = ''
              export CUDA_PATH=${pkgs.cudaPackages.cuda_nvcc}
              export CUDAToolkit_ROOT=${pkgs.cudaPackages.cuda_nvcc}
              export CUDA_CUDART_ROOT=${pkgs.cudaPackages.cuda_cudart}
              export CUDA_NVRTC_ROOT=${pkgs.cudaPackages.cuda_nvrtc}
              export HIP_PATH=${pkgs.rocmPackages.clr}
              export HIPCC_ROOT=${pkgs.rocmPackages.hipcc}
              export FC=${pkgs.gfortran}/bin/gfortran
            '';
          };
        });
    };
}
