# adhscounter – Verkabelung

## ESP32-S3 N16R8 ↔ ILI9488 3,5" + XPT2046 Touch (Standard-Gespann)

Gleiches Board/Display-Pärchen wie bei WetterCubePlus / OrgaCube.

### Display (ILI9488) – SPI2

| Display-Pin | ESP32-S3 GPIO | Hinweis |
|---|---|---|
| VCC | 3,3 V | Nicht 5V! |
| GND | GND | |
| CS | GPIO 15 | Chip Select Display |
| RST | GPIO 16 | Reset |
| DC / RS | GPIO 2 | Data/Command |
| MOSI / SDI | GPIO 13 | SPI Data |
| SCK / CLK | GPIO 14 | SPI Clock |
| LED / BL | GPIO 17 | Backlight (PWM) |
| MISO / SDO | GPIO 12 | (optional, nur für Lesebefehle) |

### Touch (XPT2046) – EIGENER SPI-Bus (SPI3)

| Touch-Pin | ESP32-S3 GPIO | Hinweis |
|---|---|---|
| T_CLK | GPIO 6 | eigener SPI3-Bus |
| T_DIN | GPIO 7 | Touch MOSI |
| T_DO  | GPIO 8 | Touch MISO |
| T_CS  | GPIO 21 | Chip Select |
| T_IRQ | GPIO 18 | nicht genutzt (Polling-Modus) |

## Audio (MAX98357A I2S-Verstärker + 4Ω/3W-Lautsprecher)

GPIO 1/4/5 waren beim Standard-Gespann bisher frei (ursprünglich für einen
inzwischen entfernten Analog-Joystick vorgesehen, siehe OrgaCube-Historie).

| MAX98357A-Pin | ESP32-S3 GPIO | Hinweis |
|---|---|---|
| VIN | 5V | volle ~3W Ausgangsleistung |
| GND | GND | gemeinsame Masse |
| BCLK | GPIO 1 | Bit Clock |
| LRC / WS | GPIO 4 | Word Select (L/R Clock) |
| DIN | GPIO 5 | I2S Datenleitung |
| SD | offen lassen | intern hochgezogen = Verstärker aktiv |
| GAIN | offen lassen | Standard-Verstärkung (9 dB) |

Lautsprecher (4Ω, 3W): beide Adern an die `+`/`-` Speaker-Ausgänge des
MAX98357A – niemals direkt an ESP32-Pins.

### Spannungsversorgung

| ESP32-S3 Pin | Anschluss |
|---|---|
| 5V / VIN | USB oder extern 5V |
| 3,3 V | Display VCC, Touch VCC |
| GND | gemeinsame Masse |

### Hinweise

- **GPIO 26–37** sind beim N16R8 intern für Flash/OPI-PSRAM belegt → nicht verwenden!
- **GPIO 19/20** sind USB D−/D+ → für normale I/O vermeiden
- **GPIO 0/3/45/46** sind Strapping-Pins (Bootverhalten) → für Inputs vermeiden bzw. nur mit Bedacht nutzen
- Das ILI9488-Modul hat meist einen integrierten 3,3V-Pegelwandler auf der Platine.
  Wenn das Display-Modul mit 5V arbeitet, trotzdem nur 3,3V an VCC anlegen, sofern kein
  separater 3,3V-Eingang vorhanden.

### Schematische Übersicht

```
ESP32-S3 N16R8
┌─────────────────────────────┐
│  GPIO 1  ────────── BCLK (MAX98357A)
│  GPIO 2  ────────── DC   (Display)
│  GPIO 4  ────────── LRC  (MAX98357A)
│  GPIO 5  ────────── DIN  (MAX98357A)
│  GPIO 6  ────────── CLK  (Touch)
│  GPIO 7  ────────── DIN  (Touch MOSI)
│  GPIO 8  ────────── DO   (Touch MISO)
│  GPIO 12 ────────── MISO (Display)
│  GPIO 13 ────────── MOSI (Display)
│  GPIO 14 ────────── SCLK (Display)
│  GPIO 15 ────────── CS   (Display)
│  GPIO 16 ────────── RST  (Display)
│  GPIO 17 ────────── BL   (Backlight)
│  GPIO 18 ────────── IRQ  (Touch, ungenutzt)
│  GPIO 21 ────────── CS   (Touch)
│  5V      ────────── VIN  (MAX98357A)
│  3.3V    ────────── VCC  (Display + Touch)
│  GND     ────────── GND  (alle)
└─────────────────────────────┘
```

## Arduino IDE Board-Einstellungen

Wie bei WetterCubePlus/OrgaCube:

| Einstellung | Wert |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16 MB |
| Partition Scheme | **Custom** (`partitions.csv` im Sketch-Ordner) |
