# CYD Pin-Up Calendar

An old gas-station giveaway pin-up calendar, but on an ESP32-2432S028R "Cheap Yellow Display" in portrait mode.
You pick a calendar series (Dogs, Guns, Bikinis, and any you add). Each series has a separate edition for each year, with 12 pictures. The CYD downloads every edition over Wi-Fi to the microSD card and shows the month's picture above a calendar marked with US holidays. Years with no edition show a placeholder.

```
┌────────────────────┐
│                    │  picture for the month (240x170 window
│     pin-up art     │  of a 240x320 image, tap for full screen)
│   Miss November    │
├────────────────────┤
│ ◀  NOVEMBER 2026 ▶ │  tap arrows: prev/next month · tap title: big calendar · hold (dev): picker
│ S  M  T  W  T  F  S│
│ 1• 2  3• 4  5  6  7│  red pill = federal holiday / observed day off
│ 8  9 10 (11)12 13 14│  blue dot = observance (Halloween, Mother's Day…)
│ …                  │  box = today, tap a day to see its holidays
├────────────────────┤
│Thu Nov 26 Thanksgiv│  edition title, then the month's holidays · tap = back to today
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
2. **Wi-Fi.** Create `/calendar/config.txt` on the SD card:

   ```
   ssid=MyNetwork
   pass=MyPassword
   # optional:
   tz=CST6CDT,M3.2.0,M11.1.0
   catalog=https://raw.githubusercontent.com/rybbit/cyd-pinup-calendar/main/content/catalog.json
   ```

   `tz` is a POSIX time zone string. The default is US Eastern (`EST5EDT,M3.2.0,M11.1.0`). Use one of:
   `CST6CDT,M3.2.0,M11.1.0` (Central), `MST7MDT,M3.2.0,M11.1.0` (Mountain), `MST7` (Arizona), `PST8PDT,M3.2.0,M11.1.0` (Pacific), `AKST9AKDT,M3.2.0,M11.1.0` (Alaska), `HST10` (Hawaii).
   **Optional advertising block,** like the dealer imprint on an old giveaway calendar:

   ```
   organization=MODERN ENGINEERING CO.
   address1=3555 SCOTT AVENUE
   address2=ST. LOUIS, MO 63103
   contact=PHONE: JEFFERSON 3-3100
   motto=Everything For Welding
   ```

   Every line is optional and takes up space only when it's set; the picture grows to fill whatever is left. `company` works in place of `organization`, `contact` can hold a phone number, email or website (`phone` and `email` also work as the key), and `ad_`-prefixed names like `ad_company` work too. With none set, the page uses the classic layout.

   When set, the fields appear like this:
   - organization: large, bold
   - the two address lines: side by side, small
   - contact: medium, centered below the address
   - motto: navy italic under the calendar

   The calendar rows get a little shorter to make room. Long text drops to a smaller font automatically; keep the two address lines to about 38 characters combined. With a motto, tapping a day shows its holidays in the motto line for a few seconds. Without one, the bottom line cycles the edition title and holidays as usual.

   The boot log lists the values it read and flags any unrecognized key, which helps catch typos.

   **`env`** (optional, `dev` or `prod`; default prod) controls taps on the month name and the advertising block:

   | On the calendar page | env=prod | env=dev |
   |---|---|---|
   | Tap month name | big calendar | big calendar |
   | Tap advertising block | big calendar | big calendar |
   | Long press (~0.7 s) month name | same as a tap: big calendar | calendar picker |
   | Long press (~0.7 s) advertising block | same as a tap: big calendar | calendar picker |
   | Tap picture | full-screen picture | full-screen picture |

   A prod board has no way into the picker or SETUP, which suits a gift or a unit that shouldn't be changed. To change one, set `env=dev` in `config.txt` on the card.

   With `env=dev`, if organization, address1, address2 and contact are all blank, the block shows the project name `cyd-pinup-calendar` (in the large organization font) and the developer contact instead. These are set by `DEV_ORGANIZATION` / `DEV_CONTACT` in `platformio.ini`. With `env=prod`, an all-blank block takes no space.

   **Big calendar.** Tapping the month name shows the month filling the whole screen, with large numbers and the holidays cycling along the bottom. ◀ ▶ change month and tapping a day shows its holidays. Tap the month name or the bottom line to go back.

   **Editing on the device** (env=dev). Open the picker (long-press the month name or the advertising block) and tap **SETUP**. Every value is listed; tap one to change it:
   - **Wi-Fi:** scans nearby networks, then asks for the password. **TYPE NAME** handles hidden networks.
   - **Time zone:** pick a US zone, or **CUSTOM** for a POSIX string.
   - **Env:** choose dev or prod.
   - **Everything else:** an on-screen keyboard (ABC for capitals, #+= for symbols).

   **SAVE** rewrites `config.txt` in place, keeping your key spellings, comments and unknown lines, and applies the changes immediately: Wi-Fi reconnects, the clock's time zone updates, and the advertising layout redraws.

   When you edit the file on a PC instead, reset the board afterwards: it's read at boot. If Wi-Fi is down at boot, the board retries every 2 minutes on its own. The dot in the picker's title bar is green when it's online.
3. **Pick a calendar.** Tap the category bar to filter (All → Bikinis → Dogs → Guns → …), then tap a series. It downloads every year's edition with a progress bar, then opens to the current month.

After that, the calendar boots straight to the current month, and downloaded calendars work without Wi-Fi. Without Wi-Fi the clock isn't set, though: the ESP32 has no battery-backed clock.

## Adding calendars (the list grows from GitHub)

A calendar is a **series** with one **edition** per year. All content lives in `content/` and is served from GitHub's raw URLs:

```
content/catalog.json                              series list + a version per year
content/calendars/<series>/series.json            series title + category
content/calendars/<series>/placeholder.jpg        optional: shown for years with no edition
content/calendars/<series>/<year>/calendar.json   edition title, captions, crop per month
content/calendars/<series>/<year>/01.jpg … 12.jpg 240x320 baseline JPEG
```

The board mirrors this on the SD card under `/calendar/<series>/`. The firmware reads the catalog from `https://raw.githubusercontent.com/rybbit/cyd-pinup-calendar/main/content/catalog.json` (set in `platformio.ini`; override with `catalog=` in `config.txt`).

**Add a year's edition.** Use any 12 pictures; they're sorted by filename, January to December:

```sh
pip install pillow
python3 tools/build_calendar.py --series bikinis --year 2029 --title "Poolside Pals" \
    --images ~/Pictures/bikinis-2029/ --captions captions.txt --focus top
```

For a brand-new series, also pass `--series-title "Dog Pin-Ups" --category Dogs`. The script does the following:
- crops and resizes each picture to 240x320
- saves baseline JPEGs under 120 KB (the ESP32 decoder can't read progressive JPEGs)
- writes `calendar.json`
- rebuilds `catalog.json`

Commit and push. The board picks up changes at boot, on **SYNC**, and every 6 hours, and it downloads only the years and images that changed.

**Placeholder for missing years.** When you browse to a year the series has no edition for (say Bikinis 2026), the calendar still works and the picture area shows a placeholder. That's the series' `placeholder.jpg` if it has one, set with:

```sh
python3 tools/build_calendar.py --series bikinis --placeholder art.png
```

Otherwise the board draws a retro sunburst with the category, the year, and "COMING SOON".

**Fine-tuning.** The calendar page shows a 240x170 window of each picture. To change which part shows, set `"crop_y"` (0–150) for that month in the edition's `calendar.json`. Tapping the picture always shows the whole image. After editing any JSON by hand or swapping a jpg, run `python3 tools/build_calendar.py --restamp` before committing, so the board notices.

**Photos and per-month control.** For photo calendars, `--fit` picks how each full-screen picture is made:
- `crop`: fill the screen (pin-up art and portrait photos)
- `frame`: a bordered print on a sunburst (product shots)
- `blur`: the whole photo, with blurred edges filling the space
- `bars`: the whole photo, with dark bars
- `auto`: crop or frame, decided per picture

Years can start late (`--first-month 10`) or end early (`--partial`).

**Manifest.** For month-by-month choices, use a CSV manifest with the columns `year,month,image,fit,top,caption`:

```sh
python3 tools/build_calendar.py --series bwca --series-title "Boundary Waters" --category Outdoors \
    --manifest bwca.csv --quality 85 --titles 2027="Paddle, Portage, Repeat" 2028="North of Ordinary"
```

A `top` value (0–1 vertical focus: 0.5 is center, 0.1 is near the top) also writes `MM_top.jpg`, a separate 240x184 header crop. The calendar page shows the header and full screen shows the main picture, so a wide landscape can have a good crop in both places.

`tools/make_demo_calendars.py` generates placeholder-art editions for testing.

## Custom calendars on the SD card

Calendars copied onto the card are listed **first** in the picker with an **SD** badge, ahead of the GitHub calendars. They're never updated or deleted by the board:

```
/calendar/custom/<name>/<year>/01.jpg … 12.jpg     required: 240x320 baseline JPEGs
/calendar/custom/<name>/<year>/calendar.json       optional: edition title, captions
/calendar/custom/<name>/series.json                optional: title, category
/calendar/custom/<name>/placeholder.jpg            optional
```

Without `series.json`, the title comes from the folder name (`smith-family` → "Smith Family") and the category is "Custom". The pictures must be prepared, because the board can't decode full-size photos. The easiest way is to let the build script write straight to the card while it's mounted on your PC:

```sh
python3 tools/build_calendar.py --sd /media/$USER/CARD --series smith-family \
    --series-title "Smith Family" --year 2027 --title "Smith Family 2027" \
    --images ~/Pictures/family-2027/
```

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
| `src/store.cpp` | series/edition catalog, SD layout, custom calendars, incremental downloads |
| `src/net.cpp` | Wi-Fi, NTP, HTTPS → SD download |
| `src/settings.cpp` | `/calendar/config.txt` read/write + selected calendar (NVS) |
| `src/setup_ui.cpp` | on-device settings editor: list, keyboard, Wi-Fi scan, time zones |
| `src/touch.cpp` | bit-banged XPT2046 + 3-point calibration (saved in NVS) |
| `src/holidays.cpp` | US holiday engine (pure C++) |
| `tools/` | image/catalog builders |
