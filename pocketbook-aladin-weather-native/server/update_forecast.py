#!/usr/bin/env python3
"""Create a small Brno forecast file from official CHMI ALADIN GRIB data."""
import argparse, bz2, re, tempfile, urllib.request
from datetime import datetime, timedelta, timezone
from pathlib import Path
from zoneinfo import ZoneInfo
from eccodes import codes_grib_find_nearest, codes_grib_new_from_file, codes_get, codes_release

BASE = "https://opendata.chmi.cz/meteorology/weather/nwp_aladin/CZ_1km"
LAT, LON = 49.1951, 16.6068
VARS = ["CLSTEMPERATURE", "CLSWIND_SPEED", "CLSWIND_DIREC", "CLSRAFAL_MOD_XFU", "SURFNEBUL_TOTALE", "SURFPREC_TOTAL"]

def latest_run():
    available = []
    for run in ("00", "06", "12", "18"):
        html = urllib.request.urlopen(f"{BASE}/{run}/", timeout=30).read().decode("utf-8", "ignore")
        stamps = set(re.findall(r"ALADCZ1K4opendata_(\d{10})_CLSTEMPERATURE\.grb\.bz2", html))
        available.extend((stamp, run) for stamp in stamps)
    if not available: raise RuntimeError("CHMI directory contains no forecast")
    return max(available)

def values(url, folder):
    packed = folder / "data.bz2"; raw = folder / "data.grb"
    with urllib.request.urlopen(url, timeout=120) as src, packed.open("wb") as dst:
        while chunk := src.read(1024 * 1024): dst.write(chunk)
    with bz2.open(packed, "rb") as src, raw.open("wb") as dst:
        while chunk := src.read(1024 * 1024): dst.write(chunk)
    out = {}
    with raw.open("rb") as stream:
        while (gid := codes_grib_new_from_file(stream)) is not None:
            try:
                step = int(codes_get(gid, "step"))
                if step <= 72: out[step] = float(codes_grib_find_nearest(gid, LAT, LON)[0]["value"])
            finally: codes_release(gid)
    packed.unlink(); raw.unlink()
    return out

def recommendation(rows):
    near = rows[:12]
    low, high = min(r[1] for r in near), max(r[1] for r in near)
    rain, gust = max(r[2] for r in near), max(r[4] for r in near)
    if high < 2: clothes = "Zimni bunda, cepice a rukavice."
    elif high < 9: clothes = "Tepla bunda a uzavrene boty."
    elif high < 15: clothes = "Bunda nebo svetr, idealne ve vrstvach."
    elif high < 21: clothes = "Lehka bunda nebo mikina."
    else: clothes = "Tricko a lehke obleceni."
    extra = " Destnik nebo plastenka se budou hodit." if rain >= .4 else ""
    if gust >= 12: extra += " Pocitej se silnym vetrem."
    return f"{clothes}{extra} Rozmezi {low:.0f} az {high:.0f} C."

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--output", default="public/weather.dat")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args(); output = Path(args.output); output.parent.mkdir(parents=True, exist_ok=True)
    stamp, run = latest_run()
    if output.exists() and not args.force and f"|{stamp}|" in output.read_text(encoding="ascii", errors="ignore").splitlines()[0]:
        print(f"ALADIN run {stamp} is already current")
        return
    data = {}
    with tempfile.TemporaryDirectory() as td:
        folder = Path(td)
        for var in VARS:
            name = f"ALADCZ1K4opendata_{stamp}_{var}.grb.bz2"
            data[var] = values(f"{BASE}/{run}/{name}", folder)
    start = datetime.strptime(stamp, "%Y%m%d%H").replace(tzinfo=timezone.utc)
    common = sorted(set.intersection(*(set(v) for v in data.values())))
    rows=[]; previous = 0.0
    now_hour = datetime.now(ZoneInfo("Europe/Prague")).replace(minute=0, second=0, microsecond=0)
    for step in common:
        cumulative=data["SURFPREC_TOTAL"][step]
        rain=max(0.0,cumulative-previous) if step else 0.0; previous=cumulative
        local=(start+timedelta(hours=step)).astimezone(ZoneInfo("Europe/Prague"))
        temp=data["CLSTEMPERATURE"][step]-273.15; cloud=data["SURFNEBUL_TOTALE"][step]
        icon=3 if rain>=.2 else 2 if cloud>=.72 else 1 if cloud>=.3 else 0
        if local >= now_hour:
            rows.append((local,temp,rain,data["CLSWIND_SPEED"][step],data["CLSRAFAL_MOD_XFU"][step],data["CLSWIND_DIREC"][step],icon))
    lines=[f"META|Brno|{stamp}|{datetime.now(ZoneInfo('Europe/Prague')):%d.%m. %H:%M}"]
    for r in rows: lines.append(f"H|{r[0]:%d.%m.}|{r[0]:%H}|{r[1]:.1f}|{r[2]:.1f}|{r[3]:.1f}|{r[4]:.1f}|{r[5]:.0f}|{r[6]}")
    lines.append("REC|"+recommendation(rows))
    tmp=output.with_suffix(".tmp"); tmp.write_text("\n".join(lines)+"\n", encoding="ascii"); tmp.replace(output)
    print(f"Wrote {len(rows)} hours from ALADIN run {stamp} to {output}")
if __name__ == "__main__": main()
