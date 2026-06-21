{
  pkgs ? import <nixpkgs> { },
  glslSupport ? false,
  cudaSupport ? false,
  hipSupport ? false,
  pprSupport ? true,
  kdtSupport ? true,
  cvmixSupport ? false,
}:

pkgs.callPackage ./basilisk.nix {
  inherit
    glslSupport
    cudaSupport
    hipSupport
    pprSupport
    kdtSupport
    cvmixSupport
    ;
}
