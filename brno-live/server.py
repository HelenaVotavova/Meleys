#!/usr/bin/env python3
import json
import html
import math
import re
import time
import urllib.parse
import urllib.request
from http.cookiejar import CookieJar
from datetime import datetime, timedelta, timezone
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).parent
BRNO = (49.1951, 16.6068)
CACHE = {}
AVIATION_CACHE = {}
RADIATION_CACHE = {}


def fetch_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=15) as response:
        return json.load(response)


def sports_occupancy():
    request = urllib.request.Request("https://wwwbrno.cz/default.asp", headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=15) as response:
        page = response.read().decode("utf-8", "replace")
    wanted = ("Bazény Lužánky", "Bazény Lužánky – wellness", "Lázně Rašínova – bazén", "Lázně Rašínova – wellness")
    rows = {}
    for row in re.findall(r"<tr[^>]*>.*?</tr>", page, re.S):
        cells = re.findall(r"<td[^>]*>(.*?)</td>", row, re.S)
        values = [html.unescape(re.sub(r"<[^>]+>", " ", cell)).strip() for cell in cells]
        if values:
            for name in sorted(wanted, key=len, reverse=True):
                if values[0].startswith(name):
                    rows[name] = values
                    break
    result = []
    for name in wanted:
        values = rows.get(name)
        if not values:
            continue
        numbers = [int(value) for value in values[1:5]]
        result.append({"name": name, "capacity": numbers[0], "occupied": numbers[1],
                       "free": numbers[2], "visits_today": numbers[3]})
    if len(result) != 4:
        raise ValueError("Obsazenost STAREZ není kompletní")
    return result


def aircraft_and_flights():
    if AVIATION_CACHE.get("data") and time.time() - AVIATION_CACHE["at"] < 300:
        return AVIATION_CACHE["data"]
    states = fetch_json("https://opensky-network.org/api/states/all?lamin=48.975&lomin=16.05&lamax=49.525&lomax=17.15&extended=1")
    aircraft = []
    for state in states.get("states") or []:
        if state[5] is None or state[6] is None:
            continue
        aircraft.append({"icao": state[0], "callsign": (state[1] or "").strip() or state[0].upper(),
                         "country": state[2], "lon": state[5], "lat": state[6],
                         "altitude": state[7], "ground": state[8], "speed": state[9],
                         "heading": state[10], "vertical_rate": state[11],
                         "category": state[17] if len(state) > 17 else None})
    now = datetime.now(ZoneInfo("Europe/Prague"))
    flights = []
    schedule_available = True
    for direction in ("arrivals", "departures"):
        request = urllib.request.Request(f"https://www.brno-airport.cz/en/{direction}", headers={"User-Agent": "Meleys-Brno-Live/1.0"})
        with urllib.request.urlopen(request, timeout=15) as response:
            page = response.read().decode("utf-8", "replace")
        if "flight-table__table" not in page:
            schedule_available = False
            continue
        for row in re.findall(r"<tr>(.*?)</tr>", page, re.S):
            cells = re.findall(r"<td[^>]*>(.*?)</td>", row, re.S)
            values = [" ".join(html.unescape(re.sub(r"<[^>]+>", " ", cell)).split()) for cell in cells]
            if len(values) < 5 or not re.fullmatch(r"\d{2}:\d{2}", values[0]):
                continue
            day = now.date() + timedelta(days=1 if values[1] == "Tomorrow" else 0)
            scheduled = datetime.combine(day, datetime.strptime(values[0], "%H:%M").time(), now.tzinfo)
            if now <= scheduled <= now + timedelta(hours=48):
                flights.append({"direction": direction, "scheduled": scheduled.isoformat(),
                                "airline": values[2], "place": values[3], "flight": values[4],
                                "note": values[5] if len(values) > 5 else ""})
    data = {"aircraft": aircraft, "flights": sorted(flights, key=lambda item: item["scheduled"]),
            "schedule_available": schedule_available, "updated": int(time.time())}
    AVIATION_CACHE.update(data=data, at=time.time())
    return data


def radiation_data():
    if RADIATION_CACHE.get("data") and time.time() - RADIATION_CACHE["at"] < 900:
        return RADIATION_CACHE["data"]
    url = "https://sujb.gov.cz/aplikace/monras/tabulky/svz"
    opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(CookieJar()))
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    page = opener.open(request, timeout=15).read().decode("utf-8", "replace")
    token_match = re.search(r'name="_csrf" content="([^"]+)"', page)
    if not token_match:
        raise ValueError("SÚJB neposkytl bezpečnostní token")
    request = urllib.request.Request(url + ".json?zhp=false", data=b"draw=1", method="POST",
                                     headers={"User-Agent": "Meleys-Brno-Live/1.0",
                                              "X-CSRF-TOKEN": token_match.group(1),
                                              "X-Requested-With": "XMLHttpRequest",
                                              "Content-Type": "application/x-www-form-urlencoded"})
    payload = json.load(opener.open(request, timeout=15))
    stations = [{"station": row["mermisto"], "measured": row["datum"],
                 "average": row["hodnotaPrumer"], "maximum": row["hodnotaMax"],
                 "status": row["typMonitorovani"]}
                for row in payload.get("data", []) if row.get("mermisto") in ("Brno - Ponava", "Brno - Tuřany")]
    if not stations:
        raise ValueError("Aktuální radiační data pro Brno nejsou dostupná")
    data = {"stations": stations, "unit": "nSv/h", "source": "SÚJB MonRaS"}
    RADIATION_CACHE.update(data=data, at=time.time())
    return data


def daylight_series():
    today = datetime.now(ZoneInfo("Europe/Prague")).date()
    rows = []
    latitude = math.radians(BRNO[0])
    for offset in range(-61, 62):
        day = today + timedelta(days=offset)
        gamma = 2 * math.pi / 365 * (day.timetuple().tm_yday - 1)
        declination = (0.006918 - 0.399912 * math.cos(gamma) + 0.070257 * math.sin(gamma)
                       - 0.006758 * math.cos(2 * gamma) + 0.000907 * math.sin(2 * gamma)
                       - 0.002697 * math.cos(3 * gamma) + 0.00148 * math.sin(3 * gamma))
        zenith = math.radians(90.833)
        hour_angle = math.acos((math.cos(zenith) / (math.cos(latitude) * math.cos(declination)))
                               - math.tan(latitude) * math.tan(declination))
        rows.append({"date": day.isoformat(), "hours": 24 * hour_angle / math.pi})
    return rows


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
        "occupancy": sports_occupancy(),
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
        if self.path == "/api/radiation":
            try:
                body, status = json.dumps(radiation_data()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/aviation":
            try:
                body, status = json.dumps(aircraft_and_flights()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/daylight":
            body = json.dumps(daylight_series()).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "public, max-age=21600"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
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
