from csv import writer
from datetime import datetime, timezone
from math import hypot
from pathlib import Path
from statistics import mean, median, pstdev
from time import monotonic, sleep

import cv2
import numpy as np
from astro_pi_orbit import ISS
from picamzero import Camera
from sense_hat import SenseHat

COLLECTION_SECONDS = 8 * 60
TOTAL_SECONDS = 9 * 60
SAMPLE_SECONDS = 5
PHOTO_SECONDS = 12
MAX_PHOTOS = 38
RAW_SELECTIONS = 2
GSD_METRES_PER_PIXEL = 126.48
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
    earth_pixels = small[earth]
    channel_range = 0.0
    if earth_pixels.size:
        channel_range = float(np.mean(
            np.percentile(earth_pixels, 98, axis=0) - np.percentile(earth_pixels, 2, axis=0)
        ))
    result = {
        "sea": 100 * np.count_nonzero(sea) / count,
        "cloud": 100 * np.count_nonzero(cloud) / count,
        "land": 100 * np.count_nonzero(land) / count,
        "earth": 100 * count / earth.size,
        "colour": (round(mean_r), round(mean_g), round(mean_b)),
        "colour_range": channel_range,
    }
    result["score"] = result["earth"] * (1 - result["cloud"] / 100)
    return result


def estimate_speed(first_path, second_path, seconds):
    first = cv2.imread(str(first_path), cv2.IMREAD_GRAYSCALE)
    second = cv2.imread(str(second_path), cv2.IMREAD_GRAYSCALE)
    if first is None or second is None or seconds <= 0:
        return None
    scale = min(1.0, 1024 / first.shape[1])
    first = cv2.resize(first, None, fx=scale, fy=scale)
    second = cv2.resize(second, None, fx=scale, fy=scale)
    detector = cv2.ORB_create(nfeatures=1500)
    points1, descriptors1 = detector.detectAndCompute(first, None)
    points2, descriptors2 = detector.detectAndCompute(second, None)
    if descriptors1 is None or descriptors2 is None:
        return None
    pairs = cv2.BFMatcher(cv2.NORM_HAMMING).knnMatch(descriptors1, descriptors2, k=2)
    good = [best for best, other in pairs if best.distance < 0.7 * other.distance]
    if len(good) < 12:
        return None
    distances = [hypot(points2[m.trainIdx].pt[0] - points1[m.queryIdx].pt[0],
                       points2[m.trainIdx].pt[1] - points1[m.queryIdx].pt[1]) / scale
                 for m in good]
    pixels = median(distances)
    return pixels, pixels * GSD_METRES_PER_PIXEL / seconds / 1000, len(good)


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


def save_red_raw(photos):
    """Save the red channel of the most colour-diverse photos as raw bytes."""
    selected = sorted(photos, key=lambda item: item["colour_range"], reverse=True)[:RAW_SELECTIONS]
    rows = []
    for rank, item in enumerate(selected, 1):
        image = cv2.imread(str(item["path"]))
        if image is None:
            continue
        red = image[:, :, 2]
        raw_path = ROOT / f"red_channel_{rank:02d}.raw"
        raw_path.write_bytes(red.tobytes())
        rows.append((raw_path.name, item["path"].name, red.shape[1], red.shape[0],
                     "uint8", "row-major", round(item["colour_range"], 2)))
    with (ROOT / "red_channels.csv").open("w", newline="", encoding="utf-8") as output:
        data = writer(output)
        data.writerow(["raw_filename", "source_image", "width", "height", "data_type",
                       "layout", "colour_range_score"])
        data.writerows(rows)
    return rows


def create_report(samples, photos, speeds, raw_rows):
    def summary(key, unit):
        numbers = [sample[key] for sample in samples]
        return (f"{key.replace('_', ' ').title()}: mean {mean(numbers):.2f}{unit}, "
                f"range {min(numbers):.2f}-{max(numbers):.2f}{unit}, "
                f"variation {pstdev(numbers):.2f}{unit}.")

    lines = [
        "SPACE GARDEN AND COLOURS OF EARTH - AUTOMATIC REPORT",
        "",
        f"The experiment collected {len(samples)} sensor samples and {len(photos)} Earth images.",
        summary("temperature", " C"),
        summary("humidity", " %"),
        summary("pressure", " hPa"),
        summary("light", ""),
        summary("acceleration", " g"),
        "",
    ]
    if samples:
        stable = pstdev([sample["temperature"] for sample in samples]) < 0.5
        lines.append("The measured temperature was " + ("stable." if stable else "changing during the experiment."))
    if photos:
        lines.extend([
            f"Estimated mean image coverage: sea {mean(p['sea'] for p in photos):.1f}%, "
            f"cloud {mean(p['cloud'] for p in photos):.1f}%, land {mean(p['land'] for p in photos):.1f}%.",
            "These image classes are estimates based on colour and brightness, not confirmed labels.",
            "The clearest selected views are combined in earth_panorama.jpg.",
            "The sequence of average Earth colours is saved in colours_of_earth.png.",
            f"Red-only raw data from {len(raw_rows)} most colour-diverse images is described "
            "in red_channels.csv.",
            f"First photo position: {photos[0]['latitude']:.5f}, {photos[0]['longitude']:.5f}.",
            f"Last photo position: {photos[-1]['latitude']:.5f}, {photos[-1]['longitude']:.5f}.",
        ])
    else:
        lines.append("No Earth image was successfully analysed.")
    valid_speeds = [item["speed"] for item in speeds if item["accepted"]]
    if valid_speeds:
        lines.extend([
            "",
            f"Estimated ISS speed from {len(valid_speeds)} reliable image pairs: "
            f"{median(valid_speeds):.2f} km/s (median).",
            f"Mean accepted estimate: {mean(valid_speeds):.2f} km/s.",
            "The estimate uses ORB image features and an approximate ground sampling distance "
            f"of {GSD_METRES_PER_PIXEL:.2f} metres per full-resolution pixel.",
        ])
    else:
        lines.append("No reliable image pair was available for an ISS speed estimate.")
    lines.extend([
        "",
        "Conclusion: compare the sensor ranges with the images to investigate whether changes in "
        "light and station movement coincide with the observed colours of Earth.",
    ])
    (ROOT / "result.txt").write_text("\n".join(lines), encoding="utf-8")


sense, camera, iss = SenseHat(), Camera(), ISS()
sense.color.gain, sense.color.integration_cycles = 4, 64
sense.clear(0, 20, 0)
sensor_header = [
    "time_utc", "elapsed_s", "temperature_c", "humidity_percent", "pressure_hpa",
    "light_red", "light_green", "light_blue", "light_clear",
    "magnetic_x", "magnetic_y", "magnetic_z", "acceleration_x", "acceleration_y",
    "acceleration_z", "gyroscope_x", "gyroscope_y", "gyroscope_z", "pitch", "roll", "yaw",
]
photo_header = [
    "time_utc", "elapsed_s", "filename", "latitude", "longitude",
    "sea_percent", "cloud_percent",
    "land_percent", "earth_in_frame_percent", "average_red", "average_green",
    "average_blue", "quality_score",
    "colour_range_score",
]
speed_header = ["first_image", "second_image", "interval_s", "matches",
                "pixel_distance", "speed_km_s", "accepted"]
start, next_photo, photo_number = monotonic(), 0, 0
photos, samples, speeds = [], [], []

with (ROOT / "space_garden.csv").open("w", newline="", encoding="utf-8") as sensors, \
        (ROOT / "earth_analysis.csv").open("w", newline="", encoding="utf-8") as images, \
        (ROOT / "iss_speed.csv").open("w", newline="", encoding="utf-8") as speed_file:
    sensor_data, photo_data, speed_data = writer(sensors), writer(images), writer(speed_file)
    sensor_data.writerow(sensor_header)
    photo_data.writerow(photo_header)
    speed_data.writerow(speed_header)
    while monotonic() - start < COLLECTION_SECONDS:
        sample_start = monotonic()
        elapsed = sample_start - start
        red, green, blue, clear = sense.color.colour
        orientation = sense.get_orientation_degrees()
        now = datetime.now(timezone.utc).isoformat()
        temperature = round(sense.get_temperature(), 2)
        humidity = round(sense.get_humidity(), 2)
        pressure = round(sense.get_pressure(), 2)
        acceleration = xyz(sense.get_accelerometer_raw())
        light = (red + green + blue) / 3
        samples.append({"temperature": temperature, "humidity": humidity,
                        "pressure": pressure, "light": light,
                        "acceleration": sum(value * value for value in acceleration) ** 0.5})
        sensor_data.writerow([
            now, round(elapsed, 2), temperature, humidity, pressure,
            red, green, blue, clear, *xyz(sense.get_compass_raw()),
            *acceleration, *xyz(sense.get_gyroscope_raw()),
            round(orientation["pitch"], 2), round(orientation["roll"], 2),
            round(orientation["yaw"], 2),
        ])
        sensors.flush()

        if elapsed >= next_photo and photo_number < MAX_PHOTOS:
            photo_number += 1
            path = ROOT / f"earth_{photo_number:02d}.jpg"
            try:
                position = iss.coordinates()
                camera.take_photo(str(path))
                result = analyse(path)
                result.update(number=photo_number, path=path, elapsed=elapsed,
                              latitude=position.latitude.degrees,
                              longitude=position.longitude.degrees)
                if photos:
                    interval = elapsed - photos[-1]["elapsed"]
                    estimate = estimate_speed(photos[-1]["path"], path, interval)
                    if estimate:
                        pixels, speed, matches = estimate
                        accepted = 5 <= speed <= 10
                        speeds.append({"speed": speed, "accepted": accepted})
                        speed_data.writerow([photos[-1]["path"].name, path.name,
                                             round(interval, 2), matches, round(pixels, 2),
                                             round(speed, 3), accepted])
                        speed_file.flush()
                photos.append(result)
                photo_data.writerow([
                    now, round(elapsed, 2), path.name, round(result["latitude"], 6),
                    round(result["longitude"], 6), round(result["sea"], 2),
                    round(result["cloud"], 2), round(result["land"], 2),
                    round(result["earth"], 2), *result["colour"], round(result["score"], 2),
                    round(result["colour_range"], 2),
                ])
                images.flush()
            # Replay Online can raise a browser JsException outside Exception
            # when a historical image download temporarily fails.
            except BaseException:
                path.unlink(missing_ok=True)
            next_photo += PHOTO_SECONDS

        remaining = SAMPLE_SECONDS - (monotonic() - sample_start)
        if remaining > 0:
            sleep(remaining)

create_visuals(photos)
raw_rows = save_red_raw(photos)
create_report(samples, photos, speeds, raw_rows)
remaining = TOTAL_SECONDS - (monotonic() - start)
if remaining > 0:
    sleep(remaining)
sense.clear()
