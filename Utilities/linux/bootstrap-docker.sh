#!/usr/bin/env bash

# Root-owned, argumentless runtime bootstrap for the Celestia service account.
# It is installed only by install_service_mode.sh together with a sudoers rule
# that permits exactly this path. Do not add parameters or generic command
# execution here: DockerOrchestrator is the sole intended caller.
set -euo pipefail

if [[ "${EUID}" -ne 0 || "$#" -ne 0 ]]; then
    printf 'This helper must run as root without arguments.\n' >&2
    exit 64
fi

missing_packages=()
command -v docker >/dev/null 2>&1 || missing_packages+=(docker.io)
docker compose version >/dev/null 2>&1 || missing_packages+=(docker-compose-v2)
command -v composer >/dev/null 2>&1 || missing_packages+=(composer php-cli)

if [[ "${#missing_packages[@]}" -gt 0 ]]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y "${missing_packages[@]}"
fi

systemctl enable --now docker
usermod -aG docker celestianova

printf 'Celestia runtime prerequisites are ready (Docker, Docker Compose, Composer, PHP CLI). Restart celestianova.service before Compose actions so its group membership is refreshed.\n'
