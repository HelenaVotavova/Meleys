#!/bin/sh
set -eu

package=$1
libs="$package/HelcinyPodcastyQt-libs"
plugins="$package/HelcinyPodcastyQt-plugins"
queue="$package/.runtime-queue"
seen="$package/.runtime-seen"
: >"$queue"
: >"$seen"
find "$libs" "$plugins" -type f -name '*.so*' -print >"$queue"

while IFS= read -r object; do
  grep -Fxq "$object" "$seen" && continue
  echo "$object" >>"$seen"
  readelf -d "$object" 2>/dev/null | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' |
  while IFS= read -r soname; do
    case "$soname" in
      ld-linux*|libc.so.*|libm.so.*|libpthread.so.*|librt.so.*|libdl.so.*|libgcc_s.so.*|libstdc++.so.*|libQt6Core.so.*|libQt6Gui.so.*|libQt6Qml.so.*|libQt6Quick.so.*|libQt6Network.so.*|libQt6DBus.so.*|libinkview.so) continue ;;
    esac
    [ -e "$libs/$soname" ] && continue
    source=$(find /usr/lib/arm-linux-gnueabi /lib/arm-linux-gnueabi -name "$soname" -type f -o -name "$soname" -type l 2>/dev/null | head -1)
    [ -n "$source" ] || continue
    cp -L "$source" "$libs/$soname"
    echo "$libs/$soname" >>"$queue"
  done
done <"$queue"

rm -f "$queue" "$seen"

