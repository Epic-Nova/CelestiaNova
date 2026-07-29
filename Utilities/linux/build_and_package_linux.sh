#!/usr/bin/env bash

# Build a relocatable Celestia Nova Linux runtime package.  The package is
# intentionally source-free: it contains the host binary, NovaCore, extension
# libraries, descriptors, Content, and systemd assets needed by service mode.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_CONFIGURATION="${BUILD_CONFIGURATION:-Production}"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/Intermediate/linux-ninja-${BUILD_TYPE,,}}"
PACKAGE_ROOT="${PACKAGE_ROOT:-${REPO_ROOT}/Artifacts/CelestiaNova-Linux-${BUILD_CONFIGURATION}}"
# CanvasCore is intentionally a large translation unit. Small VMs can run out
# of memory when Ninja uses every CPU, so packaging defaults to two workers.
# Override with BUILD_JOBS=1 (or a larger verified value) when appropriate.
BUILD_JOBS="${BUILD_JOBS:-2}"

case "${PACKAGE_ROOT}" in
    "${REPO_ROOT}"/Artifacts/*) ;;
    *)
        printf 'PACKAGE_ROOT must stay inside %s/Artifacts: %s\n' "${REPO_ROOT}" "${PACKAGE_ROOT}" >&2
        exit 1
        ;;
esac

for required_command in cmake c++; do
    if ! command -v "${required_command}" >/dev/null 2>&1; then
        printf 'Missing required build command: %s\n' "${required_command}" >&2
        exit 1
    fi
done

if command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Ninja"
elif command -v make >/dev/null 2>&1; then
    CMAKE_GENERATOR="Unix Makefiles"
else
    printf 'Missing build executor: install Ninja or GNU Make.\n' >&2
    exit 1
fi

cmake \
    -S "${REPO_ROOT}" \
    -B "${BUILD_DIR}" \
    -G "${CMAKE_GENERATOR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_CONFIGURATION="${BUILD_CONFIGURATION}" \
    -DBUILD_OUTPUT_DIR="${REPO_ROOT}/Binaries/Linux-${BUILD_CONFIGURATION}"

cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"
rm -rf "${PACKAGE_ROOT}"
cmake --install "${BUILD_DIR}" --prefix "${PACKAGE_ROOT}"

# Extension descriptors deliberately use stable library names such as
# `libCanvasCore.so`. Production builds add a configuration suffix so both
# Development and Production artifacts can live next to each other. Create a
# stable alias in the packaged runtime for every declared Linux extension
# library. This keeps descriptor resolution deterministic and avoids relying
# on fuzzy filename matching at daemon startup.
missing_descriptor_libraries=0
while IFS= read -r descriptor; do
    declared_library="$(sed -nE 's/^[[:space:]]*"file"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/p' "${descriptor}" | head -n 1)"
    case "${declared_library}" in
        *.so) ;;
        *) continue ;;
    esac

    if find "${PACKAGE_ROOT}/Binaries" -type f -name "${declared_library}" -print -quit | grep -q .; then
        continue
    fi

    library_stem="${declared_library%.so}"
    built_library="$(find "${PACKAGE_ROOT}/Binaries" -type f -name "${library_stem}-${BUILD_CONFIGURATION}.so" -print -quit)"
    if [[ -z "${built_library}" ]]; then
        printf 'Missing packaged extension library %s declared by %s\n' "${declared_library}" "${descriptor}" >&2
        missing_descriptor_libraries=1
        continue
    fi

    ln -sfn "$(basename "${built_library}")" "$(dirname "${built_library}")/${declared_library}"
done < <(find "${PACKAGE_ROOT}/Extensions" -type f -name '*.json' -print)

if [[ "${missing_descriptor_libraries}" -ne 0 ]]; then
    printf 'Celestia Nova package validation failed: one or more extension libraries are missing.\n' >&2
    exit 1
fi

printf 'Celestia Nova Linux package created at: %s\n' "${PACKAGE_ROOT}"
