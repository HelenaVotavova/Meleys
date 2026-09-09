from csv import writer
from datetime import datetime, timezone
from pathlib import Path
from time import monotonic, sleep

from sense_hat import SenseHat


RUN_SECONDS = 9 * 60 + 30
SAMPLE_SECONDS = 5
OUTPUT_FILE = Path(__file__).parent / "space_garden.csv"


def values(vector):
    return round(vector["x"], 4), round(vector["y"], 4), round(vector["z"], 4)


sense = SenseHat()
sense.color.gain = 4
sense.color.integration_cycles = 64
sense.clear(0, 20, 0)

header = [
    "time_utc",
    "elapsed_s",
    "temperature_c",
    "humidity_percent",
    "pressure_hpa",
    "light_red",
    "light_green",
    "light_blue",
    "light_clear",
    "magnetic_x",
    "magnetic_y",
    "magnetic_z",
    "acceleration_x",
    "acceleration_y",
    "acceleration_z",
    "gyroscope_x",
    "gyroscope_y",
    "gyroscope_z",
    "pitch",
    "roll",
    "yaw",
]

start = monotonic()

with OUTPUT_FILE.open("w", newline="", encoding="utf-8") as output:
    data = writer(output)
    data.writerow(header)

    while monotonic() - start < RUN_SECONDS:
        sample_start = monotonic()

        red, green, blue, clear = sense.color.colour
        magnetic = values(sense.get_compass_raw())
        acceleration = values(sense.get_accelerometer_raw())
        gyroscope = values(sense.get_gyroscope_raw())
        orientation = sense.get_orientation_degrees()

        data.writerow([
            datetime.now(timezone.utc).isoformat(),
            round(monotonic() - start, 2),
            round(sense.get_temperature(), 2),
            round(sense.get_humidity(), 2),
            round(sense.get_pressure(), 2),
            red,
            green,
            blue,
            clear,
            *magnetic,
            *acceleration,
            *gyroscope,
            round(orientation["pitch"], 2),
            round(orientation["roll"], 2),
            round(orientation["yaw"], 2),
        ])
        output.flush()

        remaining = SAMPLE_SECONDS - (monotonic() - sample_start)
        if remaining > 0:
            sleep(remaining)

sense.clear()
