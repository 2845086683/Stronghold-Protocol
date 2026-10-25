#!/bin/sh
# Explicit opt-in only; official signed apt repository, never curl | sh.
set -eu
if [ "${1:-}" != '--install' ]; then
    printf '%s\n' 'Usage: sudo sh deploy/server/Install-Docker-Ubuntu.sh --install' \
      'Ubuntu only. Installs Docker Engine + Compose from Docker official apt repository.' \
      'It changes package sources and starts the Docker daemon. It does not edit firewall rules.' \
      'Review https://docs.docker.com/engine/install/ubuntu/ before opting in.'
    exit 0
fi
if [ "$(id -u)" -ne 0 ]; then printf '%s\n' 'Root privileges required.' >&2; exit 1; fi
. /etc/os-release
if [ "$ID" != ubuntu ]; then printf '%s\n' 'Only official Ubuntu is supported by this helper; use the vendor guide for your distribution.' >&2; exit 1; fi
case "${VERSION_CODENAME:-}" in jammy|noble|resolute|stonking) ;; *) printf '%s\n' 'Unsupported Ubuntu release.' >&2; exit 1;; esac
for package in docker.io docker-compose docker-compose-v2 docker-doc podman-docker containerd runc; do
    if dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'install ok installed'; then
        printf '%s\n' "Conflicting existing package: $package. Review official migration steps; no packages were removed." >&2; exit 1
    fi
done
if [ -e /etc/apt/sources.list.d/docker.sources ] || [ -e /etc/apt/sources.list.d/docker.list ]; then
    printf '%s\n' 'Existing Docker source configuration found; use apt upgrade after reviewing it. No files overwritten.' >&2; exit 1
fi
apt-get update
apt-get install -y ca-certificates curl gnupg python3
install -m 0755 -d /etc/apt/keyrings
curl --proto '=https' --tlsv1.2 -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
gpg --batch --show-keys /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc
architecture=$(dpkg --print-architecture)
cat > /etc/apt/sources.list.d/docker.sources <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $VERSION_CODENAME
Components: stable
Architectures: $architecture
Signed-By: /etc/apt/keyrings/docker.asc
EOF
apt-get update
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
docker compose version
printf '%s\n' 'Dependencies installed. Use server.sh init with digest-pinned official images, then server.sh up.'
