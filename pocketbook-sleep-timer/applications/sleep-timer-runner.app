#!/bin/sh

MINUTES="$1"
STATE_DIR="/mnt/ext1/system/config"
PID_FILE="$STATE_DIR/meleys-sleep-timer.pid"
LOG_FILE="$STATE_DIR/meleys-sleep-timer.log"

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

(
  echo "$(date 2>/dev/null) sleep timer started: ${MINUTES} minutes" >> "$LOG_FILE"
  echo "$$" > "$PID_FILE"
  sleep "$((MINUTES * 60))"
  rm -f "$PID_FILE"
  echo "$(date 2>/dev/null) sleep timer expired, powering off" >> "$LOG_FILE"

  /sbin/poweroff >/dev/null 2>&1 && exit 0
  /bin/poweroff >/dev/null 2>&1 && exit 0
  poweroff >/dev/null 2>&1 && exit 0
  /sbin/shutdown -h now >/dev/null 2>&1 && exit 0
  shutdown -h now >/dev/null 2>&1 && exit 0

  echo "$(date 2>/dev/null) poweroff failed" >> "$LOG_FILE"
) &

exit 0

