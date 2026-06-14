#!/usr/bin/env bash
#
# Build the pinned Warcraft Recorder OBS fork and vendor the macOS runtime.
#
# Output:
#   Frameworks/libobs.framework
#   Frameworks/libobs-opengl.dylib
#   Frameworks/obs-ffmpeg-mux
#   Frameworks/lib*.dylib                # ffmpeg + crypto runtime deps
#   PlugIns/{obs-x264,obs-ffmpeg,mac-capture,mac-videotoolbox,
#            image-source,obs-filters}.plugin
#   data/effects/                        # libobs effect files
#
# Prereqs (Homebrew): cmake, ninja, jq, ccache (optional). Xcode 15+
# with command-line tools accepted (`xcodebuild -license accept`).

set -euo pipefail

OBS_REPO="${OBS_REPO:-https://github.com/aza547/warcraft-recorder-obs-studio.git}"
OBS_REF="${OBS_REF:-6a41f9a5716cb86bc07fb71957849bc751d2fb2f}"
OBS_VERSION="${OBS_VERSION:-31.1.1}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
NOOBS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OBS_ROOT="${OBS_ROOT:-${NOOBS_ROOT}/../warcraft-recorder-obs-studio}"
OBS_BUILD="${OBS_ROOT}/build_macos"
DEPS_DIR="${OBS_ROOT}/.deps/obs-deps-2025-07-11-universal"

PLUGINS=(
  obs-x264
  obs-ffmpeg
  mac-capture
  mac-videotoolbox
  image-source
  obs-filters
)

# Runtime libraries loaded through @rpath from obs-deps.
RUNTIME_DYLIBS=(
  libavcodec
  libavdevice
  libavfilter
  libavformat
  libavutil
  libswresample
  libswscale
  librist
  libsrt
  libdatachannel
  libfreetype
  libmbedcrypto
  libmbedtls
  libmbedx509
  libx264
)

log() { printf '[build-libobs-mac] %s\n' "$*"; }

clone_or_update_obs() {
  if [[ ! -d "${OBS_ROOT}/.git" ]]; then
    log "Cloning ${OBS_REPO} into ${OBS_ROOT}"
    git clone --filter=blob:none --depth 1 --no-checkout "${OBS_REPO}" "${OBS_ROOT}"
  else
    log "Updating existing OBS checkout at ${OBS_ROOT}"
  fi

  log "Checking out OBS ref ${OBS_REF}"
  git -C "${OBS_ROOT}" fetch --depth 1 origin "${OBS_REF}"
  git -C "${OBS_ROOT}" checkout --detach FETCH_HEAD

  log "Initializing submodules"
  git -C "${OBS_ROOT}" submodule update --init --recursive --depth 1
}

configure_obs() {
  local cmake_args=(
    -S "${OBS_ROOT}"
    --preset macos
    -DOBS_VERSION_OVERRIDE="${OBS_VERSION}"
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}"
    -DENABLE_BROWSER=OFF
    -DENABLE_VLC=OFF
    -DENABLE_WEBRTC=OFF
    -DENABLE_AJA=OFF
    -DENABLE_DECKLINK=OFF
    -DENABLE_UI=OFF
    -DENABLE_WEBSOCKET=OFF
    -DENABLE_SCRIPTING=OFF
    -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
  )

  if [[ -n "${CMAKE_OSX_SYSROOT:-}" ]]; then
    cmake_args+=(-DCMAKE_OSX_SYSROOT="${CMAKE_OSX_SYSROOT}")
  elif [[ -n "${SDKROOT:-}" ]]; then
    cmake_args+=(-DCMAKE_OSX_SYSROOT="${SDKROOT}")
  fi

  log "Configuring CMake (preset macos, arm64, no UI/browser/webrtc)"
  cmake "${cmake_args[@]}"
}

build_target() {
  local target=$1
  log "Building target: ${target}"
  xcodebuild -project "${OBS_BUILD}/obs-studio.xcodeproj" \
    -target "${target}" \
    -configuration Release \
    ARCHS=arm64 \
    MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
    ONLY_ACTIVE_ARCH=NO \
    build > "${OBS_BUILD}/${target}.build.log" 2>&1 \
    || { log "build of ${target} failed; tail of log:"; tail -20 "${OBS_BUILD}/${target}.build.log"; exit 1; }
}

vendor_artifacts() {
  log "Vendoring artifacts into ${NOOBS_ROOT}"
  rm -rf "${NOOBS_ROOT}/Frameworks" "${NOOBS_ROOT}/PlugIns" "${NOOBS_ROOT}/data"
  mkdir -p "${NOOBS_ROOT}/Frameworks" "${NOOBS_ROOT}/PlugIns" "${NOOBS_ROOT}/data"

  cp -R "${OBS_BUILD}/libobs/Release/libobs.framework" \
        "${NOOBS_ROOT}/Frameworks/"
  cp "${OBS_BUILD}/libobs-opengl/Release/libobs-opengl.dylib" \
     "${NOOBS_ROOT}/Frameworks/"
  cp "${OBS_BUILD}/plugins/obs-ffmpeg/ffmpeg-mux/Release/obs-ffmpeg-mux" \
     "${NOOBS_ROOT}/Frameworks/"

  for p in "${PLUGINS[@]}"; do
    cp -R "${OBS_BUILD}/plugins/${p}/Release/${p}.plugin" \
          "${NOOBS_ROOT}/PlugIns/"
  done

  for lib in "${RUNTIME_DYLIBS[@]}"; do
    cp "${DEPS_DIR}/lib/${lib}.dylib" "${NOOBS_ROOT}/Frameworks/"
  done

  cp -R "${OBS_ROOT}/libobs/data" "${NOOBS_ROOT}/data/effects"

  log "Vendored:"
  ls "${NOOBS_ROOT}/Frameworks" | sed 's/^/  Frameworks\//'
  ls "${NOOBS_ROOT}/PlugIns"   | sed 's/^/  PlugIns\//'
}

main() {
  clone_or_update_obs
  configure_obs
  build_target libobs
  build_target libobs-opengl
  build_target obs-ffmpeg-mux
  for p in "${PLUGINS[@]}"; do build_target "${p}"; done
  vendor_artifacts
  log "Done. Run \`npm run build\` next to compile noobs.node and assemble dist/."
}

main "$@"
