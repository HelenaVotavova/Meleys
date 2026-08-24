#!/bin/sh
set -u
package=$1
libs="$package/HelcinyAlsaWorker-libs"
queue="$package/.alsa-queue"; seen="$package/.alsa-seen"
: >"$queue"; : >"$seen"
find "$libs" -type f -name '*.so*' -print >"$queue"
while IFS= read -r object; do
  grep -Fxq "$object" "$seen" && continue
  echo "$object" >>"$seen"
  readelf -d "$object" 2>/dev/null | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' |
  while IFS= read -r soname; do
    case "$soname" in ld-linux*|libc.so.*|libm.so.*|libpthread.so.*|librt.so.*|libdl.so.*|libgcc_s.so.*) continue ;; esac
    [ -e "$libs/$soname" ] && continue
    source=$(find /usr/lib/arm-linux-gnueabi /lib/arm-linux-gnueabi \( -type f -o -type l \) -name "$soname" -print -quit 2>/dev/null)
    [ -n "$source" ] || continue
    cp -L "$source" "$libs/$soname"; echo "$libs/$soname" >>"$queue"
  done
done <"$queue"
rm -f "$queue" "$seen"
