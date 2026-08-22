#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p public/images
(while :; do ./update_catalog.py || true; sleep 21600; done) &
exec python3 -m http.server 8094 --bind 0.0.0.0 --directory public
