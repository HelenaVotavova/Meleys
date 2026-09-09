from sense_hat import SenseHat
from time import sleep


sense = SenseHat()
sense.set_rotation(270)

# Read the colour sensor and make the measured colour suitable for the LEDs.
sense.color.gain = 60
sense.color.integration_cycles = 64
sleep(2 * sense.color.integration_time)
red, green, blue, clear = sense.color.colour
background = (
    min(red, 80),
    min(green, 80),
    min(blue, 80),
)

yellow = (255, 210, 0)
white = (255, 255, 255)
black = (0, 0, 0)

# A small rocket. Replace pixels to make the picture your own.
rocket = [
    background, background, background, white, white, background, background, background,
    background, background, white, white, white, white, background, background,
    background, background, white, black, black, white, background, background,
    background, yellow, white, white, white, white, yellow, background,
    background, yellow, white, white, white, white, yellow, background,
    background, background, white, white, white, white, background, background,
    background, background, yellow, white, white, yellow, background, background,
    background, yellow, background, yellow, yellow, background, yellow, background,
]

sense.set_pixels(rocket)
sleep(5)
sense.show_message("AHOJ ISS", scroll_speed=0.08, text_colour=white, back_colour=background)
sense.clear()
