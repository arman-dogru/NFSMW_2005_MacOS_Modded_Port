#!/bin/bash
# Local source checkout launcher; the installed app/profile remain separate.
set -euo pipefail
sb_root="$(cd "$(dirname "$0")/../.." && pwd)"
if /usr/bin/pgrep -x SpeedBreaker >/dev/null; then
    echo "SpeedBreaker is already running. Use its open window, or quit it before launching again."
    exit 0
fi
export NFSMW_RACE_POLICE="${NFSMW_RACE_POLICE:-1}"
export NFSMW_POLICE_DENSITY="${NFSMW_POLICE_DENSITY:-3}"
export NFSMW_POLICE_START_DELAY="${NFSMW_POLICE_START_DELAY:-3}"
export NFSMW_FAIRCOPS="${NFSMW_FAIRCOPS:-1}"
export NFSMW_SB_MASS="${NFSMW_SB_MASS:-1000}"
# Live Game settings (unlimited Speedbreaker and all boss rewards, default On)
# keep saved choices. run_dev.sh executes build/main/runtime/SpeedBreaker.
# Verbose diagnostics are off unless explicitly supplied by the developer.
echo "Starting the modded game..."
exec /bin/bash "$sb_root/scripts/macos/run_dev.sh" "$@"
