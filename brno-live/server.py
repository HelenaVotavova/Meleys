#!/usr/bin/env python3
import json
import time
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).parent
BRNO = (49.1951, 16.6068)
CACHE = {}


def fetch_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=15) as response:
        return json.load(response)


def dashboard_data():
    if CACHE.get("data") and time.time() - CACHE["at"] < 300:
        return CACHE["data"]
    lat, lon = BRNO
    weather_query = urllib.parse.urlencode({
        "latitude": lat, "longitude": lon, "timezone": "Europe/Prague",
        "forecast_days": 2,
        "current": "temperature_2m,apparent_temperature,precipitation,weather_code,cloud_cover,wind_speed_10m,wind_gusts_10m,wind_direction_10m",
        "hourly": "temperature_2m,precipitation_probability,precipitation,cloud_cover,wind_speed_10m,wind_gusts_10m",
        "daily": "sunrise,sunset,daylight_duration,precipitation_sum,temperature_2m_max,temperature_2m_min",
    })
    air_query = urllib.parse.urlencode({
        "latitude": lat, "longitude": lon, "timezone": "Europe/Prague",
        "current": "european_aqi,pm10,pm2_5,nitrogen_dioxide,ozone",
    })
    data = {
        "weather": fetch_json("https://api.open-meteo.com/v1/forecast?" + weather_query),
        "air": fetch_json("https://air-quality-api.open-meteo.com/v1/air-quality?" + air_query),
        "updated": int(time.time()),
    }
    CACHE.update(data=data, at=time.time())
    return data


def satellite_image():
    date = (datetime.now(timezone.utc) - timedelta(days=1)).date().isoformat()
    params = {
        "SERVICE": "WMS", "VERSION": "1.1.1", "REQUEST": "GetMap",
        "LAYERS": "MODIS_Terra_CorrectedReflectance_TrueColor", "STYLES": "",
        "SRS": "EPSG:4326", "BBOX": "13.8,47.6,19.4,50.8",
        "WIDTH": 1400, "HEIGHT": 800, "FORMAT": "image/jpeg", "TIME": date,
    }
    url = "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?" + urllib.parse.urlencode(params)
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=25) as response:
        return response.read(), date


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/data":
            try:
                body, status = json.dumps(dashboard_data()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/satellite":
            try:
                body, date = satellite_image()
                self.send_response(200)
                self.send_header("Content-Type", "image/jpeg")
                self.send_header("X-Imagery-Date", date)
                self.send_header("Cache-Control", "public, max-age=1800")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers(); self.wfile.write(body)
            except Exception as exc:
                self.send_error(503, str(exc))
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    import os
    os.chdir(ROOT / "web")
    ThreadingHTTPServer(("0.0.0.0", 8100), Handler).serve_forever()
