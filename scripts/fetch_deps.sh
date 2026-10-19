#!/usr/bin/env bash
# Clone third-party sources into app/src/main/cpp/
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
mkdir -p "$CPP"
cd "$CPP"

clone() {
  local url="$1" dir="$2" branch="${3:-}"
  if [ -d "$dir/.git" ] || [ -f "$dir/CMakeLists.txt" ]; then
    echo "==> $dir already present, skip"
    return
  fi
  echo "==> Cloning $url -> $dir"
  if [ -n "$branch" ]; then
    git clone --depth 1 --branch "$branch" "$url" "$dir"
  else
    git clone --depth 1 "$url" "$dir"
  fi
}

# dhewm3 engine
clone https://github.com/dhewm/dhewm3.git dhewm3

# SDL2 (2.x line)
clone https://github.com/libsdl-org/SDL.git SDL2 release-2.30.9

# GL4ES — desktop GL → GLES2 translator (this port relies on it)
clone https://github.com/ptitSeb/gl4es.git gl4es

# OpenAL Soft
clone https://github.com/kcat/openal-soft.git openal-soft

echo
echo "Deps ready under $CPP"
echo "Next: scripts/apply_patches.sh"
