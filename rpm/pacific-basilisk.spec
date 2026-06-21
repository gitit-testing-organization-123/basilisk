
# PacIFiC Basilisk
# ============================================

Name:           pacific-basilisk
Version:        %{?version}
Release:        %{?release}
Summary:        PacIFiC basilisk
License:        MIT
URL:            https://github.com/PacIFiC-Development-Team/basilisk
Source0:        pacific-basilisk-%{version}.tar.gz
 
BuildRequires:  cmake-rpm-macros

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  gcc-gfortran
BuildRequires:  glfw-devel
BuildRequires:  bison
BuildRequires:  flex
BuildRequires:  mesa-libGL-devel
BuildRequires:  make

Requires:       gcc
Requires:       openmpi
Requires:       python3
Requires:       swig
Requires:       ffmpeg
Requires:       ImageMagick
Requires:       gifsicle
Requires:       gnuplot
Requires:       glfw
Requires:       mesa-libGL
Requires:       libgfortran
 
%description
Basilisk

%global __spec_build_shell /bin/bash
%global _hardened_build 0
%global _lto_cflags %{nil}
%global _annotated_build 0

%prep
%autosetup 

%build 
%cmake -G "Unix Makefiles" \
  -DCMAKE_SKIP_RPATH:BOOL=ON \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH:BOOL=OFF \
  -DCMAKE_INSTALL_RPATH:STRING= \
  -DCMAKE_INSTALL_RPATH_USE_LINK_PATH:BOOL=OFF \
  -DBUILD_SHARED_LIBS:BOOL=ON \
  -DCMAKE_BUILD_TYPE:STRING=Debug \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
  -DBASILISK_USE_PPR=ON \
  -DBASILISK_USE_KDT=ON \
  -DBASILISK_USE_CVMIX=OFF \
  -DBASILISK_USE_GLSL=ON \
  -DBASILISK_USE_CUDA=OFF \
  -DBASILISK_USE_HIP=OFF 
%cmake_build 

%install
%cmake_install

%files
%license src/COPYING
%{_includedir}/basilisk/*
%{_bindir}/bview2D
%{_bindir}/bview2Dm
%{_bindir}/bview3D
%{_bindir}/kdtquery
%{_bindir}/ppm2*
%{_bindir}/qcc
%{_bindir}/xyz2kdt
%{_libdir}/cmake/basilisk/*
%{_libdir}/liberrors.a
%{_libdir}/libfb_tiny.a
%{_libdir}/libglutils.a
%{_libdir}/libgpu.a
%{_libdir}/libkdt.a
%{_libdir}/libppr.a
%{_libdir}/libtinyrenderer.a
%{_libdir}/libws.a

%changelog
* Thu Jan 15 2026 Conor Olive - %{version}-%{release}
- Init
