#!/bin/sh
PID_FILE="/mnt/ext1/system/config/meleys-sleep-timer.pid"
LOG_FILE="/mnt/ext1/system/config/meleys-sleep-timer.log"

if [ -f "$PID_FILE" ]; then
  PID="$(cat "$PID_FILE" 2>/dev/null)"
  if [ -n "$PID" ]; then
    kill "$PID" 2>/dev/null
  fi
  rm -f "$PID_FILE"
fi

echo "$(date 2>/dev/null) sleep timer cancelled" >> "$LOG_FILE"
exit 0

