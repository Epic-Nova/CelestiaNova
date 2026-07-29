#!/usr/bin/env bash
# Bootstrap build prerequisites for a clean Debian/Ubuntu Linux host.
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  printf 'Run through sudo: sudo bash %s\n' "$0" >&2
  exit 1
fi
if ! command -v apt-get >/dev/null 2>&1; then
  printf 'This bootstrap currently supports Debian/Ubuntu hosts with apt-get.\n' >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build git pkg-config \
  ca-certificates curl zip unzip openssl libssl-dev \
  python3

printf 'Build requirements installed. Build with: BUILD_JOBS=1 bash Utilities/linux/build_and_package_linux.sh\n'
