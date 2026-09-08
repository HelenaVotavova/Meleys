#!/usr/bin/env python3
import csv
import io
import json
import re
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
test_schedule = {}
route_labels = {}
trip_labels = {}
vehicle_plans = {}
active_test_runs = {}
test_target_seen = {}
leg_target_seen = {}
tracked_vehicle_state = {}
TRACKED_VEHICLES = {"31054": "Lena", "19080": "Helenka", "30650": "Mario", "30660": "Luigi"}
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
    connection.execute("""CREATE TABLE IF NOT EXISTS test_runs(
        service_date TEXT NOT NULL, trip_id TEXT NOT NULL, vehicle_id TEXT,
        line TEXT NOT NULL, destination TEXT NOT NULL, target TEXT,
        origin_planned INTEGER, destination_planned INTEGER,
        origin_actual INTEGER, destination_actual INTEGER,
        scheduled INTEGER NOT NULL DEFAULT 1,
        PRIMARY KEY(service_date, trip_id))""")
    connection.execute("""CREATE TABLE IF NOT EXISTS vehicle_stops(
        service_date TEXT NOT NULL, trip_key TEXT NOT NULL, trip_id TEXT,
        line TEXT NOT NULL, destination TEXT NOT NULL, stop_id TEXT NOT NULL,
        stop_name TEXT NOT NULL, planned INTEGER, first_seen INTEGER NOT NULL,
        last_seen INTEGER NOT NULL, passed_at INTEGER, latitude REAL, longitude REAL,
        PRIMARY KEY(service_date, trip_key, stop_id))""")
    columns = {row[1] for row in connection.execute("PRAGMA table_info(vehicle_stops)")}
    if "vehicle_code" not in columns:
        connection.execute("ALTER TABLE vehicle_stops ADD COLUMN vehicle_code TEXT")
        connection.execute("UPDATE vehicle_stops SET vehicle_code='31054' WHERE vehicle_code IS NULL")
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


def realtime_stop_id(stop_id):
    match = re.fullmatch(r"U(\d+)Z(\d+)", stop_id)
    return f"U{int(match.group(1)):05d}Z{int(match.group(2)):02d}" if match else stop_id


def load_schedule(day):
    global schedule, journey_schedule, test_schedule, route_labels, trip_labels, vehicle_plans, schedule_day
    if not GTFS.exists() or time.time() - GTFS.stat().st_mtime > 20 * 3600:
        GTFS.write_bytes(fetch(STATIC_URL))
    with zipfile.ZipFile(GTFS) as archive:
        active = active_services(archive, day)
        route_names = {r["route_id"]: r["route_short_name"] for r in csv_rows(archive, "routes.txt")}
        stop_names = {r["stop_id"]: r["stop_name"] for r in csv_rows(archive, "stops.txt")}
        trip_rows = list(csv_rows(archive, "trips.txt"))
        all_trip_labels = {r["trip_id"]: (route_names.get(r["route_id"], ""), r["trip_headsign"])
                           for r in trip_rows}
        trips = {r["trip_id"]: all_trip_labels[r["trip_id"]] for r in trip_rows if r["service_id"] in active}
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
        tests = {}
        plans = {}
        for trip, rows in stop_times.items():
            for row in rows:
                plans[(trip, realtime_stop_id(row["stop_id"]))] = (seconds(row["arrival_time"]), stop_names.get(row["stop_id"], row["stop_id"]))
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
            if "U1483Z1" in ids:
                oi = ids.index("U1483Z1")
                targets = [(i, "Vozovna Medlánky") for i in range(oi + 1, len(ids))
                           if ids[i] in {"U1756Z1", "U1756Z3"}]
                if not targets:
                    targets = [(i, "Kořískova") for i in range(oi + 1, len(ids)) if ids[i] == "U1272Z1"]
                if targets:
                    di, target = targets[0]
                    tests[trip] = (line or "?", destination, target, seconds(rows[oi]["departure_time"]), seconds(rows[di]["arrival_time"]))
    with lock:
        schedule, journey_schedule, test_schedule, route_labels, trip_labels, vehicle_plans, schedule_day = found, legs, tests, route_names, all_trip_labels, plans, day
    connection = db()
    connection.executemany("INSERT OR IGNORE INTO departures(service_date,trip_id,line,destination,planned,actual,observed_at) VALUES(?,?,?,?,?,NULL,NULL)",
        [(day.isoformat(), trip, line, destination, planned) for trip, (line, destination, planned) in found.items()])
    connection.executemany("INSERT OR IGNORE INTO journey_legs(service_date,trip_id,leg,line,destination,origin_planned,destination_planned) VALUES(?,?,?,?,?,?,?)",
        [(day.isoformat(), trip, leg, values[0], values[1], values[2], values[3]) for (trip, leg), values in legs.items()])
    connection.executemany("""INSERT INTO test_runs(service_date,trip_id,line,destination,target,origin_planned,destination_planned,scheduled)
        VALUES(?,?,?,?,?,?,?,1) ON CONFLICT(service_date,trip_id) DO UPDATE SET line=excluded.line,
        destination=excluded.destination, target=excluded.target, origin_planned=excluded.origin_planned,
        destination_planned=excluded.destination_planned, scheduled=1""",
        [(day.isoformat(), trip, *values) for trip, values in tests.items()])
    for trip_id, vehicle_id, target in connection.execute("""SELECT trip_id,vehicle_id,target FROM test_runs
            WHERE service_date=? AND origin_actual IS NOT NULL AND destination_actual IS NULL AND vehicle_id IS NOT NULL""", (day.isoformat(),)):
        active_test_runs[vehicle_id] = (trip_id, target)
    connection.commit(); connection.close()


def finish_test_run(day, vehicle_id, stamp, target):
    record_trip = active_test_runs.get(vehicle_id, (None, None))[0]
    if not record_trip:
        return
    connection = db()
    connection.execute("""UPDATE test_runs SET destination_actual=COALESCE(destination_actual,?),
        target=COALESCE(target,?) WHERE service_date=? AND trip_id=?""",
        (stamp, target, day, record_trip))
    connection.commit(); connection.close()
    active_test_runs.pop(vehicle_id, None)
    test_target_seen.pop(vehicle_id, None)


def collect_once():
    now = datetime.now(TZ)
    if schedule_day != now.date(): load_schedule(now.date())
    feed = gtfs_realtime_pb2.FeedMessage()
    feed.ParseFromString(fetch(REALTIME_URL))
    updates = []
    with lock:
        current = dict(schedule)
        current_legs = dict(journey_schedule)
        current_tests = dict(test_schedule)
        current_routes = dict(route_labels)
        current_trip_labels = dict(trip_labels)
    leg_updates = []
    live_vehicle_ids = set()
    for entity in feed.entity:
        if not entity.HasField("vehicle"):
            continue
        vehicle = entity.vehicle
        trip = vehicle.trip.trip_id
        vehicle_id = vehicle.vehicle.id or vehicle.vehicle.label or entity.id
        live_vehicle_ids.add(vehicle_id)
        stamp = int(vehicle.timestamp or feed.header.timestamp or time.time())
        local = datetime.fromtimestamp(stamp, TZ)
        if local.date() != now.date():
            continue
        tracked_code = next((code for code in TRACKED_VEHICLES if code in {vehicle.vehicle.id, vehicle.vehicle.label}), None)
        if tracked_code:
            state = tracked_vehicle_state.get(tracked_code)
            trip_key = trip or (state[0] if state else f"vehicle:{tracked_code}:{stamp}")
            if state and (state[0] != trip_key or state[1] != vehicle.stop_id):
                connection = db()
                connection.execute("UPDATE vehicle_stops SET passed_at=COALESCE(passed_at,?) WHERE service_date=? AND trip_key=? AND stop_id=?",
                                   (stamp, now.date().isoformat(), state[0], state[1]))
                connection.commit(); connection.close()
            details = current_trip_labels.get(trip, (current_routes.get(vehicle.trip.route_id, vehicle.trip.route_id or "?"), "bez označení"))
            plan_info = vehicle_plans.get((trip, vehicle.stop_id), (None, vehicle.stop_id))
            connection = db()
            connection.execute("""INSERT INTO vehicle_stops(service_date,trip_key,trip_id,line,destination,stop_id,stop_name,planned,first_seen,last_seen,latitude,longitude,vehicle_code)
                VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(service_date,trip_key,stop_id) DO UPDATE SET
                last_seen=excluded.last_seen,latitude=excluded.latitude,longitude=excluded.longitude""",
                (now.date().isoformat(), trip_key, trip, details[0] or "?", details[1], vehicle.stop_id,
                 plan_info[1], plan_info[0], stamp, stamp, vehicle.position.latitude, vehicle.position.longitude, tracked_code))
            connection.commit(); connection.close()
            tracked_vehicle_state[tracked_code] = (trip_key, vehicle.stop_id)
        if trip in current and vehicle.stop_id == NEXT_STOP:
            updates.append((stamp, stamp, now.date().isoformat(), trip))
        for (leg_trip, leg), values in current_legs.items():
            if trip != leg_trip:
                continue
            origin_next, destination_stop = values[4], values[5]
            if vehicle.stop_id == origin_next:
                leg_updates.append(("origin", stamp, now.date().isoformat(), trip, leg))
            key = (trip, leg)
            if vehicle.stop_id == destination_stop:
                leg_target_seen[key] = stamp
                if vehicle.current_status in {0, 1}:
                    leg_updates.append(("destination", stamp, now.date().isoformat(), trip, leg))
                    leg_target_seen.pop(key, None)
            elif key in leg_target_seen:
                leg_updates.append(("destination", stamp, now.date().isoformat(), trip, leg))
                leg_target_seen.pop(key, None)
        if vehicle.stop_id == "U01166Z01" and vehicle_id not in active_test_runs:
            record_trip = trip or f"vehicle:{vehicle_id}:{stamp}"
            details = current_tests.get(trip)
            fallback = current_trip_labels.get(trip, (current_routes.get(vehicle.trip.route_id, vehicle.trip.route_id or "Služební"), "bez označení"))
            line = details[0] if details else fallback[0]
            destination = details[1] if details else fallback[1]
            expected_target = details[2] if details else ("Vozovna Medlánky" if "Vozovna Medlánky" in destination else None)
            if line == "6" and expected_target != "Vozovna Medlánky":
                continue
            connection = db()
            connection.execute("""INSERT INTO test_runs(service_date,trip_id,vehicle_id,line,destination,target,origin_actual,scheduled)
                VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(service_date,trip_id) DO UPDATE SET
                vehicle_id=excluded.vehicle_id, origin_actual=COALESCE(test_runs.origin_actual,excluded.origin_actual)""",
                (now.date().isoformat(), record_trip, vehicle_id, line, destination, expected_target, stamp, 1 if details else 0))
            connection.commit(); connection.close()
            active_test_runs[vehicle_id] = (record_trip, expected_target)
        target_by_stop = {"U01272Z01": "Kořískova", "U01756Z01": "Vozovna Medlánky", "U01756Z03": "Vozovna Medlánky"}
        if vehicle_id in active_test_runs:
            expected_target = active_test_runs[vehicle_id][1]
            reported_target = target_by_stop.get(vehicle.stop_id)
            if reported_target and (expected_target is None or reported_target == expected_target):
                if expected_target is None:
                    expected_target = reported_target
                    active_test_runs[vehicle_id] = (active_test_runs[vehicle_id][0], expected_target)
                test_target_seen[vehicle_id] = stamp
                if vehicle.current_status in {0, 1}:
                    finish_test_run(now.date().isoformat(), vehicle_id, stamp, expected_target)
            elif vehicle_id in test_target_seen:
                finish_test_run(now.date().isoformat(), vehicle_id, stamp, expected_target)
    for vehicle_id, last_stamp in list(test_target_seen.items()):
        if vehicle_id not in live_vehicle_ids and active_test_runs.get(vehicle_id, (None, None))[1] == "Vozovna Medlánky":
            finish_test_run(now.date().isoformat(), vehicle_id, last_stamp, "Vozovna Medlánky")
    for vehicle_id, (record_trip, target) in list(active_test_runs.items()):
        if vehicle_id not in live_vehicle_ids and vehicle_id not in test_target_seen and record_trip.startswith("vehicle:"):
            connection = db()
            connection.execute("DELETE FROM test_runs WHERE service_date=? AND trip_id=? AND destination_actual IS NULL",
                               (now.date().isoformat(), record_trip))
            connection.commit(); connection.close()
            active_test_runs.pop(vehicle_id, None)
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


def test_runs():
    connection = db(); connection.row_factory = sqlite3.Row
    rows = connection.execute("""SELECT * FROM test_runs
        WHERE scheduled=1 OR target IS NOT NULL OR destination_actual IS NOT NULL
        ORDER BY service_date DESC, COALESCE(origin_actual, origin_planned) DESC LIMIT 3000""").fetchall()
    connection.close()
    return {"generated": int(time.time()), "records": [dict(row) for row in rows]}


def tracked_vehicle(vehicle_code):
    connection = db(); connection.row_factory = sqlite3.Row
    rows = connection.execute("SELECT * FROM vehicle_stops WHERE vehicle_code=? ORDER BY service_date DESC, first_seen DESC LIMIT 3000", (vehicle_code,)).fetchall()
    connection.close()
    return {"generated": int(time.time()), "vehicle": vehicle_code, "name": TRACKED_VEHICLES.get(vehicle_code, vehicle_code),
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
        if self.path.split("?", 1)[0] == "/api/test-runs":
            body = json.dumps(test_runs(), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path.split("?", 1)[0] == "/api/vehicle-lena":
            body = json.dumps(tracked_vehicle("31054"), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path.startswith("/api/vehicle/"):
            vehicle_code = self.path.split("?", 1)[0].rsplit("/", 1)[-1]
            if vehicle_code not in TRACKED_VEHICLES:
                self.send_error(404); return
            body = json.dumps(tracked_vehicle(vehicle_code), ensure_ascii=False).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        super().do_GET()
    def log_message(self, fmt, *args): pass


if __name__ == "__main__":
    load_schedule(datetime.now(TZ).date())
    threading.Thread(target=collector, daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", 8097), Handler).serve_forever()
