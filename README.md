# CYD Pin-Up Calendar

An old gas-station giveaway pin-up calendar, but on an ESP32-2432S028R "Cheap Yellow Display" in portrait mode.
You pick a calendar (Dogs, Guns, Bikinis, and any you add). The CYD downloads its 12 pictures over Wi-Fi to the microSD card and shows the month's picture above a calendar marked with US holidays.

```
┌────────────────────┐
│                    │  picture for the month (240x170 window
│     pin-up art     │  of a 240x320 image, tap for full screen)
│   Miss November    │
├────────────────────┤
│ ◀  NOVEMBER 2026 ▶ │  tap arrows: prev/next month · tap title: calendar picker
│ S  M  T  W  T  F  S│
│ 1• 2  3• 4  5  6  7│  red pill = federal holiday / observed day off
│ 8  9 10 (11)12 13 14│  blue dot = observance (Halloween, Mother's Day…)
│ …                  │  box = today, tap a day to see its holidays
├────────────────────┤
│Thu Nov 26 Thanksgiv│  rotates through the month's holidays · tap = back to today
└────────────────────┘
```

## Hardware

ESP32-2432S028R (2.8" 240x320, XPT2046 resistive touch, microSD slot).

| Part    | Pins |
|---------|------|
| Display | HSPI: SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, backlight 21 |
| Touch   | bit-banged: CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
| microSD | VSPI: SCK 18, MISO 19, MOSI 23, CS 5 |

The touch controller is driven in software (bit-banged), so it never has to share the VSPI bus with the SD card. That bus sharing is the usual trouble spot on CYD projects.

There are two display variants of this board. `platformio.ini` has an environment for each:

* `cyd2usb` (default): newer boards with USB-C, ST7789 controller
* `cyd`: original micro-USB-only boards, ILI9341 controller

If the screen stays white or shows garbage after flashing, flash the other one. If colors look like a photo negative, change `TFT_INVERSION_OFF` to `TFT_INVERSION_ON`. If red and blue are swapped, add `-DTFT_RGB_ORDER=TFT_BGR`.

## Build & flash

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

```sh
pio run -e cyd2usb -t upload && pio device monitor
```

You need a FAT32 microSD card. Calendars are stored in `/calendar/<calendar-id>/`.

## First boot

1. **Touch calibration.** Tap the three crosshairs with the stylus. To redo it later, hold the screen while "Hold screen to recalibrate" is shown at power-up.
2. **Wi-Fi.** On your phone, join the `PinUp-Calendar` network and open `192.168.4.1`. Pick your Wi-Fi network. The same page has two more fields:
   * **Calendar catalog URL**: where the list of calendars lives (see below).
   * **Time zone**: a POSIX TZ string. Defaults to US Eastern. Use one of:
     `CST6CDT,M3.2.0,M11.1.0` (Central), `MST7MDT,M3.2.0,M11.1.0` (Mountain), `MST7` (Arizona), `PST8PDT,M3.2.0,M11.1.0` (Pacific), `AKST9AKDT,M3.2.0,M11.1.0` (Alaska), `HST10` (Hawaii).
3. **Pick a calendar.** Tap the category bar to filter (All → Bikinis → Dogs → Guns → …), then tap a calendar. It downloads all 12 images with a progress bar, then opens to the current month.

After that, the calendar boots straight to the current month, and downloaded calendars work without Wi-Fi. Without Wi-Fi the clock isn't set, though: the ESP32 has no battery-backed clock.

## Adding calendars (the list grows from GitHub)

All content lives in `content/` and is served from GitHub's raw URLs:

```
content/catalog.json                     list: id, title, category, year
content/calendars/<id>/calendar.json     captions + crop per month
content/calendars/<id>/01.jpg … 12.jpg   240x320 baseline JPEG
```

1. Push this repo to GitHub as `cyd-pinup-calendar`. Then replace `YOUR_GITHUB_USER` in `platformio.ini` (or paste the URL into the Wi-Fi portal):
   `https://raw.githubusercontent.com/<you>/cyd-pinup-calendar/main/content/catalog.json`
2. Build a calendar from any 12 pictures (they're sorted by filename, January to December):

   ```sh
   pip install pillow
   python3 tools/build_calendar.py --id dogs-2028 --title "Good Boys & Girls" \
       --category Dogs --year 2028 --images ~/Pictures/dogs-2028/ \
       --captions ~/Pictures/dogs-2028/captions.txt --focus top
   ```

   The script crops and resizes to 240x320, saves baseline JPEGs (the ESP32 decoder can't read progressive JPEGs), keeps each image under 120 KB, writes `calendar.json`, and adds the calendar to `catalog.json`.
3. Commit and push. On the CYD, open the picker and tap **SYNC**. The new calendar shows up.

A new category is just a new `--category` value; the picker builds its category list from the catalog.

The calendar page shows a 240x170 window of each picture. To change which part shows, set `"crop_y"` (0–150) for that month in `calendar.json`. Tapping the picture always shows the whole image.

`tools/make_demo_calendars.py` generates placeholder Dogs/Guns/Bikinis calendars, which are already in `content/`. Use them to test the whole pipeline, then replace them with real art by running `build_calendar.py` with the same `--id`.

## US holidays

Calculated on the device for any year, in `src/holidays.cpp`:

* **Federal (red):** New Year's, MLK Day, Presidents' Day, Memorial Day, Juneteenth, Independence Day, Labor Day, Columbus Day, Veterans Day, Thanksgiving, Christmas. Includes the weekend "observed" rules: a Saturday holiday moves to Friday, and a Sunday holiday moves to Monday. When New Year's falls on a Saturday, it's observed on Friday, Dec 31.
* **Observances (blue dot):** Groundhog Day, Valentine's, DST start/end, St. Patrick's, Easter, Tax Day, Mother's Day, Flag Day, Father's Day, Halloween, Election Day, Christmas Eve, New Year's Eve.

Host test (no hardware needed):

```sh
g++ -std=c++11 -I src test/host/test_holidays.cpp src/holidays.cpp -o /tmp/th && /tmp/th
```

## Code map

| File | What |
|------|------|
| `src/main.cpp` | boot sequence, screens, tap handling |
| `src/ui.cpp` | drawing: calendar page, full-screen picture, picker, progress |
| `src/store.cpp` | catalog + SD layout + calendar download |
| `src/net.cpp` | Wi-Fi (WiFiManager portal), NTP, HTTPS → SD download |
| `src/touch.cpp` | bit-banged XPT2046 + 3-point calibration (saved in NVS) |
| `src/holidays.cpp` | US holiday engine (pure C++) |
| `tools/` | image/catalog builders |
