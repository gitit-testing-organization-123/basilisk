
# PacIFiC Basilisk
# ============================================

Name:           pacific-basilisk
Version:        %{?version}
Release:        %{?release}
Summary:        PacIFiC basilisk
License:        MIT
URL:            https://gitlab.math.ubc.ca/pacific-devel-team/pacific-basilisk
Source0:        pacific-basilisk-%{version}.tar.gz
 
BuildRequires:  cmake-rpm-macros

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
 
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
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON 
%cmake_build 

%install
%cmake_install

%files
%license src/COPYING
%{_includedir}/basilisk/*
%{_bindir}/qcc
%{_bindir}/bview2D
%{_bindir}/bview2Dm
%{_bindir}/bview3D
%{_libdir}/libfb_tiny.a
%{_libdir}/libglutils.a
%{_libdir}/libtinyrenderer.a
%{_libdir}/libwsserver.a
%{_libdir}/cmake/basilisk/*

%changelog
* Thu Jan 15 2026 Conor Olive - %{version}-%{release}
- Init
