#!/usr/bin/env python3
import base64
import hmac
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
TRANSIT_CACHE = {}
AURORA_CACHE = {}
MEDLANKY_CACHE = {}
MEDLANKY_SPORTS_CACHE = {}
MENUS_CACHE = {}
AUTH_FILE = Path.home() / ".config" / "brno-live-auth"


def dashboard_authorized(header):
    try:
        expected = AUTH_FILE.read_text(encoding="utf-8").strip()
        scheme, token = (header or "").split(" ", 1)
        supplied = base64.b64decode(token).decode("utf-8")
        return scheme.lower() == "basic" and hmac.compare_digest(supplied, expected)
    except (OSError, ValueError, UnicodeDecodeError):
        return False


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
    flights = brno_airport_schedule(now)
    schedule_available = bool(flights)
    flight_source = "Letiště Brno"
    flight_source_url = "https://www.brno-airport.cz/en/flight-information"
    if not flights:
        flights = flightstats_schedule(now)
        schedule_available = bool(flights)
        flight_source = "FlightStats (záložní zdroj)"
        flight_source_url = "https://www.airportia.com/czech-republic/brno-turany-airport/"
    data = {"aircraft": aircraft, "flights": sorted(flights, key=lambda item: item["scheduled"]),
            "schedule_available": schedule_available, "flight_source": flight_source,
            "flight_source_url": flight_source_url, "updated": int(time.time())}
    AVIATION_CACHE.update(data=data, at=time.time())
    return data


def brno_airport_schedule(now):
    flights = []
    pages = {"arrivals": "prilety", "departures": "odlety"}
    for direction, path in pages.items():
        request = urllib.request.Request(
            f"https://www.brno-airport.cz/{path}",
            headers={"User-Agent": "facebookexternalhit/1.1"},
        )
        try:
            with urllib.request.urlopen(request, timeout=15) as response:
                page = response.read().decode("utf-8", "replace")
        except OSError:
            continue
        if "flight-table__table" not in page:
            continue
        for row in re.findall(r"<tr[^>]*>(.*?)</tr>", page, re.S):
            cells = re.findall(r"<td[^>]*>(.*?)</td>", row, re.S)
            values = [" ".join(html.unescape(re.sub(r"<[^>]+>", " ", cell)).split()) for cell in cells]
            if len(values) < 5 or not re.fullmatch(r"\d{2}:\d{2}", values[0]):
                continue
            date_label = values[1].lower()
            day_offset = 1 if date_label in ("zítra", "zitra", "tomorrow") else 0
            scheduled = datetime.combine(
                now.date() + timedelta(days=day_offset),
                datetime.strptime(values[0], "%H:%M").time(),
                now.tzinfo,
            )
            if now - timedelta(hours=3) <= scheduled <= now + timedelta(hours=48):
                flights.append({"direction": direction, "scheduled": scheduled.isoformat(),
                                "airline": values[2], "place": values[3], "flight": values[4],
                                "note": values[5] if len(values) > 5 else ""})
    return flights


def flightstats_schedule(now):
    flights = []
    for direction in ("arrivals", "departures"):
        url = f"https://www.flightstats.com/v2/flight-tracker/{direction}/BRQ"
        request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        try:
            page = urllib.request.urlopen(request, timeout=15).read().decode("utf-8", "replace")
            match = re.search(r"__NEXT_DATA__\s*=\s*(\{.*?\});__NEXT_LOADED_PAGES__", page, re.S)
            payload = json.loads(match.group(1)) if match else {}
            rows = payload.get("props", {}).get("initialState", {}).get("flightTracker", {}).get("route", {}).get("flights", [])
            for row in rows:
                stamp = row.get("sortTime")
                carrier = row.get("carrier") or {}
                airport = row.get("airport") or {}
                if not stamp:
                    continue
                scheduled = datetime.fromisoformat(stamp.replace("Z", "+00:00")).astimezone(now.tzinfo)
                if not (now - timedelta(hours=3) <= scheduled <= now + timedelta(hours=48)):
                    continue
                flights.append({
                    "direction": direction,
                    "scheduled": scheduled.isoformat(),
                    "airline": carrier.get("name") or carrier.get("fs") or "–",
                    "place": airport.get("city") or airport.get("fs") or "–",
                    "flight": f'{carrier.get("fs", "")}{carrier.get("flightNumber", "")}' or "–",
                    "note": "",
                })
        except (OSError, ValueError, json.JSONDecodeError):
            continue
    return flights


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


def transit_incidents():
    if TRANSIT_CACHE.get("data") and time.time() - TRANSIT_CACHE["at"] < 120:
        return TRANSIT_CACHE["data"]
    request = urllib.request.Request("https://www.dpmb.cz/", headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=15) as response:
        page = response.read().decode("utf-8", "replace")
    selected = {"1", "6", "25", "26", "32"}
    incidents = []
    for article in re.findall(r'<article class="[^"]*node--type-event[^"]*">(.*?)</article>', page, re.S):
        lines = set(re.findall(r'field--name-name[^>]*field__item">\s*([^<]+)', article))
        affected = sorted(lines & selected, key=int)
        if not affected:
            continue
        title_match = re.search(r'<a href="([^"]+)"><span[^>]*field--name-title[^>]*>(.*?)</span>', article, re.S)
        times = re.findall(r'<time datetime="([^"]+)"[^>]*>(.*?)</time>', article, re.S)
        delay_match = re.search(r'field--name-field-zdrzeni.*?field__item">\s*([^<]+)', article, re.S)
        direction_match = re.search(r'field--name-field-smer.*?field__item">\s*([^<]+)', article, re.S)
        if not title_match:
            continue
        incidents.append({"title": " ".join(html.unescape(title_match.group(2)).split()),
                          "url": "https://www.dpmb.cz" + title_match.group(1), "lines": affected,
                          "from": times[0][0] if times else None, "to": times[1][0] if len(times) > 1 else None,
                          "delay_minutes": delay_match.group(1).strip() if delay_match else None,
                          "direction": direction_match.group(1).strip() if direction_match else None})
    data = {"incidents": incidents, "lines": sorted(selected, key=int), "checked": int(time.time())}
    TRANSIT_CACHE.update(data=data, at=time.time())
    return data


def aurora_data():
    if AURORA_CACHE.get("data") and time.time() - AURORA_CACHE["at"] < 300:
        return AURORA_CACHE["data"]
    ovation = fetch_json("https://services.swpc.noaa.gov/json/ovation_aurora_latest.json")
    kp_rows = fetch_json("https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json")
    now = datetime.now(timezone.utc).replace(tzinfo=None)
    observed = [row for row in kp_rows if row.get("observed") == "observed"]
    predicted = []
    for row in kp_rows:
        stamp = datetime.fromisoformat(row["time_tag"].replace("Z", ""))
        if row.get("observed") == "predicted" and now <= stamp <= now + timedelta(hours=48):
            predicted.append(row)
    current_kp = float(observed[-1]["kp"]) if observed else None
    peak = max(predicted, key=lambda row: float(row["kp"])) if predicted else None
    local = min(ovation["coordinates"], key=lambda point: abs(point[0] - BRNO[1]) + abs(point[1] - BRNO[0]))
    peak_kp = float(peak["kp"]) if peak else current_kp
    if peak_kp is None or peak_kp < 6:
        level, message = "velmi malá", "Z Brna nyní polární záře pravděpodobně vidět nebude."
    elif peak_kp < 7:
        level, message = "malá", "Fotograficky může být slabě zachytitelná nízko nad severním obzorem."
    elif peak_kp < 8:
        level, message = "zvýšená", "Vyplatí se sledovat jasný severní obzor mimo městské osvětlení."
    else:
        level, message = "vysoká", "Polární záře může být z jižní Moravy viditelná i pouhým okem."
    data = {"current_kp": current_kp, "peak_kp_48h": peak_kp,
            "peak_time": peak["time_tag"] + "Z" if peak else None,
            "brno_probability": local[2], "forecast_time": ovation["Forecast Time"],
            "level": level, "message": message}
    AURORA_CACHE.update(data=data, at=time.time())
    return data


def medlanky_events():
    if MEDLANKY_CACHE.get("data") and time.time() - MEDLANKY_CACHE["at"] < 1800:
        return MEDLANKY_CACHE["data"]
    today = datetime.now(ZoneInfo("Europe/Prague")).date().isoformat()
    query = urllib.parse.urlencode({"q": "", "fromDate": today, "groupId": "4864658",
                                    "showSiteUrl": "false", "page": 1, "pageSize": 20,
                                    "order": "datumOd"})
    payload = fetch_json("https://medlanky.brno.cz/o/rest/search/mc/akce?" + query)
    events = []
    for row in payload.get("results", []):
        events.append({"title": row.get("title"), "date_from": row.get("datumOd"),
                       "date_to": row.get("datumDo"), "time_from": row.get("casOd"),
                       "time_to": row.get("casDo"), "place": row.get("nazevMista"),
                       "url": "https://medlanky.brno.cz" + row.get("url", "")})
    data = {"events": events, "total": payload.get("total", len(events)), "checked": int(time.time())}
    MEDLANKY_CACHE.update(data=data, at=time.time())
    return data


def medlanky_sports():
    if MEDLANKY_SPORTS_CACHE.get("data") and time.time() - MEDLANKY_SPORTS_CACHE["at"] < 21600:
        return MEDLANKY_SPORTS_CACHE["data"]
    query = urllib.parse.urlencode({
        "where": "adresa_cast_obce='Medlánky'", "outFields": "*",
        "returnGeometry": "true", "outSR": 4326, "f": "geojson",
    })
    url = "https://gis.brno.cz/ags1/rest/services/OMI/OMI_pasport_hrist_a_sportovist/FeatureServer/1/query?" + query
    data = fetch_json(url)
    enrichments = {
        "Jabloňová": {
            "equipment": "Hrazdy, bradla, prvky pro cvičení vlastní vahou a tenisová stěna.",
            "access": "Veřejnost: Po, St, Pá 17–20 h; So, Ne 8–20 h.",
        },
        "V Újezdech": {
            "equipment": "Fitness a workout; v areálu jsou také víceúčelové sportovní plochy.",
            "access": "Veřejně přístupný venkovní areál.",
        },
        "K Rybníku": {
            "equipment": "Dvě pískoviště a houpačky; hřiště je určené zejména menším dětem.",
            "access": "Veřejně přístupné, podle OSM bez omezení vstupu.",
        },
    }
    for feature in data.get("features", []):
        props = feature.setdefault("properties", {})
        street = props.get("adresa_ulice") or props.get("nazev") or "Místo bez názvu"
        extra = enrichments.get(street, {})
        if street == "V Újezdech" and props.get("typ_hriste_nazev") == "dětské hřiště":
            extra = {}
        props["display_name"] = props.get("nazev") or f"{street} – {props.get('typ_hriste_nazev', 'hřiště')}"
        props["equipment_display"] = props.get("popis") or props.get("sportoviste_nazev") or extra.get("equipment") or "Veřejné mapové zdroje místo potvrzují, konkrétní herní prvky ale nepopisují."
        props["access_display"] = props.get("dostupnost") or extra.get("access") or "Veřejně přístupné podle OpenStreetMap; provozní doba není uvedena."
        props["source_url"] = "https://www.openstreetmap.org/search?query=" + urllib.parse.quote(f"{street}, Brno-Medlánky")
        if street == "K Rybníku":
            props["source_url"] = "https://paro.damenavas.cz/project/498/"
        if street == "Jabloňová" and props.get("typ_hriste_nazev") == "sportoviště":
            props["display_name"] = "Workout na Jabloňové u ZŠ"
            props["source_url"] = "https://zdravi.brno.cz/wp-content/uploads/2026/06/Adresar_seniorskych_organizaci_2026.pdf"
    data.setdefault("features", []).append({
        "type": "Feature", "geometry": {"type": "Point", "coordinates": [16.5797361, 49.2389839]},
        "properties": {"display_name": "Centrum volného času Jabloňka", "typ_hriste_nazev": "volnočasové centrum",
                       "equipment_display": "Prostory pro dětské kroužky, výtvarné aktivity, jógu, pilates a komunitní program.",
                       "access_display": "Přístup v době pořádaných aktivit nebo po sjednaném pronájmu.",
                       "source_url": "https://medlanky.brno.cz/w/pronajem-centra-volneho-casu-jablonka"},
    })
    data.setdefault("features", []).append({
        "type": "Feature", "geometry": {"type": "Point", "coordinates": [16.579094, 49.239175]},
        "properties": {"display_name": "Sportovní hřiště u CVČ Jabloňka", "typ_hriste_nazev": "sportoviště",
                       "equipment_display": "Běžecký ovál a víceúčelové hřiště pro basketbal a házenou.",
                       "access_display": "Školní sportovní areál; vstup se řídí provozem školy a organizovaných aktivit.",
                       "source_url": "https://www.openstreetmap.org/?mlat=49.239175&mlon=16.579094#map=19/49.239175/16.579094"},
    })
    data.setdefault("features", []).append({
        "type": "Feature", "geometry": {"type": "Point", "coordinates": [16.575801, 49.240191]},
        "properties": {"display_name": "Multifunkční hřiště Matalova", "typ_hriste_nazev": "sportoviště",
                       "equipment_display": "Asfaltová multifunkční plocha pro míčové hry.",
                       "access_display": "Veřejně přístupné venkovní hřiště.",
                       "source_url": "https://www.openstreetmap.org/?mlat=49.240191&mlon=16.575801#map=19/49.240191/16.575801"},
    })
    MEDLANKY_SPORTS_CACHE.update(data=data, at=time.time())
    return data


def _strava_menu(canteen, target):
    payload = fetch_json_with_body("https://app.strava.cz/api/jidelnickyPage", {
        "cislo": canteen, "lang": "CZ",
    }, method="POST")
    target_text = target.strftime("%d.%m.%Y")
    rows = []
    for table in payload.get("meals", {}).values():
        if isinstance(table, list):
            rows.extend(row for row in table if row.get("datum") == target_text)
    return [{"type": row.get("druh_popis") or row.get("druh_chod"), "name": row.get("nazev")} for row in rows]


def fetch_json_with_body(url, payload, method="GET"):
    request = urllib.request.Request(url, data=json.dumps(payload).encode(), method=method,
                                     headers={"User-Agent": "Meleys-Brno-Live/1.0",
                                              "Content-Type": "text/plain;charset=UTF-8",
                                              "Referer": "https://app.strava.cz/"})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def _uvoz_menu(target):
    request = urllib.request.Request("https://www.sjuvoz.cz/jidelnicky/", headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=20) as response:
        page = response.read().decode("utf-8", "replace")
    target_text = target.strftime("%d.%m.%Y")
    start = page.find(f"({target_text})")
    if start < 0:
        return []
    end = page.find("<table style='border: solid black", start)
    block = page[start:end if end >= 0 else len(page)]
    rows = []
    for label, meal in re.findall(r"<td[^>]*align='right'[^>]*>(.*?)</td><td[^>]*>(.*?)(?:<div class='alergeny'>|</td>)", block, re.S):
        clean = lambda value: html.unescape(re.sub(r"<[^>]+>", " ", value)).strip()
        rows.append({"type": clean(label), "name": clean(meal)})
    return rows


def school_menus():
    now = datetime.now(ZoneInfo("Europe/Prague"))
    target = now.date()
    if now.hour >= 15:
        target += timedelta(days=1)
    while target.weekday() >= 5:
        target += timedelta(days=1)
    key = target.isoformat()
    if MENUS_CACHE.get("key") == key and time.time() - MENUS_CACHE["at"] < 1800:
        return MENUS_CACHE["data"]
    sources = [
        ("ZŠ Úvoz", lambda: _uvoz_menu(target), "https://www.sjuvoz.cz/jidelnicky/"),
        ("Vitalité · MŠ Lentilka Kounicova", lambda: [x for x in _strava_menu("10190", target) if any(k in (x["type"] or "").lower() for k in ("přesníd", "mš", "svačin"))], "https://app.strava.cz/jidelnicky?jidelna=10190"),
        ("Gymnázium Brno-Řečkovice", lambda: _strava_menu("4658", target), "https://app.strava.cz/jidelnicky?jidelna=4658"),
    ]
    menus = []
    for name, loader, source in sources:
        try:
            meals = loader()
            error = None if meals else "Jídelníček na tento den zatím není ve veřejném zdroji."
        except Exception:
            meals, error = [], "Jídelníček se nyní nepodařilo načíst."
        menus.append({"school": name, "meals": meals, "message": error, "source": source})
    data = {"date": key, "menus": menus}
    MENUS_CACHE.update(key=key, data=data, at=time.time())
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
        "hourly": "temperature_2m,apparent_temperature,precipitation_probability,precipitation,cloud_cover,wind_speed_10m,wind_gusts_10m",
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


def night_viirs_image():
    date = datetime.now(timezone.utc).date().isoformat()
    params = {
        "SERVICE": "WMS", "VERSION": "1.1.1", "REQUEST": "GetMap",
        "LAYERS": "VIIRS_NOAA20_DayNightBand_At_Sensor_Radiance", "STYLES": "",
        "SRS": "EPSG:4326", "BBOX": "13.8,47.6,19.4,50.8",
        "WIDTH": 1400, "HEIGHT": 800, "FORMAT": "image/png", "TIME": date,
    }
    url = "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?" + urllib.parse.urlencode(params)
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=25) as response:
        return response.read(), date


def night_infrared_image():
    params = {
        "service": "WMS", "request": "GetMap", "version": "1.3.0",
        "layers": "msg_fes:ir108", "styles": "", "format": "image/png",
        "crs": "EPSG:4326", "bbox": "47.6,13.8,50.8,19.4",
        "width": 1400, "height": 800,
    }
    url = "https://view.eumetsat.int/geoserver/wms?" + urllib.parse.urlencode(params)
    request = urllib.request.Request(url, headers={"User-Agent": "Meleys-Brno-Live/1.0"})
    with urllib.request.urlopen(request, timeout=25) as response:
        return response.read()


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        path = urllib.parse.urlsplit(self.path).path
        if path in ("/", "/index.html", "/api/school-menus") and not dashboard_authorized(self.headers.get("Authorization")):
            self.send_response(401)
            self.send_header("WWW-Authenticate", 'Basic realm="Soukromy dashboard Brno", charset="UTF-8"')
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path in ("/verejne", "/verejne.html"):
            page = (ROOT / "web" / "index.html").read_text(encoding="utf-8")
            page = page.replace("<title>Brno živě</title>", "<title>Brno živě · veřejný přehled</title>")
            page = re.sub(r'\s*<section class="section family-schedule-section">.*?</section>', "", page, flags=re.S)
            page = re.sub(r'\s*<section class="section school-menus-section">.*?</section>', "", page, flags=re.S)
            page = re.sub(r'\s*<article><h3>MUDr\. Ivana Bartůňková</h3>.*?</article>', "", page, flags=re.S)
            page = page.replace("<h2>Lékařka a lékárna</h2>", "<h2>Lékárna v Medlánkách</h2>")
            body = page.encode("utf-8")
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/school-menus":
            try:
                body, status = json.dumps(school_menus()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/medlanky-sports":
            try:
                body, status = json.dumps(medlanky_sports()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/geo+json")
            self.send_header("Cache-Control", "public, max-age=3600"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/medlanky-events":
            try:
                body, status = json.dumps(medlanky_events()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/aurora":
            try:
                body, status = json.dumps(aurora_data()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/night-viirs":
            try:
                body, date = night_viirs_image()
                self.send_response(200); self.send_header("Content-Type", "image/png")
                self.send_header("X-Imagery-Date", date); self.send_header("Cache-Control", "public, max-age=1800")
                self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
            except Exception as exc:
                self.send_error(503, str(exc))
            return
        if self.path == "/api/night-infrared":
            try:
                body = night_infrared_image()
                self.send_response(200); self.send_header("Content-Type", "image/png")
                self.send_header("Cache-Control", "public, max-age=600"); self.send_header("Content-Length", str(len(body)))
                self.end_headers(); self.wfile.write(body)
            except Exception as exc:
                self.send_error(503, str(exc))
            return
        if self.path == "/api/transit-incidents":
            try:
                body, status = json.dumps(transit_incidents()).encode(), 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc)}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
        if urllib.parse.urlsplit(self.path).path == "/api/live-departures":
            try:
                query = urllib.parse.urlsplit(self.path).query
                url = "http://127.0.0.1:8097/api/live-departures" + ("?" + query if query else "")
                body = json.dumps(fetch_json(url), ensure_ascii=False).encode()
                status = 200
            except Exception as exc:
                body, status = json.dumps({"error": str(exc), "groups": []}).encode(), 503
            self.send_response(status); self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body); return
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
