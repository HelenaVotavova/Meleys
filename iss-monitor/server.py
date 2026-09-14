#!/usr/bin/env python3
import json
import io
import math
import sqlite3
import time
import urllib.request
from datetime import datetime, timedelta, timezone
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlencode, urlparse

from PIL import Image

ROOT = Path(__file__).parent
CACHE = {"at": 0, "data": None}
TRACK_CACHE = {"at": 0, "data": None}
CAMERA_CACHE = {}
DB = ROOT / "iss_history.db"


def camera_snapshot(latitude, longitude):
    date = (datetime.now(timezone.utc) - timedelta(days=1)).date().isoformat()
    key = (round(latitude, 1), round(longitude, 1), date)
    if key in CAMERA_CACHE:
        return CAMERA_CACHE[key]
    half_lat = 2.1
    half_lon = min(8, half_lat / max(0.25, math.cos(math.radians(latitude))))
    west, east = longitude - half_lon, longitude + half_lon
    if west < -180:
        west, east = -180, -180 + 2 * half_lon
    elif east > 180:
        west, east = 180 - 2 * half_lon, 180
    params = {
        "SERVICE": "WMS", "VERSION": "1.1.1", "REQUEST": "GetMap",
        "LAYERS": "MODIS_Terra_CorrectedReflectance_TrueColor", "STYLES": "",
        "SRS": "EPSG:4326", "BBOX": f"{west},{latitude-half_lat},{east},{latitude+half_lat}",
        "WIDTH": 640, "HEIGHT": 480, "FORMAT": "image/jpeg", "TIME": date,
    }
    url = "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?" + urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": "Meleys-ISS-Monitor/1.0"})
    with urllib.request.urlopen(req, timeout=20) as response:
        image_bytes = response.read()
    image = Image.open(io.BytesIO(image_bytes)).convert("RGB").resize((160, 120))
    counts = {"cloud": 0, "water": 0, "land": 0, "unknown": 0}
    for red, green, blue in image.getdata():
        brightest, darkest = max(red, green, blue), min(red, green, blue)
        if brightest < 18:
            counts["unknown"] += 1
        elif brightest > 150 and brightest - darkest < 48:
            counts["cloud"] += 1
        elif blue > 42 and blue > red * 1.12 and blue > green * 0.92:
            counts["water"] += 1
        else:
            counts["land"] += 1
    known = max(1, sum(counts.values()) - counts["unknown"])
    analysis = {name: round(value / known * 100) for name, value in counts.items() if name != "unknown"}
    analysis.update(date=date, latitude=latitude, longitude=longitude)
    result = (image_bytes, analysis)
    CAMERA_CACHE.clear()
    CAMERA_CACHE[key] = result
    return result


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


def get_past_track():
    now = time.time()
    if TRACK_CACHE["data"] and now - TRACK_CACHE["at"] < 3600:
        return TRACK_CACHE["data"]
    end = int(now) // 300 * 300
    stamps = list(range(end - 86400, end + 1, 300))
    result = []
    for offset in range(0, len(stamps), 10):
        query = ",".join(map(str, stamps[offset:offset + 10]))
        url = f"https://api.wheretheiss.at/v1/satellites/25544/positions?timestamps={query}&units=kilometers"
        req = urllib.request.Request(url, headers={"User-Agent": "Meleys-ISS-Monitor/1.0"})
        with urllib.request.urlopen(req, timeout=10) as response:
            result.extend(json.load(response))
    data = [{"latitude": p["latitude"], "longitude": p["longitude"], "timestamp": p["timestamp"]} for p in result]
    TRACK_CACHE.update(at=now, data=data)
    return data


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
        parsed = urlparse(self.path)
        if parsed.path in ("/api/camera-view", "/api/camera-analysis"):
            try:
                query = parse_qs(parsed.query)
                latitude = float(query["lat"][0])
                longitude = float(query["lon"][0])
                image, analysis = camera_snapshot(latitude, longitude)
                if parsed.path == "/api/camera-view":
                    body, content_type = image, "image/jpeg"
                else:
                    body, content_type = json.dumps(analysis).encode(), "application/json"
                status = 200
            except Exception as exc:
                body, content_type, status = json.dumps({"error": str(exc)}).encode(), "application/json", 503
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed.path == "/api/position":
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
        if self.path == "/api/past-track":
            try:
                body, status = json.dumps(get_past_track()).encode(), 200
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
