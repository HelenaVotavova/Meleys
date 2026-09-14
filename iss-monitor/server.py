#!/usr/bin/env python3
import json
import math
import sqlite3
import time
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).parent
CACHE = {"at": 0, "data": None}
DB = ROOT / "iss_history.db"


def save_measurement(data):
    with sqlite3.connect(DB) as db:
        db.execute("CREATE TABLE IF NOT EXISTS measurements (minute INTEGER PRIMARY KEY, altitude REAL, velocity REAL)")
        minute = data["timestamp"] // 60 * 60
        db.execute("INSERT OR REPLACE INTO measurements VALUES (?, ?, ?)", (minute, data["altitude"], data["velocity"]))
        db.execute("DELETE FROM measurements WHERE minute < ?", (time.time() - 30 * 86400,))


def get_history():
    with sqlite3.connect(DB) as db:
        db.execute("CREATE TABLE IF NOT EXISTS measurements (minute INTEGER PRIMARY KEY, altitude REAL, velocity REAL)")
        rows = db.execute("SELECT minute, altitude, velocity FROM measurements ORDER BY minute").fetchall()
    return [{"timestamp": r[0], "altitude": r[1], "velocity": r[2]} for r in rows]


def get_forecast():
    start = int(time.time()) // 60 * 60
    stamps = [start + step * 300 for step in range(13)]
    result = []
    for offset in range(0, len(stamps), 10):
        query = ",".join(map(str, stamps[offset:offset + 10]))
        url = f"https://api.wheretheiss.at/v1/satellites/25544/positions?timestamps={query}&units=kilometers"
        req = urllib.request.Request(url, headers={"User-Agent": "Meleys-ISS-Monitor/1.0"})
        with urllib.request.urlopen(req, timeout=8) as response:
            result.extend(json.load(response))
    return [{"latitude": p["latitude"], "longitude": p["longitude"], "timestamp": p["timestamp"]} for p in result]


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
    save_measurement(data)
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
        if self.path == "/api/history":
            body = json.dumps(get_history()).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/forecast":
            try:
                body, status = json.dumps(get_forecast()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body); return
        super().do_GET()

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    import os
    os.chdir(ROOT / "web")
    ThreadingHTTPServer(("0.0.0.0", 8098), Handler).serve_forever()
