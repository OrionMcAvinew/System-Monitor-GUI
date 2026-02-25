#!/usr/bin/env bash
set -euo pipefail

if command -v apt-get >/dev/null 2>&1; then
  echo "[deps] Detected apt-get (Debian/Ubuntu). Installing build dependencies..."
  sudo apt-get update
  sudo apt-get install -y \
    build-essential cmake ninja-build pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
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
