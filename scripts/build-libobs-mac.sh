#!/usr/bin/env bash
#
# Build vanilla libobs.framework + the Mac plugin set we need from
# obsproject/obs-studio source, then vendor everything into this repo's
# Frameworks/, PlugIns/, and data/ directories. Replaces the Phase 1
# spike that linked against the Streamlabs OSN tarball.
#
# Run from the noobs repo root. Expects an obs-studio checkout at
# ../obs-studio (same parent dir as this repo). If that doesn't exist,
# the script clones it at the pinned tag.
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

OBS_TAG="${OBS_TAG:-31.0.3}"
NOOBS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OBS_ROOT="${OBS_ROOT:-${NOOBS_ROOT}/../obs-studio}"
OBS_BUILD="${OBS_ROOT}/build_macos"
DEPS_DIR="${OBS_ROOT}/.deps/obs-deps-2024-09-12-universal"

PLUGINS=(
  obs-x264
  obs-ffmpeg
  mac-capture
  mac-videotoolbox
  image-source
  obs-filters
)

# ffmpeg + crypto runtime libs that libobs and obs-ffmpeg's plugin
# binary link against via @rpath/<name>.dylib. Sourced from obs-deps.
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
  if [[ ! -d "${OBS_ROOT}" ]]; then
    log "Cloning obsproject/obs-studio at ${OBS_TAG} into ${OBS_ROOT}"
    git clone --depth 1 --branch "${OBS_TAG}" \
      https://github.com/obsproject/obs-studio.git "${OBS_ROOT}"
  fi
  log "Initializing submodules"
  git -C "${OBS_ROOT}" submodule update --init --recursive --depth 1
}

configure_obs() {
  log "Configuring CMake (preset macos, no UI/browser/webrtc)"
  cmake -S "${OBS_ROOT}" --preset macos \
    -DENABLE_BROWSER=OFF \
    -DENABLE_VLC=OFF \
    -DENABLE_WEBRTC=OFF \
    -DENABLE_AJA=OFF \
    -DENABLE_DECKLINK=OFF \
    -DENABLE_UI=OFF \
    -DENABLE_WEBSOCKET=OFF \
    -DENABLE_SCRIPTING=OFF \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
}

build_target() {
  local target=$1
  log "Building target: ${target}"
  xcodebuild -project "${OBS_BUILD}/obs-studio.xcodeproj" \
    -target "${target}" \
    -configuration Release \
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
  log "Done. Run \`npx node-gyp rebuild\` next, then \`node test/mac-init.js\`."
}

main "$@"
