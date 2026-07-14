#!/bin/sh

STATE_DIR="/mnt/ext1/system/config"
LOG_FILE="$STATE_DIR/meleys-sleep-timer-debug.log"

mkdir -p "$STATE_DIR" 2>/dev/null

{
  echo "=== Meleys Sleep Timer debug ==="
  date 2>/dev/null
  echo "whoami/id:"
  whoami 2>/dev/null
  id 2>/dev/null
  echo "pwd: $(pwd 2>/dev/null)"
  echo "script: $0"
  echo "uname:"
  uname -a 2>/dev/null
  echo "PATH: $PATH"
  echo "commands:"
  for c in sh setsid nohup sleep poweroff shutdown halt reboot busybox killall mplayer; do
    echo "$c -> $(command -v "$c" 2>/dev/null)"
  done
  echo "mount:"
  mount 2>/dev/null | head -20
  echo "state dir:"
  ls -ld "$STATE_DIR" 2>/dev/null
  echo "applications dir:"
  ls -l "$(dirname "$0")" 2>/dev/null
  echo "existing timer log:"
  tail -40 "$STATE_DIR/meleys-sleep-timer.log" 2>/dev/null
  echo "=== end ==="
} >> "$LOG_FILE" 2>&1

exit 0

