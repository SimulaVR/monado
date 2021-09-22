{ lib
, stdenv
, fetchFromGitLab
, writeText
, cmake
, doxygen
, glslang
, pkg-config
, python3
, SDL2
, dbus
, eigen
, ffmpeg
, gst_all_1
, hidapi
, libGL
, libXau
, libXdmcp
, libXrandr
, libffi
, libjpeg
, libusb1
, libuv
, libuvc
, libv4l
, libxcb
, opencv4
, openhmd
, udev
, vulkan-headers
, vulkan-loader
, wayland
, wayland-protocols
, zlib
# Set as 'false' to build monado without service support, i.e. allow VR
# applications linking against libopenxr_monado.so to use OpenXR standalone
# instead of via the monado-service program. For more information see:
# https://gitlab.freedesktop.org/monado/monado/-/blob/master/doc/targets.md#xrt_feature_service-disabled
, serviceSupport ? true
, callPackage
, writeShellScriptBin
, zstd
, awscli
, libbsd
}:
let

rr = callPackage ./nix/rr/unstable.nix {};

/* Modify a stdenv so that it produces debug builds; that is,
  binaries have debug info, and compiler optimisations are
  disabled. */
keepDebugInfo = stdenv: stdenv //
  { mkDerivation = args: stdenv.mkDerivation (args // {
      dontStrip = true;
      NIX_CFLAGS_COMPILE = toString (args.NIX_CFLAGS_COMPILE or "") + " -g -ggdb -Og";
      DEBUG = "1";
    });
  };
stdenvDebug = keepDebugInfo stdenv;

openblas-dev = callPackage ./openblas.nix { stdenv = stdenvDebug; };
libsurvive = callPackage ./libsurvive.nix { stdenv = stdenvDebug; openblas = openblas-dev; };
libsurvive-dev = libsurvive.overrideAttrs (oldAttrs: rec {

NIX_CFLAGS_COMPILE = toString (oldAttrs.NIX_CFLAGS_COMPILE or "") + " -ffile-prefix-map=/build/source/build/redist=. -ffile-prefix-map=/build/source/src=./result/srcs/libsurvive/src -ffile-prefix-map=/build/source/build/src=./result/srcs/libsurvive -ffile-prefix-map=/build/source/redist=redist -ffile-prefix-map=/build/source/build/src=./result/srcs/libsurvive/src -ffile-prefix-map=/build/source/build/src=. -ffile-prefix-map=/build/source/redist=./result/srcs/libsurvive/redist -ffile-prefix-map=/build/source/include/libsurvive=result/srcs/libsurvive/include/libsurvive";

});

rrSources = writeShellScriptBin "rr_sources" ''
  RR_LOG=all:debug ./result/bin/rr sources \
  --substitute=$(basename $(readlink ${openblas-dev}/lib/libopenblas.so))=$PWD/result/srcs/openblas/src \
  --substitute=$(basename $(readlink ${libsurvive-dev}/lib/libsurvive.so))=$PWD/result/srcs/libsurvive/src \
  ./rr/latest-trace \
  > sources.txt 2>&1
'';

pernoscoSubmit = writeShellScriptBin "pernosco_submit" ''
  PATH=${zstd}/bin:${awscli}/bin:./result/bin:$PATH ${python3}/bin/python3 ./submodules/pernosco-submit/pernosco-submit \
    -x \
  upload \
  --title $2 \
  --substitute=$(basename $(readlink ${openblas-dev}/lib/libopenblas.so))=$PWD/result/srcs/openblas/src \
  --substitute=$(basename $(readlink ${libsurvive-dev}/lib/libsurvive.so))=$PWD/result/srcs/libsurvive/src \
  $1 ./. \
  $PWD \
  $PWD/xrt \
  $PWD/external \
  $PWD/result/srcs \
  $PWD/result/srcs/openblas/src \
  ${openblas-dev} \
  $PWD/result/srcs/libsurvive/src \
  ${libsurvive-dev} \
  > pernosco.txt 2>&1
'';

in

stdenvDebug.mkDerivation rec {
  pname = "monado";
  version = "21.0.0";

  src = ./.;

  NIX_CFLAGS_COMPILE = ''-ffile-prefix-map=/build/monado/build/src/xrt/targets/service=.
  -ffile-prefix-map=/build/monado/src=./src
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/ipc=.
  -ffile-prefix-map=/build/monado/build/src/xrt/targets/common=.
  -ffile-prefix-map=/build/monado/build/src/xrt/state_trackers/gui=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/state_trackers/prober=.
  -ffile-prefix-map=/build/monado/build/src/xrt/compositor=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/compositor=.
  -ffile-prefix-map=/build/monado/build/src/xrt/targets/common=.
  -ffile-prefix-map=/build/monado/build/src/xrt/drivers=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/drivers=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary/bindings=.
  -ffile-prefix-map=/build/monado/build/src/xrt/auxiliary=.
  '';

  nativeBuildInputs = [
    cmake
    doxygen
    glslang
    pkg-config
    python3
  ];

  cmakeFlags = [
    "-DXRT_FEATURE_SERVICE=${if serviceSupport then "ON" else "OFF"}"
  ];

  buildInputs = [
    SDL2
    dbus
    eigen
    ffmpeg
    gst_all_1.gst-plugins-base
    gst_all_1.gstreamer
    hidapi
    libGL
    libXau
    libXdmcp
    libXrandr
    libjpeg
    libffi
    # librealsense.dev - see below
    libsurvive-dev
    libusb1
    libuv
    libuvc
    libv4l
    libxcb
    opencv4
    openhmd
    udev
    vulkan-headers
    vulkan-loader
    wayland
    wayland-protocols
    zlib
    openblas-dev
    libbsd
  ];

  # realsense is disabled, the build ends with the following error:
  #
  # CMake Error in src/xrt/drivers/CMakeLists.txt:
  # Imported target "realsense2::realsense2" includes non-existent path
  # "/nix/store/2v95aps14hj3jy4ryp86vl7yymv10mh0-librealsense-2.41.0/include"
  # in its INTERFACE_INCLUDE_DIRECTORIES.
  #
  # for some reason cmake is trying to use ${librealsense}/include
  # instead of ${librealsense.dev}/include as an include directory

  # Help openxr-loader find this runtime
  setupHook = writeText "setup-hook" ''
    export XDG_CONFIG_DIRS=@out@/etc/xdg''${XDG_CONFIG_DIRS:+:''${XDG_CONFIG_DIRS}}
  '';

  meta = with lib; {
    description = "Open source XR runtime";
    homepage = "https://monado.freedesktop.org/";
    license = licenses.boost;
    maintainers = with maintainers; [ expipiplus1 prusnak ];
    platforms = platforms.linux;
  };


  fixupPhase = ''
  mkdir -p $out/srcs
  cp -r ${openblas-dev.src} $out/srcs/openblas
  cp -r ${libsurvive-dev.src} $out/srcs/libsurvive

  ln -s ${rr}/bin/rr $out/bin/rr
  ln -s ${rrSources}/bin/rr_sources $out/bin/rr_sources
  ln -s ${pernoscoSubmit}/bin/pernosco_submit $out/bin/pernosco_submit

  echo "_RR_TRACE_DIR=./rr nixGLIntel ${rr}/bin/rr record -i SIGUSR1 ./result/bin/monado-service" >> $out/bin/rr_record
  chmod +x $out/bin/rr_record

  echo "_RR_TRACE_DIR=./rr ${rr}/bin/rr -M replay \"\$@\"" >> $out/bin/rr_replay
  chmod +x $out/bin/rr_replay
  '';
}
