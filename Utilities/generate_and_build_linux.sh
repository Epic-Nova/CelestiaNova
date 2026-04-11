#!/usr/bin/env bash
set -euo pipefail

# generate_and_build_linux.sh
# Usage: ./generate_and_build_linux.sh [--action generate|build|all] [--build-dir DIR]
#        [--generator GEN] [--config Debug|Release] [--force] [--install] [--install-prefix DIR]
# This script is intended for Linux. It will:
# - safely handle stale/permission-problem CMakeCache.txt (prompt or --force to auto-clean)
# - generate CMake project files
# - build the project

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ACTION="all"
BUILD_DIR="Intermediate/Build"
CONFIG="Release"
GENERATOR=""
FORCE=false
INSTALL=false
INSTALL_PREFIX=""
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 1)}
GENERATE_EXTENSION_CMAKE="ON"
RESOLVE_EXTENSION_DEPS="ON"
CMAKE_EXTRA_ARGS=""

usage(){
  cat <<EOF
Usage: $0 [options]
Options:
  --action <generate|build|all>   Action to perform (default: all)
  --build-dir <dir>               Build directory relative to repo (default: Intermediate/Build)
  --generator <cmake-generator>   CMake generator (default: Ninja if present, else Unix Makefiles)
  --config <Debug|Release>        Build configuration (default: Release)
  --force                         Auto-remove stale build dir when permission errors occur
  --install                       Run 'cmake --install' after build
  --install-prefix <dir>          Installation prefix (passed to cmake --install)
  --jobs <n>                      Parallel jobs for build (default: detected CPU count)
  --generate-extension-cmake <ON|OFF>
                                  Generate extension Source/CMakeLists.txt from JSON descriptors (default: ON)
  --resolve-extension-deps <ON|OFF>
                                  Enable/disable extension dependency autowiring (default: ON)
  --help                          Show this help
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --action) ACTION="$2"; shift 2;;
    --build-dir) BUILD_DIR="$2"; shift 2;;
    --generator) GENERATOR="$2"; shift 2;;
    --config) CONFIG="$2"; shift 2;;
    --force) FORCE=true; shift;;
    --install) INSTALL=true; shift;;
    --install-prefix) INSTALL_PREFIX="$2"; shift 2;;
    --jobs) JOBS="$2"; shift 2;;
    --generate-extension-cmake) GENERATE_EXTENSION_CMAKE="$2"; shift 2;;
    --resolve-extension-deps) RESOLVE_EXTENSION_DEPS="$2"; shift 2;;
    --help) usage; exit 0;;
    --*) CMAKE_EXTRA_ARGS+=" $1"; shift;;
    *) CMAKE_EXTRA_ARGS+=" $1"; shift;;
  esac
done

generate_extension_cmakelists(){
  if [ "$GENERATE_EXTENSION_CMAKE" != "ON" ]; then
    echo "Skipping extension CMakeLists generation (GENERATE_EXTENSION_CMAKE=$GENERATE_EXTENSION_CMAKE)"
    return
  fi

  local generator_script="$SCRIPT_DIR/generate_extension_cmakelists.cmake"
  if [ ! -f "$generator_script" ]; then
    echo "WARNING: extension CMake generator script not found: $generator_script"
    return
  fi

  echo "Generating extension Source/CMakeLists.txt files from JSON descriptors"
  cmake -DREPO_ROOT="$REPO_ROOT" -P "$generator_script"
}

# Choose default generator if not provided
if [ -z "$GENERATOR" ]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
  else
    GENERATOR="Unix Makefiles"
  fi
fi

REPO_BUILD_DIR="$REPO_ROOT/$BUILD_DIR"

# Safety: ensure build dir is inside repo root
case "$REPO_BUILD_DIR" in
  "$REPO_ROOT"/*) ;; 
  *) echo "ERROR: build directory ($REPO_BUILD_DIR) is outside repository root ($REPO_ROOT)"; exit 1;;
esac

check_and_prepare_build_dir(){
  if [ -f "$REPO_BUILD_DIR/CMakeCache.txt" ]; then
    if [ ! -r "$REPO_BUILD_DIR/CMakeCache.txt" ]; then
      echo "CMakeCache.txt exists but is not readable: $REPO_BUILD_DIR/CMakeCache.txt"
      if [ "$FORCE" = true ]; then
        echo "--force specified: removing build directory $REPO_BUILD_DIR"
        rm -rf "$REPO_BUILD_DIR"
        mkdir -p "$REPO_BUILD_DIR"
      else
        read -p "Remove and recreate the build directory? [y/N] " yn
        if [[ "$yn" =~ ^[Yy] ]]; then
          rm -rf "$REPO_BUILD_DIR"
          mkdir -p "$REPO_BUILD_DIR"
        else
          echo "Aborting due to unreadable CMakeCache.txt"; exit 1
        fi
      fi
    fi
  else
    mkdir -p "$REPO_BUILD_DIR"
  fi
}

generate(){
  generate_extension_cmakelists
  echo "Generating project files in: $REPO_BUILD_DIR"
  cmake -S "$REPO_ROOT" -B "$REPO_BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$CONFIG" -DNOVA_ENABLE_EXTENSION_DEPENDENCY_AUTOWIRE="$RESOLVE_EXTENSION_DEPS" $CMAKE_EXTRA_ARGS
}

build(){
  echo "Building in: $REPO_BUILD_DIR"
  if [[ "$GENERATOR" == Visual* || "$GENERATOR" == "Xcode" ]]; then
    cmake --build "$REPO_BUILD_DIR" --config "$CONFIG"
  else
    cmake --build "$REPO_BUILD_DIR" -- -j "$JOBS"
  fi
}

install_target(){
  if [ "$INSTALL" = true ]; then
    echo "Installing from: $REPO_BUILD_DIR"
    if [ -n "$INSTALL_PREFIX" ]; then
      cmake --install "$REPO_BUILD_DIR" --config "$CONFIG" --prefix "$INSTALL_PREFIX"
    else
      cmake --install "$REPO_BUILD_DIR" --config "$CONFIG"
    fi
  fi
}

# Main
check_and_prepare_build_dir
if [ "$ACTION" = "generate" ] || [ "$ACTION" = "all" ]; then
  generate
fi
if [ "$ACTION" = "build" ] || [ "$ACTION" = "all" ]; then
  build
  install_target
fi

echo "Done. Build dir: $REPO_BUILD_DIR"
