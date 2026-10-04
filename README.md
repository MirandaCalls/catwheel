# Cat Wheel

Tracks how far the cat runs on her wheel and reports it to Home Assistant.
A USB optical mouse reads the wheel's surface, and a Raspberry Pi Pico 2 W
turns the motion into distance and publishes it over MQTT.

Planned: dispense food once she's run far enough (see [Roadmap](#roadmap)).

```
[USB mouse] --GP16/GP17--> [Pico 2 W] --Wi-Fi/MQTT--> [Mosquitto] --> [Home Assistant]
```

## Hardware

- Raspberry Pi Pico 2 W
- Any wired USB optical mouse
- A USB-A female socket/breakout, or cut the mouse cable
- Optional: a strip of matte paper or tape where the mouse reads the wheel

### Wiring

The Pico's own micro-USB port stays free for power, flashing and the serial
monitor. The mouse is connected to a second USB host port that the firmware
emulates on two GPIO pins with [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB).

| Mouse wire (usual color) | Pico 2 W pin |
|---|---|
| 5V (red)      | VBUS (pin 40) |
| GND (black)   | GND (pin 38)  |
| D+ (green)    | GP16 (pin 21) |
| D- (white)    | GP17 (pin 22) |

Wire colors aren't guaranteed; check with a multimeter if in doubt. VBUS
carries 5V only when the Pico is powered from its micro-USB port, which is
the expected setup.

### Mounting the mouse

- Aim the sensor at the side face or rim of the wheel, 1-2 mm away and as
  close to touching as practical. Optical sensors lose tracking quickly
  further out.
- Glossy or clear plastic tracks poorly; stick matte paper where the sensor
  reads.
- Mount it where the wheel wobbles least (near the axle on the side face, or
  close to the stand on the rim).
- The firmware measures total movement (`sqrt(dx² + dy²)`), so the mouse
  doesn't need to be perfectly aligned with the direction of travel.

## Home Assistant setup

The Pico talks to Home Assistant through **MQTT**, a lightweight publish and
subscribe protocol. The Pico publishes readings to an MQTT *broker*, and Home
Assistant subscribes to them. **Mosquitto** is the broker. It isn't part of
Home Assistant itself but is available as an official add-on:

1. **Settings → Add-ons → Add-on Store → Mosquitto broker → Install**, then
   **Start**.
2. Home Assistant will offer to set up the discovered **MQTT** integration
   under **Settings → Devices & services**. Click **Configure → Submit**.
3. Create a login for the Pico. Mosquitto accepts Home Assistant users, so add
   one under **Settings → People → Users → Add user** (e.g. `catwheel`). You
   may need to enable *Advanced mode* in your user profile to see the Users
   tab.

After you flash the Pico, a **Cat Wheel** device appears automatically under
the MQTT integration (via MQTT discovery) with these entities:

| Entity | Description |
|---|---|
| Lifetime distance | Total meters run; persists across reboots |
| Speed | km/h, averaged over 2 s |
| Running | On while the wheel is moving |
| Mouse connected | Whether the mouse is detected |
| Raw mouse counts | Counts since boot, for calibration |

### Daily distance and graphs

- **Daily distance**: a Utility Meter helper, fed by the lifetime total, that
  resets at midnight. Install [`homeassistant/catwheel_package.yaml`](homeassistant/catwheel_package.yaml)
  as a package or create the same helper in the UI.
- **Dashboard**: paste [`homeassistant/dashboard_card.yaml`](homeassistant/dashboard_card.yaml)
  into a manual card. It shows today by hour, distance per day for the last
  30 days, and the lifetime total. These use Home Assistant's long-term
  statistics, so history goes back as long as the sensor has existed.

## Building and flashing

Uses [PlatformIO](https://platformio.org/) with the
[Arduino-Pico](https://github.com/earlephilhower/arduino-pico) core.

1. Copy `include/secrets.example.h` to `include/secrets.h` and fill in your
   Wi-Fi and MQTT details.
2. Hold **BOOTSEL** on the Pico while plugging it in, then:
   ```sh
   pio run -t upload
   ```
   Or copy `.pio/build/pico2w/firmware.uf2` onto the `RP2350` drive that
   appears.
3. Watch the output with `pio device monitor`.

## Calibration

The firmware needs to know how many mouse counts equal one meter of wheel
travel. This depends on the mouse, the surface and the mounting.

1. Measure the wheel's diameter at the point where the mouse reads it.
   On the side face, that's twice the distance from the axle to the sensor,
   not the outer diameter.
2. Put a piece of tape on the wheel as a marker. Note **Raw mouse counts**
   (in Home Assistant or the serial monitor).
3. Turn the wheel exactly 10 full turns by hand, at roughly cat speed.
4. Note the counts again, then calculate:
   ```
   COUNTS_PER_METER = (counts_after - counts_before) / (10 × π × diameter_m)
   ```
5. Set `COUNTS_PER_METER` in [`include/config.h`](include/config.h) and
   re-flash.

Repeat it a couple of times. If the results vary a lot, the mouse is probably
too far from the surface or the surface is too shiny.

## Roadmap

- [x] Read wheel distance with a USB mouse
- [x] Report to Home Assistant with daily and lifetime graphs
- [ ] Food dispenser: reward after a distance goal, with a daily treat cap and
      cooldown, and goal/dispense controls in Home Assistant
- [ ] 3D-printed mouse mount and enclosure
