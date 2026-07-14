#!/bin/sh

MINUTES="$1"
STATE_DIR="/mnt/ext1/system/config"
PID_FILE="$STATE_DIR/meleys-sleep-timer.pid"
LOG_FILE="$STATE_DIR/meleys-sleep-timer.log"
APP_DIR="$(dirname "$0")"
WORKER="$APP_DIR/sleep-timer-worker.app"

case "$MINUTES" in
  ''|*[!0-9]*)
    MINUTES=30
    ;;
esac

mkdir -p "$STATE_DIR" 2>/dev/null

if [ -f "$PID_FILE" ]; then
  OLD_PID="$(cat "$PID_FILE" 2>/dev/null)"
  if [ -n "$OLD_PID" ]; then
    kill "$OLD_PID" 2>/dev/null
  fi
fi

echo "$(date 2>/dev/null) sleep timer requested: ${MINUTES} minutes" >> "$LOG_FILE"

if command -v setsid >/dev/null 2>&1; then
  setsid "$WORKER" "$MINUTES" >/dev/null 2>&1 &
elif command -v nohup >/dev/null 2>&1; then
  nohup "$WORKER" "$MINUTES" >/dev/null 2>&1 &
else
  "$WORKER" "$MINUTES" >/dev/null 2>&1 &
fi
echo "$!" > "$PID_FILE"
echo "$(date 2>/dev/null) sleep timer background pid: $!" >> "$LOG_FILE"

exit 0
