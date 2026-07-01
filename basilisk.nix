{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  makeWrapper,
  gawk,
  bison,
  flex,
  glfw,
  libGL,
  cudaPackages,
  rocmPackages,
  gfortran,
  netcdffortran,
  netcdf,
  hdf5,
  curl,
  zlib,
  openmpi,
  python3,
  swig,
  gnuplot,
  gifsicle,
  ffmpeg,
  imagemagick,
  glslSupport ? false,
  cudaSupport ? false,
  hipSupport ? false,
  pprSupport ? true,
  kdtSupport ? true,
  cvmixSupport ? false,
  gotmSupport ? false,
}:

let
  inherit (lib) optional optionals optionalString;
  mpiDev = lib.getDev openmpi;
in
stdenv.mkDerivation {
  version = "0.0.1";
  pname =
    "pacific-basilisk"
    + optionalString glslSupport "-glsl"
    + optionalString cudaSupport "-cuda"
    + optionalString hipSupport "-hip"
    + optionalString cvmixSupport "-cvmix"
    + optionalString gotmSupport "-gotm"
    + optionalString (!pprSupport) "-without-ppr"
    + optionalString (!kdtSupport) "-without-kdt";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    makeWrapper
    gawk
    bison
    flex
  ]
  ++ optional cudaSupport cudaPackages.cuda_nvcc
  ++ optional hipSupport rocmPackages.hipcc
  ++ optional (pprSupport || cvmixSupport || gotmSupport) gfortran
  ++ optional gotmSupport netcdffortran;

  buildInputs =
    optionals glslSupport [
      glfw
      libGL
    ]
    ++ optionals cudaSupport [
      cudaPackages.cuda_cudart
      cudaPackages.cuda_nvrtc
    ]
    ++ optionals hipSupport [
      rocmPackages.clr
    ]
    ++ optionals gotmSupport [
      netcdffortran
      netcdf
      hdf5
      curl
      zlib
    ];

  propagatedBuildInputs = [
    openmpi
    python3
    swig
    gnuplot
    ffmpeg
    imagemagick
    gifsicle
    stdenv.cc.cc.lib
  ]
  ++ optional (pprSupport || cvmixSupport || gotmSupport) gfortran.cc.lib;

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
  ]
  ++ optional cudaSupport "-DBASILISK_USE_CUDA=ON"
  ++ optional glslSupport "-DBASILISK_USE_GLSL=ON"
  ++ optional hipSupport "-DBASILISK_USE_HIP=ON"
  ++ optional pprSupport "-DBASILISK_USE_PPR=ON"
  ++ optional kdtSupport "-DBASILISK_USE_KDT=ON"
  ++ optional cvmixSupport "-DBASILISK_USE_CVMIX=ON"
  ++ optional gotmSupport "-DBASILISK_USE_GOTM=ON";

  passthru = {
    inherit
      glslSupport
      cudaSupport
      hipSupport
      pprSupport
      kdtSupport
      cvmixSupport
      gotmSupport
      ;
  };

  postInstall = ''
    wrapProgram "$out/bin/qcc" \
      --prefix PATH : ${lib.makeBinPath [
        mpiDev
        openmpi
        python3
        swig
      ]} \
      --set CC99 "${mpiDev}/bin/mpicc -std=c99 -D_XOPEN_SOURCE=700 -D_GNU_SOURCE=1 -pedantic -Wno-unused-result -Wno-overlength-strings -fno-diagnostics-show-caret" \
      --set CPP99 "${mpiDev}/bin/mpicc -E -std=c99 -D_XOPEN_SOURCE=700 -D_GNU_SOURCE=1" \
      --set PYTHONINCLUDE "${python3}/include/${python3.libPrefix}" \
      --set MDFLAGS "-fpic"
  '';

  meta = {
    description = "CMake build of the PacIFiC Basilisk source tree";
    homepage = "https://basilisk.fr";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.unix;
    mainProgram = "qcc";
  };
}
 
