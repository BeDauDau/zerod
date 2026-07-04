#!/usr/bin/env bash
set -euo pipefail

# ── Build & Flash for ESP32-C3 (Knomi) ──────────────
PROJECT_DIR="$(cd "$(dirname "$0")/src" && pwd)"
PLATFORMIO="$HOME/.platformio/penv/bin/platformio"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
ENV="knomi"
PORT="${PORT:-/dev/ttyACM0}"
CONNECT_ATTEMPTS=10

echo "======================================================"
echo " ZeroD – Build & Flash"
echo "======================================================"

# ── Step 1: Build ───────────────────────────────────
echo
echo "[1/2] Building firmware ($ENV) ..."
cd "$PROJECT_DIR"
"$PLATFORMIO" run -e "$ENV"

FW_BIN="$PROJECT_DIR/.pio/build/$ENV/firmware.bin"
if [[ ! -f "$FW_BIN" ]]; then
  echo "ERROR: firmware.bin not found at $FW_BIN"
  exit 1
fi

# ── Step 2: Flash ───────────────────────────────────
echo
echo "[2/2] Flashing to $PORT ..."
python3 "$ESPTOOL"               \
  --chip esp32c3                 \
  --port "$PORT"                 \
  --before usb_reset             \
  --after hard_reset             \
  --connect-attempts "$CONNECT_ATTEMPTS" \
  write_flash 0x0 "$FW_BIN"

echo
echo "======================================================"
echo " Build & Flash complete."
echo "======================================================"
