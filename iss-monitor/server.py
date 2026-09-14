#!/usr/bin/env python3
import json
import math
import time
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).parent
CACHE = {"at": 0, "data": None}


def fetch_position():
    now = time.time()
    if CACHE["data"] and now - CACHE["at"] < 4:
        return CACHE["data"]
    req = urllib.request.Request(
        "https://api.wheretheiss.at/v1/satellites/25544",
        headers={"User-Agent": "Meleys-ISS-Monitor/1.0"},
    )
    with urllib.request.urlopen(req, timeout=8) as response:
        raw = json.load(response)
    previous = CACHE["data"]
    heading = None
    if previous:
        lat1, lat2 = map(math.radians, (previous["latitude"], raw["latitude"]))
        delta_lon = math.radians(raw["longitude"] - previous["longitude"])
        y = math.sin(delta_lon) * math.cos(lat2)
        x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(delta_lon)
        heading = (math.degrees(math.atan2(y, x)) + 360) % 360
    data = {
        "latitude": raw["latitude"], "longitude": raw["longitude"],
        "altitude": raw["altitude"], "velocity": raw["velocity"],
        "visibility": raw.get("visibility", "unknown"),
        "timestamp": raw["timestamp"], "heading": heading,
    }
    CACHE.update(at=now, data=data)
    return data


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/position":
            try:
                payload, status = fetch_position(), 200
            except Exception as exc:
                payload, status = {"error": str(exc), "cached": CACHE["data"]}, 503
            body = json.dumps(payload).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    import os
    os.chdir(ROOT / "web")
    ThreadingHTTPServer(("0.0.0.0", 8098), Handler).serve_forever()
