#!/usr/bin/env python3
import csv
import io
import json
import sqlite3
import threading
import time
import urllib.request
import zipfile
from datetime import datetime, date
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from zoneinfo import ZoneInfo

from google.transit import gtfs_realtime_pb2

ROOT = Path(__file__).resolve().parent
DB = ROOT / "departures.sqlite3"
GTFS = ROOT / "gtfs.zip"
STATIC_URL = "https://kordis-jmk.cz/gtfs/gtfs.zip"
REALTIME_URL = "https://kordis-jmk.cz/gtfs/gtfsReal.dat"
TZ = ZoneInfo("Europe/Prague")
STOP = "U1483Z6"
NEXT_STOP = "U1262Z4"
ROUTES = {"L25D99": "25", "L26D99": "26"}
schedule = {}
schedule_day = None
lock = threading.Lock()


def db():
    connection = sqlite3.connect(DB)
    connection.execute("""CREATE TABLE IF NOT EXISTS departures(
        service_date TEXT NOT NULL, trip_id TEXT NOT NULL, line TEXT NOT NULL,
        destination TEXT NOT NULL, planned INTEGER NOT NULL, actual INTEGER,
        observed_at INTEGER, PRIMARY KEY(service_date, trip_id))""")
    return connection


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-punctuality/1.0"})
    with urllib.request.urlopen(request, timeout=35) as response:
        return response.read()


def csv_rows(archive, name):
    return csv.DictReader(io.TextIOWrapper(archive.open(name), encoding="utf-8-sig"))


def active_services(archive, day):
    key = day.strftime("%Y%m%d")
    weekday = day.strftime("%A").lower()
    active = {r["service_id"] for r in csv_rows(archive, "calendar.txt")
              if r[weekday] == "1" and r["start_date"] <= key <= r["end_date"]}
    for row in csv_rows(archive, "calendar_dates.txt"):
        if row["date"] != key:
            continue
        if row["exception_type"] == "1": active.add(row["service_id"])
        elif row["exception_type"] == "2": active.discard(row["service_id"])
    return active


def seconds(value):
    h, m, s = map(int, value.split(":"))
    return h * 3600 + m * 60 + s


def load_schedule(day):
    global schedule, schedule_day
    if not GTFS.exists() or time.time() - GTFS.stat().st_mtime > 20 * 3600:
        GTFS.write_bytes(fetch(STATIC_URL))
    with zipfile.ZipFile(GTFS) as archive:
        active = active_services(archive, day)
        trips = {r["trip_id"]: (ROUTES[r["route_id"]], r["trip_headsign"])
                 for r in csv_rows(archive, "trips.txt")
                 if r["route_id"] in ROUTES and r["service_id"] in active}
        found = {}
        for row in csv_rows(archive, "stop_times.txt"):
            if row["trip_id"] in trips and row["stop_id"] == STOP:
                planned = seconds(row["departure_time"])
                if 7 * 3600 <= planned <= 8 * 3600 + 15 * 60:
                    found[row["trip_id"]] = (*trips[row["trip_id"]], planned)
    with lock:
        schedule, schedule_day = found, day
    connection = db()
    connection.executemany("INSERT OR IGNORE INTO departures VALUES(?,?,?,?,?,NULL,NULL)",
        [(day.isoformat(), trip, line, destination, planned) for trip, (line, destination, planned) in found.items()])
    connection.commit(); connection.close()


def collect_once():
    now = datetime.now(TZ)
    if schedule_day != now.date(): load_schedule(now.date())
    feed = gtfs_realtime_pb2.FeedMessage()
    feed.ParseFromString(fetch(REALTIME_URL))
    updates = []
    with lock: current = dict(schedule)
    for entity in feed.entity:
        if not entity.HasField("vehicle"):
            continue
        vehicle = entity.vehicle
        trip = vehicle.trip.trip_id
        if trip not in current or vehicle.stop_id != NEXT_STOP:
            continue
        stamp = int(vehicle.timestamp or feed.header.timestamp or time.time())
        local = datetime.fromtimestamp(stamp, TZ)
        if local.date() == now.date(): updates.append((stamp, stamp, now.date().isoformat(), trip))
    if updates:
        connection = db()
        connection.executemany("UPDATE departures SET actual=COALESCE(actual,?), observed_at=? WHERE service_date=? AND trip_id=?", updates)
        connection.commit(); connection.close()


def collector():
    while True:
        try: collect_once()
        except Exception as exc: print(datetime.now(), "collector:", exc, flush=True)
        time.sleep(15)


def stats():
    connection = db(); connection.row_factory = sqlite3.Row
    rows = connection.execute("SELECT * FROM departures WHERE planned BETWEEN ? AND ? ORDER BY service_date DESC, planned DESC LIMIT 1000", (7 * 3600, 8 * 3600 + 15 * 60)).fetchall()
    connection.close()
    result = []
    for row in rows:
        item = dict(row)
        item["delay"] = item["actual"] - (datetime.fromisoformat(item["service_date"]).replace(tzinfo=TZ).timestamp() + item["planned"]) if item["actual"] else None
        result.append(item)
    return {"generated": int(time.time()), "records": result}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs): super().__init__(*args, directory=str(ROOT / "web"), **kwargs)
    def do_GET(self):
        if self.path.split("?", 1)[0] == "/api/stats":
            body = json.dumps(stats(), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        super().do_GET()
    def log_message(self, fmt, *args): pass


if __name__ == "__main__":
    load_schedule(datetime.now(TZ).date())
    threading.Thread(target=collector, daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", 8097), Handler).serve_forever()
