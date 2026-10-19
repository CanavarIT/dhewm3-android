#!/usr/bin/env bash
# Overlay Android-specific engine files from patches/ onto fetched deps
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
PATCH="$ROOT/patches"

if [ ! -d "$CPP/dhewm3/neo" ]; then
  echo "ERROR: dhewm3 not found. Run scripts/fetch_deps.sh first."
  exit 1
fi

echo "==> Applying Android patches onto dhewm3..."
# sys/android + events + glimp + posix if present
if [ -d "$PATCH/dhewm3/neo/sys" ]; then
  mkdir -p "$CPP/dhewm3/neo/sys/android"
  cp -a "$PATCH/dhewm3/neo/sys/android/." "$CPP/dhewm3/neo/sys/android/"
  [ -f "$PATCH/dhewm3/neo/sys/events.cpp" ] && cp -f "$PATCH/dhewm3/neo/sys/events.cpp" "$CPP/dhewm3/neo/sys/events.cpp"
  [ -f "$PATCH/dhewm3/neo/sys/glimp.cpp" ] && cp -f "$PATCH/dhewm3/neo/sys/glimp.cpp" "$CPP/dhewm3/neo/sys/glimp.cpp"
  if [ -d "$PATCH/dhewm3/neo/sys/posix" ]; then
    cp -a "$PATCH/dhewm3/neo/sys/posix/." "$CPP/dhewm3/neo/sys/posix/" 2>/dev/null || true
  fi
fi

if [ -f "$PATCH/dhewm3/neo/CMakeLists.txt" ]; then
  echo "==> Overlay dhewm3 neo/CMakeLists.txt"
  cp -f "$PATCH/dhewm3/neo/CMakeLists.txt" "$CPP/dhewm3/neo/CMakeLists.txt"
fi

echo "==> Done. Open the project in Android Studio / AndroidIDE and build."
