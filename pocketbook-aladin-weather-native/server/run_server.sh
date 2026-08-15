#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p public
(while :; do ./update_forecast.py --output public/weather.dat || true; sleep 3600; done) &
exec python3 -m http.server 8093 --bind 0.0.0.0 --directory public
