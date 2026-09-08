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
NEXT_STOP = "U01262Z04"
ROUTES = {"L25D99": "25", "L26D99": "26"}
schedule = {}
journey_schedule = {}
schedule_day = None
lock = threading.Lock()


def db():
    connection = sqlite3.connect(DB)
    connection.execute("""CREATE TABLE IF NOT EXISTS departures(
        service_date TEXT NOT NULL, trip_id TEXT NOT NULL, line TEXT NOT NULL,
        destination TEXT NOT NULL, planned INTEGER NOT NULL, actual INTEGER,
        observed_at INTEGER, estimated INTEGER NOT NULL DEFAULT 0,
        PRIMARY KEY(service_date, trip_id))""")
    if "estimated" not in {row[1] for row in connection.execute("PRAGMA table_info(departures)")}:
        connection.execute("ALTER TABLE departures ADD COLUMN estimated INTEGER NOT NULL DEFAULT 0")
    connection.execute("""CREATE TABLE IF NOT EXISTS journey_legs(
        service_date TEXT NOT NULL, trip_id TEXT NOT NULL, leg TEXT NOT NULL,
        line TEXT NOT NULL, destination TEXT NOT NULL,
        origin_planned INTEGER NOT NULL, destination_planned INTEGER NOT NULL,
        origin_actual INTEGER, destination_actual INTEGER,
        PRIMARY KEY(service_date, trip_id, leg))""")
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
    global schedule, journey_schedule, schedule_day
    if not GTFS.exists() or time.time() - GTFS.stat().st_mtime > 20 * 3600:
        GTFS.write_bytes(fetch(STATIC_URL))
    with zipfile.ZipFile(GTFS) as archive:
        active = active_services(archive, day)
        route_names = {r["route_id"]: r["route_short_name"] for r in csv_rows(archive, "routes.txt")}
        trips = {r["trip_id"]: (route_names.get(r["route_id"], ""), r["trip_headsign"])
                 for r in csv_rows(archive, "trips.txt") if r["service_id"] in active}
        found = {}
        stop_times = {}
        for row in csv_rows(archive, "stop_times.txt"):
            if row["trip_id"] in trips:
                stop_times.setdefault(row["trip_id"], []).append(row)
            if row["trip_id"] in trips and trips[row["trip_id"]][0] in {"25", "26"} and row["stop_id"] == STOP:
                planned = seconds(row["departure_time"])
                if 7 * 3600 <= planned <= 8 * 3600 + 15 * 60:
                    found[row["trip_id"]] = (*trips[row["trip_id"]], planned)
        legs = {}
        for trip, rows in stop_times.items():
            ids = [r["stop_id"] for r in rows]
            line, destination = trips[trip]
            if line == "1" and "U1272Z2" in ids and "U1483Z2" in ids:
                oi, di = ids.index("U1272Z2"), ids.index("U1483Z2")
                origin = seconds(rows[oi]["departure_time"])
                if oi < di and 6 * 3600 + 45 * 60 <= origin <= 7 * 3600 + 45 * 60:
                    legs[(trip, "tram")] = (line, destination, origin, seconds(rows[di]["arrival_time"]), "U01167Z02", "U01483Z02")
            if line in {"25", "26"} and "U1483Z6" in ids and "U1055Z2" in ids:
                oi, di = ids.index("U1483Z6"), ids.index("U1055Z2")
                origin = seconds(rows[oi]["departure_time"])
                arrival = seconds(rows[di]["arrival_time"])
                if oi < di and 7 * 3600 <= origin <= 8 * 3600 + 15 * 60:
                    legs[(trip, "trolley")] = (line, destination, origin, arrival, NEXT_STOP, "U01055Z02")
    with lock:
        schedule, journey_schedule, schedule_day = found, legs, day
    connection = db()
    connection.executemany("INSERT OR IGNORE INTO departures(service_date,trip_id,line,destination,planned,actual,observed_at) VALUES(?,?,?,?,?,NULL,NULL)",
        [(day.isoformat(), trip, line, destination, planned) for trip, (line, destination, planned) in found.items()])
    connection.executemany("INSERT OR IGNORE INTO journey_legs(service_date,trip_id,leg,line,destination,origin_planned,destination_planned) VALUES(?,?,?,?,?,?,?)",
        [(day.isoformat(), trip, leg, values[0], values[1], values[2], values[3]) for (trip, leg), values in legs.items()])
    connection.commit(); connection.close()


def collect_once():
    now = datetime.now(TZ)
    if schedule_day != now.date(): load_schedule(now.date())
    feed = gtfs_realtime_pb2.FeedMessage()
    feed.ParseFromString(fetch(REALTIME_URL))
    updates = []
    with lock:
        current = dict(schedule)
        current_legs = dict(journey_schedule)
    leg_updates = []
    for entity in feed.entity:
        if not entity.HasField("vehicle"):
            continue
        vehicle = entity.vehicle
        trip = vehicle.trip.trip_id
        stamp = int(vehicle.timestamp or feed.header.timestamp or time.time())
        local = datetime.fromtimestamp(stamp, TZ)
        if local.date() != now.date():
            continue
        if trip in current and vehicle.stop_id == NEXT_STOP:
            updates.append((stamp, stamp, now.date().isoformat(), trip))
        for (leg_trip, leg), values in current_legs.items():
            if trip != leg_trip:
                continue
            origin_next, destination_stop = values[4], values[5]
            if vehicle.stop_id == origin_next:
                leg_updates.append(("origin", stamp, now.date().isoformat(), trip, leg))
            if vehicle.stop_id == destination_stop and vehicle.current_status == 1:
                leg_updates.append(("destination", stamp, now.date().isoformat(), trip, leg))
    if updates:
        connection = db()
        connection.executemany("UPDATE departures SET actual=COALESCE(actual,?), observed_at=?, estimated=0 WHERE service_date=? AND trip_id=?", updates)
        connection.commit(); connection.close()
    if leg_updates:
        connection = db()
        for field, stamp, day, trip, leg in leg_updates:
            column = "origin_actual" if field == "origin" else "destination_actual"
            connection.execute(f"UPDATE journey_legs SET {column}=COALESCE({column},?) WHERE service_date=? AND trip_id=? AND leg=?", (stamp, day, trip, leg))
        connection.commit(); connection.close()


def collector():
    while True:
        try: collect_once()
        except Exception as exc: print(datetime.now(), "collector:", exc, flush=True)
        time.sleep(15)


def stats():
    connection = db(); connection.row_factory = sqlite3.Row
    rows = connection.execute("""SELECT d.*, j.destination_planned AS arrival_planned,
        j.destination_actual AS arrival_actual
        FROM departures d LEFT JOIN journey_legs j
        ON j.service_date=d.service_date AND j.trip_id=d.trip_id AND j.leg='trolley'
        WHERE d.planned BETWEEN ? AND ?
        ORDER BY d.service_date DESC, d.planned DESC LIMIT 1000""",
        (7 * 3600, 8 * 3600 + 15 * 60)).fetchall()
    connection.close()
    result = []
    for row in rows:
        item = dict(row)
        item["delay"] = item["actual"] - (datetime.fromisoformat(item["service_date"]).replace(tzinfo=TZ).timestamp() + item["planned"]) if item["actual"] else None
        if item["estimated"]: item["destination"] += " · odhad"
        result.append(item)
    return {"generated": int(time.time()), "records": result}


def journeys():
    connection = db(); connection.row_factory = sqlite3.Row
    rows = connection.execute("""SELECT * FROM journey_legs
        WHERE leg != 'tram' OR origin_planned >= ?
        ORDER BY service_date DESC, origin_planned""", (6 * 3600 + 45 * 60,)).fetchall()
    connection.close()
    return {"generated": int(time.time()), "transfer_seconds": 240, "deadline": 7 * 3600 + 50 * 60,
            "records": [dict(row) for row in rows]}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs): super().__init__(*args, directory=str(ROOT / "web"), **kwargs)
    def do_GET(self):
        if self.path.split("?", 1)[0] == "/api/stats":
            body = json.dumps(stats(), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path.split("?", 1)[0] == "/api/journeys":
            body = json.dumps(journeys(), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        super().do_GET()
    def log_message(self, fmt, *args): pass


if __name__ == "__main__":
    load_schedule(datetime.now(TZ).date())
    threading.Thread(target=collector, daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", 8097), Handler).serve_forever()
