# Cat Wheel

Tracks how far the cat runs on her wheel and reports it to Home Assistant.
A USB optical mouse reads the wheel's surface, and a Raspberry Pi Pico W (or Pico 2 W)
turns the motion into distance and publishes it over MQTT.

Planned: dispense food once she's run far enough (see [Roadmap](#roadmap)).

```
[USB mouse] --OTG adapter--> [Pico W] --Wi-Fi/MQTT--> [Mosquitto] --> [Home Assistant]
```

## Hardware

- Raspberry Pi **Pico W** or **Pico 2 W**. It must be a W model, because the
  plain Pico and Pico 2 have no Wi-Fi.
- Any basic wired USB optical mouse
- A **powered OTG Y-cable**: USB-A female for the mouse, a micro-USB male
  plug for the Pico (data + power), and a separate power plug. These are sold
  as "micro USB OTG cable with power", e.g. for Fire TV sticks.
- A 5V USB power supply (any phone charger)
- Optional: a USB-serial adapter or Raspberry Pi Debug Probe for logs
- Optional: a strip of matte paper or tape where the mouse reads the wheel

### Wiring

The Pico's micro-USB port runs as a **USB host** using the chip's built-in
USB controller. The powered OTG Y-cable does all the wiring:

```
[Mouse] --> USB-A female --+-- micro-USB plug --> Pico W (data + 5V)
                           |
            power plug ----+  <-- 5V USB charger
```

The charger's 5V feeds both the mouse and the Pico (through the cable's
micro-USB plug), so no soldering is needed.

Optional log adapter, for debugging:

| Connection | Pico pin |
|---|---|
| Log adapter RX | GP0 / UART0 TX (pin 1) |
| Log adapter GND | GND (pin 3) |

<details>
<summary>Without a Y-cable</summary>

Use a plain OTG adapter (micro-USB male to USB-A female) and wire a 5V
supply to VBUS (pin 40) and GND (pin 38). VBUS feeds the mouse through the
micro-USB port. **Never connect the Pico to a computer while that supply is
connected**, because both would drive the same 5V line.
</details>

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
Home Assistant itself but is available as an official add-on, which runs on
the same machine as Home Assistant. Add-ons need Home Assistant OS or a
Supervised install (check **Settings → System → About → Installation Type**).
On HA Container or Core, run Mosquitto yourself, e.g. the `eclipse-mosquitto`
Docker image, and add the MQTT integration pointing at it.

1. **Settings → Add-ons → Add-on Store → Mosquitto broker → Install**, then
   **Start**.
2. Home Assistant will offer to set up the discovered **MQTT** integration
   under **Settings → Devices & services**. Click **Configure → Submit**.
3. Create a login for the Pico. Mosquitto accepts Home Assistant users, so add
   one under **Settings → People → Users → Add user** (e.g. `catwheel`). You
   may need to enable *Advanced mode* in your user profile to see the Users
   tab.

Once the Pico is set up (see below), a **Cat Wheel** device appears automatically under
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

## Installing the firmware

Apart from the first install, everything below works from an iPad or phone.

### 1. Get the firmware

GitHub builds it automatically on every change (see
[`.github/workflows/build.yml`](.github/workflows/build.yml)):

- **Releases**: on the repo's **Releases** page, download the files for your
  board from the latest release.
- **Latest build**: open **Actions → Build firmware**, pick the newest run,
  and download **catwheel-firmware** at the bottom. It's a zip; unpack it.

Each board has its own files. Using the wrong one does nothing, and the Pico
just stays in BOOTSEL mode:

| Board | First install | Web updates |
|---|---|---|
| Pico W (RP2040) | `catwheel-picow.uf2` | `catwheel-picow.bin` |
| Pico 2 W (RP2350) | `catwheel-pico2w.uf2` | `catwheel-pico2w.bin` |

The firmware contains no passwords, so it's safe to build in a public repo.

### 2. First install (USB)

This one step needs a computer (Mac, Windows, Linux or Chromebook). The
iPad's Files app doesn't write to the Pico's drive in a way it accepts.

1. Hold **BOOTSEL** on the Pico while plugging it into the computer.
2. A drive appears: **RPI-RP2** for a Pico W, **RP2350** for a Pico 2 W.
   Drag your board's `.uf2` onto it. The drive disappears within a few
   seconds as the Pico restarts.
3. Unplug it and connect it to the Y-cable, mouse and charger.

### 3. Setup

1. On first boot the Pico creates an open Wi-Fi network called
   **CatWheel-Setup**. Join it from the iPad and the setup page opens by
   itself. If it doesn't, open `http://192.168.42.1/settings` in Safari.
2. Enter your Wi-Fi details, the MQTT broker (your Home Assistant address,
   e.g. `homeassistant.local`, and the `catwheel` user), and choose an
   **admin password**.
3. Tap **Save and restart**, then rejoin your home Wi-Fi.

The Pico now has a page at **http://catwheel.local/** showing distance, mouse
and Home Assistant status, with links to:

- **Settings**: change any of the above, or the calibration.
- **Update firmware**: pick your board's new `.bin` file to install it over Wi-Fi.

Both ask for a login: username `admin` and your admin password.

To get back to the setup network (e.g. after changing Wi-Fi), hold **BOOTSEL**
for 5 seconds while the Pico is running. It also starts the setup network on
its own if it can't join your Wi-Fi within 3 minutes of starting, and returns
to normal after 10 minutes if nobody uses the setup page.

### Building it yourself (optional)

With [PlatformIO](https://platformio.org/) on a computer, `pio run -e picow`
(or `-e pico2w`) builds the same firmware into `.pio/build/<board>/`.

### Logs (optional)

The USB port is busy with the mouse, so the firmware logs to UART0 at 115200
baud. Connect a USB-serial adapter's RX to GP0 (and GND to GND). The status
page and the Home Assistant entities cover most debugging.

## Calibration

The firmware needs to know how many mouse counts equal one meter of wheel
travel. This depends on the mouse, the surface and the mounting.

1. Measure the wheel's diameter at the point where the mouse reads it.
   On the side face, that's twice the distance from the axle to the sensor,
   not the outer diameter.
2. Put a piece of tape on the wheel as a marker. Note **Raw counts since
   boot** on http://catwheel.local/ (or **Raw mouse counts** in Home
   Assistant).
3. Turn the wheel exactly 10 full turns by hand, at roughly cat speed.
4. Reload the page, note the counts again, then calculate:
   ```
   counts per meter = (counts_after - counts_before) / (10 × π × diameter_m)
   ```
5. Enter it under **Settings → Calibration** and save.

Repeat it a couple of times. If the results vary a lot, the mouse is probably
too far from the surface or the surface is too shiny. Calibrate before the
cat starts using the wheel: distance already recorded isn't recalculated.

## Roadmap

- [x] Read wheel distance with a USB mouse
- [x] Report to Home Assistant with daily and lifetime graphs
- [ ] Food dispenser: reward after a distance goal, with a daily treat cap and
      cooldown, and goal/dispense controls in Home Assistant
- [ ] 3D-printed mouse mount and enclosure
