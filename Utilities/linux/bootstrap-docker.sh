#!/usr/bin/env bash

# Root-owned, argumentless Docker bootstrap for the Celestia service account.
# It is installed only by install_service_mode.sh together with a sudoers rule
# that permits exactly this path. Do not add parameters or generic command
# execution here: DockerOrchestrator is the sole intended caller.
set -euo pipefail

if [[ "${EUID}" -ne 0 || "$#" -ne 0 ]]; then
    printf 'This helper must run as root without arguments.\n' >&2
    exit 64
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y docker.io docker-compose-v2
systemctl enable --now docker
usermod -aG docker celestianova

printf 'Docker runtime installed. Restart celestianova.service before Compose actions so its group membership is refreshed.\n'
