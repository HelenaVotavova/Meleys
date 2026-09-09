from csv import writer
from datetime import datetime, timezone
from pathlib import Path
from time import monotonic, sleep

import cv2
import numpy as np
from picamzero import Camera
from sense_hat import SenseHat

RUN_SECONDS, SAMPLE_SECONDS, PHOTO_SECONDS = 510, 5, 30
ROOT = Path(__file__).parent


def xyz(vector):
    return tuple(round(vector[key], 4) for key in "xyz")


def analyse(path):
    image = cv2.imread(str(path))
    if image is None:
        raise ValueError("Captured image could not be opened")
    scale = min(1.0, 640 / image.shape[1])
    small = cv2.resize(image, None, fx=scale, fy=scale)
    blue, green, red = cv2.split(small)
    hsv = cv2.cvtColor(small, cv2.COLOR_BGR2HSV)
    saturation, brightness = hsv[:, :, 1], hsv[:, :, 2]
    earth = brightness > 25
    cloud = earth & (brightness > 170) & (saturation < 70)
    sea = earth & ~cloud & (blue > red * 1.12) & (blue > green * 1.03)
    land = earth & ~cloud & ~sea
    count = max(1, np.count_nonzero(earth))
    mean_b, mean_g, mean_r = cv2.mean(small, mask=earth.astype(np.uint8))[:3]
    result = {
        "sea": 100 * np.count_nonzero(sea) / count,
        "cloud": 100 * np.count_nonzero(cloud) / count,
        "land": 100 * np.count_nonzero(land) / count,
        "earth": 100 * count / earth.size,
        "colour": (round(mean_r), round(mean_g), round(mean_b)),
    }
    result["score"] = result["earth"] * (1 - result["cloud"] / 100)
    return result


def create_visuals(photos):
    if not photos:
        return
    strip = np.zeros((100, 80 * len(photos), 3), dtype=np.uint8)
    for index, item in enumerate(photos):
        red, green, blue = item["colour"]
        strip[:, index * 80:(index + 1) * 80] = (blue, green, red)
    cv2.imwrite(str(ROOT / "colours_of_earth.png"), strip)

    best = sorted(photos, key=lambda item: item["score"], reverse=True)[:6]
    best.sort(key=lambda item: item["number"])
    tiles = []
    for item in best:
        image = cv2.imread(str(item["path"]))
        width = round(image.shape[1] * 240 / image.shape[0])
        tiles.append(cv2.resize(image, (width, 240)))
    cv2.imwrite(str(ROOT / "earth_panorama.jpg"), cv2.hconcat(tiles))


sense, camera = SenseHat(), Camera()
sense.color.gain, sense.color.integration_cycles = 4, 64
sense.clear(0, 20, 0)
sensor_header = [
    "time_utc", "elapsed_s", "temperature_c", "humidity_percent", "pressure_hpa",
    "light_red", "light_green", "light_blue", "light_clear",
    "magnetic_x", "magnetic_y", "magnetic_z", "acceleration_x", "acceleration_y",
    "acceleration_z", "gyroscope_x", "gyroscope_y", "gyroscope_z", "pitch", "roll", "yaw",
]
photo_header = [
    "time_utc", "elapsed_s", "filename", "sea_percent", "cloud_percent",
    "land_percent", "earth_in_frame_percent", "average_red", "average_green",
    "average_blue", "quality_score",
]
start, next_photo, photo_number, photos = monotonic(), 0, 0, []

with (ROOT / "space_garden.csv").open("w", newline="", encoding="utf-8") as sensors, \
        (ROOT / "earth_analysis.csv").open("w", newline="", encoding="utf-8") as images:
    sensor_data, photo_data = writer(sensors), writer(images)
    sensor_data.writerow(sensor_header)
    photo_data.writerow(photo_header)
    while monotonic() - start < RUN_SECONDS:
        sample_start = monotonic()
        elapsed = sample_start - start
        red, green, blue, clear = sense.color.colour
        orientation = sense.get_orientation_degrees()
        now = datetime.now(timezone.utc).isoformat()
        sensor_data.writerow([
            now, round(elapsed, 2), round(sense.get_temperature(), 2),
            round(sense.get_humidity(), 2), round(sense.get_pressure(), 2),
            red, green, blue, clear, *xyz(sense.get_compass_raw()),
            *xyz(sense.get_accelerometer_raw()), *xyz(sense.get_gyroscope_raw()),
            round(orientation["pitch"], 2), round(orientation["roll"], 2),
            round(orientation["yaw"], 2),
        ])
        sensors.flush()

        if elapsed >= next_photo and photo_number < 18:
            photo_number += 1
            path = ROOT / f"earth_{photo_number:02d}.jpg"
            try:
                camera.take_photo(str(path))
                result = analyse(path)
                result.update(number=photo_number, path=path)
                photos.append(result)
                photo_data.writerow([
                    now, round(elapsed, 2), path.name, round(result["sea"], 2),
                    round(result["cloud"], 2), round(result["land"], 2),
                    round(result["earth"], 2), *result["colour"], round(result["score"], 2),
                ])
                images.flush()
            except (OSError, ValueError, cv2.error):
                path.unlink(missing_ok=True)
            next_photo += PHOTO_SECONDS

        remaining = SAMPLE_SECONDS - (monotonic() - sample_start)
        if remaining > 0:
            sleep(remaining)

create_visuals(photos)
sense.clear()
