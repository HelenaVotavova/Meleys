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

echo "$(date 2>/dev/null) worker started: $$, ${MINUTES} minutes" >> "$LOG_FILE"
sleep "$((MINUTES * 60))"
rm -f "$PID_FILE"
echo "$(date 2>/dev/null) worker expired, trying audio stop and poweroff" >> "$LOG_FILE"

killall mplayer >/dev/null 2>&1
killall audio >/dev/null 2>&1
killall player >/dev/null 2>&1

/sbin/poweroff >/dev/null 2>&1 && exit 0
/bin/poweroff >/dev/null 2>&1 && exit 0
poweroff >/dev/null 2>&1 && exit 0
/sbin/poweroff -f >/dev/null 2>&1 && exit 0
/bin/poweroff -f >/dev/null 2>&1 && exit 0
poweroff -f >/dev/null 2>&1 && exit 0
/sbin/shutdown -h now >/dev/null 2>&1 && exit 0
shutdown -h now >/dev/null 2>&1 && exit 0
/sbin/halt >/dev/null 2>&1 && exit 0
/bin/halt >/dev/null 2>&1 && exit 0
halt >/dev/null 2>&1 && exit 0
busybox poweroff >/dev/null 2>&1 && exit 0
busybox halt >/dev/null 2>&1 && exit 0
reboot -p >/dev/null 2>&1 && exit 0

echo "$(date 2>/dev/null) all poweroff commands failed" >> "$LOG_FILE"
exit 1
