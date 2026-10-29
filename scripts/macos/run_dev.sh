#!/usr/bin/env bash
# Run the local binary with separate settings, saves, logs and GPU caches.
# Game files are read through the existing game/files link; never copied here.
set -euo pipefail
sb_root="$(cd "$(dirname "$0")/../.." && pwd)"
sb_binary="$sb_root/build/main/runtime/SpeedBreaker"
if [ ! -x "$sb_binary" ]; then
    echo "Build SpeedBreaker first (see docs/NATIVE_MODS.md)." >&2
    exit 1
fi
export NFSMW_USER_DIR="${NFSMW_USER_DIR:-$sb_root/build/dev-profile/data}"
export NFSMW_CACHE_DIR="${NFSMW_CACHE_DIR:-$sb_root/build/dev-profile/cache}"
export NFSMW_GAME_DIR="${NFSMW_GAME_DIR:-$sb_root/game/files}"
export NFSMW_FULLSCREEN="${NFSMW_FULLSCREEN:-0}"
mkdir -p "$NFSMW_USER_DIR" "$NFSMW_CACHE_DIR"
cd "$sb_root"
exec "$sb_binary" "$@"
