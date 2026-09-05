#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cd "$tmp"
for spec in 'SOKOBAN:sokoban' 'JOURNAL:reading-journal' 'WEATHER:aladin-weather' 'TIMER:sleep-timer' 'PODCAST:podcast-player'; do
    test_name=${spec%%:*}
    app=${spec#*:}
    cc -std=gnu99 -D_GNU_SOURCE -fsanitize=undefined -fno-sanitize-recover=all \
       -I "$root/pocketbook-tests" -DTEST_"$test_name" \
       -DAPP_SOURCE="\"$root/pocketbook-$app-native/src/main.c\"" \
       "$root/pocketbook-tests/regression.c" -lm -o "$tmp/test"
    printf '%s: ' "$app"
    "$tmp/test"
done
