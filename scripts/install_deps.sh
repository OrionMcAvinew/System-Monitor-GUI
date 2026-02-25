#!/usr/bin/env bash
set -euo pipefail

APT_PACKAGES=(
  build-essential cmake ninja-build pkg-config
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
)

apt_update_default() {
  sudo apt-get update
}

apt_update_core_sources_only() {
  echo "[deps] Retrying apt update with core sources only (ignoring sourceparts) ..."
  sudo apt-get \
    -o Dir::Etc::sourceparts="-" \
    -o APT::Get::List-Cleanup="0" \
    update
}

apt_install_default() {
  sudo apt-get install -y "${APT_PACKAGES[@]}"
}

apt_install_core_sources_only() {
  sudo apt-get \
    -o Dir::Etc::sourceparts="-" \
    install -y "${APT_PACKAGES[@]}"
}

if command -v apt-get >/dev/null 2>&1; then
  echo "[deps] Detected apt-get (Debian/Ubuntu). Installing build dependencies..."

  if apt_update_default; then
    apt_install_default
  else
    echo "[deps] Warning: apt update failed (often caused by a bad third-party repo key)."
    apt_update_core_sources_only
    apt_install_core_sources_only
  fi

  echo "[deps] Done."
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
