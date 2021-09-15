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
, gst_all_1 # gst-plugins-base
#, gstreamer
, hidapi
, libGL
, libXau
, libXdmcp
, libXrandr
, libffi
, libjpeg
# , librealsense
# , libsurvive
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
# , openblas
, rr
, writeShellScriptBin
, zstd
, awscli
, libbsd
}:
let

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

# openblas-dev = openblas.override { stdenv = stdenvDebug; };
openblas-dev = callPackage ./openblas.nix { stdenv = stdenvDebug; };
libsurvive = callPackage ./libsurvive.nix { stdenv = stdenvDebug; openblas = openblas-dev; };

rrSources = writeShellScriptBin "rr_sources" ''
  # :<>s/openblas/*/g
  RR_LOG=all:debug ./result/bin/rr sources \
  --substitute=libopenblas.so.0.1.0=$PWD/result/srcs/openblas/src \
  ./rr/latest-trace \
  > sources.txt 2>&1
'';

pernoscoSubmit = writeShellScriptBin "pernosco_submit" ''
  PATH=${zstd}/bin:${awscli}/bin:./result/bin:$PATH ${python3}/bin/python3 ./submodules/pernosco-submit/pernosco-submit \
    -x \
  upload \
  --title $2 \
  --substitute=libopenblas.so.0.1.0=$PWD/result/srcs/openblas/src \
  $1 ./. \
  $PWD/result/srcs \
  $PWD/result/srcs/openblas/src \
  ${openblas-dev} \
  > pernosco.txt 2>&1
'';

in

stdenvDebug.mkDerivation rec {
  pname = "monado";
  version = "21.0.0";

  # src = fetchFromGitLab {
  #   domain = "gitlab.freedesktop.org";
  #   owner = pname;
  #   repo = pname;
  #   rev = "v${version}";
  #   sha256 = "07zxs96i3prjqww1f68496cl2xxqaidx32lpfyy0pn5am4c297zc";
  # };

  src = ./.;

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
    libsurvive
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
  ln -s ${openblas-dev.src} $out/srcs/openblas

  ln -s ${rr}/bin/rr $out/bin/rr
  ln -s ${rrSources}/bin/rr_sources $out/bin/rr_sources
  ln -s ${pernoscoSubmit}/bin/pernosco_submit $out/bin/pernosco_submit


  echo "_RR_TRACE_DIR=./rr ${rr}/bin/rr record -i SIGUSR1 ./result/bin/monado-service" >> $out/bin/monado_rr_record
  chmod +x $out/bin/monado_rr_record

  echo "_RR_TRACE_DIR=./rr ${rr}/bin/rr -M replay \"\$@\"" >> $out/bin/monado_rr_replay
  chmod +x $out/bin/monado_rr_replay
  '';
}
