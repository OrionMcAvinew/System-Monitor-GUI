#!/usr/bin/env bash
set -euo pipefail

APT_PACKAGES=(
  build-essential cmake ninja-build pkg-config
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
)

apt_update_default() {
  sudo apt-get update
}

pick_core_sourcelist() {
  if [[ -f /etc/apt/sources.list ]]; then
    echo /etc/apt/sources.list
    return 0
  fi

  # Ubuntu 24+ often uses deb822 in sources.list.d
  if [[ -f /etc/apt/sources.list.d/ubuntu.sources ]]; then
    echo /etc/apt/sources.list.d/ubuntu.sources
    return 0
  fi

  # Last resort: first ubuntu-ish source file
  local candidate
  candidate="$(find /etc/apt/sources.list.d -maxdepth 1 -type f \( -name '*ubuntu*' -o -name '*debian*' -o -name '*.sources' \) | head -n1 || true)"
  if [[ -n "$candidate" ]]; then
    echo "$candidate"
    return 0
  fi

  return 1
}

apt_update_core_sources_only() {
  local sourcelist
  sourcelist="$(pick_core_sourcelist)"
  if [[ -z "$sourcelist" ]]; then
    echo "[deps] Could not find a core apt source list for fallback."
    return 1
  fi

  echo "[deps] Retrying apt update using core source list: $sourcelist"
  sudo apt-get \
    -o Dir::Etc::sourcelist="$sourcelist" \
    -o Dir::Etc::sourceparts="-" \
    -o APT::Get::List-Cleanup="0" \
    update
}

apt_install_default() {
  sudo apt-get install -y "${APT_PACKAGES[@]}"
}

apt_install_core_sources_only() {
  local sourcelist
  sourcelist="$(pick_core_sourcelist)"
  if [[ -z "$sourcelist" ]]; then
    echo "[deps] Could not find a core apt source list for install fallback."
    return 1
  fi

  echo "[deps] Installing packages via core source list: $sourcelist"
  sudo apt-get \
    -o Dir::Etc::sourcelist="$sourcelist" \
    -o Dir::Etc::sourceparts="-" \
    install -y "${APT_PACKAGES[@]}"
}

if command -v apt-get >/dev/null 2>&1; then
  echo "[deps] Detected apt-get (Debian/Ubuntu). Installing build dependencies..."

  if apt_update_default && apt_install_default; then
    echo "[deps] Done."
    exit 0
  fi

  echo "[deps] Warning: default apt path failed (often third-party repo/key issues)."
  apt_update_core_sources_only
  apt_install_core_sources_only
  echo "[deps] Done (fallback mode)."
  exit 0
fi

if command -v brew >/dev/null 2>&1; then
  echo "[deps] Detected Homebrew (macOS). Installing build dependencies..."
  brew update
  brew install cmake ninja pkg-config glfw
  echo "[deps] Done."
  exit 0
fi

cat <<MSG
[deps] Unsupported package manager in this script.
Please install manually:
- C++17 compiler
- cmake, ninja, pkg-config
- OpenGL development libraries
- GLFW prerequisites for your platform
MSG
