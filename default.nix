{
  pkgs ? import <nixpkgs> { },
  glslSupport ? false,
  cudaSupport ? false,
  hipSupport ? false,
  pprSupport ? true,
  kdtSupport ? true,
}:

pkgs.callPackage ./basilisk.nix {
  inherit
    glslSupport
    cudaSupport
    hipSupport
    pprSupport
    kdtSupport
    ;
}
