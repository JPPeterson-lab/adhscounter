# ⏱️ ADHS Counter

![Status](https://img.shields.io/badge/Status-Beta-yellow?style=flat-square)
![Version](https://img.shields.io/badge/Version-v0.1.3-blue?style=flat-square&cacheSeconds=1)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--S3%20N16R8-red?style=flat-square&logo=espressif&logoColor=white)
![Display](https://img.shields.io/badge/Display-ILI9488%203.5%22%20480×320-informational?style=flat-square)
![Lizenz](https://img.shields.io/badge/Lizenz-CC%20BY--NC%204.0-lightgrey?style=flat-square)
![Plattform](https://img.shields.io/badge/Platform-Arduino%20IDE-teal?style=flat-square&logo=arduino&logoColor=white)

Ein kompaktes WLAN-Countdown-Gerät auf Basis des **ESP32-S3 N16R8** mit 3,5"-Touchdisplay und I2S-Lautsprecher – für Zeitmanagement mit deutlichem akustischem und optischem Alarm.

---

## ✨ Funktionen

| | Funktion | Details |
|---|---|---|
| ⏳ | **3 Countdown-Timer** | Frei konfigurierbare Minutenwerte, direkt auf den Start-Buttons sichtbar; nur ein Timer gleichzeitig aktiv |
| 🔔 | **Alarm** | Sanfter Glockenspiel-Ton (kein schrilles Piepen) + rot blinkender Vollbild-Screen, stoppt erst durch Antippen |
| ⏹️ | **Stop-Button** | Laufenden Timer jederzeit abbrechen |
| ⚙️ | **Einstellungen** | Timerdauern per +/- Buttons direkt am Gerät änderbar |
| 🖥️ | **WebUI** | Status, Timer-Konfiguration, WLAN-Verwaltung und Firmware-Update unter `adhscounter.local` |
| 📡 | **Captive Portal** | WLAN-Ersteinrichtung ohne App, Hotspot „adhscounter-Setup" |
| 🔄 | **OTA-Update** | Firmware-Update per WebUI über WLAN |
| 👆 | **Touch-Kalibrierung** | Automatisch beim Erststart, per Finger-halten jederzeit neu auslösbar |
| 🎮 | **Mini-Spiel: Minesweeper** | 10×7-Feld, 10 Minen, fairer erster Klick, ein Schritt zurück (Undo), Abbrechen-Button, persistente Bestenliste (Top 5, schnellste Zeit) |

---

## 🎬 Screens

| # | Screen | Inhalt |
|---|---|---|
| 1 | **Home** | Datum, Uhrzeit, 3 Start-Buttons, große Countdown-Anzeige, Zugang zu Einstellungen & Minesweeper |
| 2 | **Einstellungen** | Timerdauern (+/- je Timer), Speichern/Zurück |
| 3 | **Alarm** | Rot blinkender Vollbild-Hinweis, Ton läuft bis zum Antippen |
| 4 | **Minesweeper – Start** | Bestenliste (Top 5), Start/Zurück |
| 5 | **Minesweeper – Spiel** | Spielfeld, Zeit, verbleibende Minen, Undo, Abbrechen |

---

## 🔧 Erstkonfiguration

1. Gerät startet – **Touch-Kalibrierung**: 4 Kreuzmarkierungen nacheinander antippen
2. WLAN-Hotspot **„adhscounter-Setup"** verbinden (Smartphone oder PC)
3. Browser öffnet Portal automatisch – alternativ `192.168.4.1`
4. WLAN-Zugangsdaten eingeben, Speichern & Neustart

Nach dem Neustart erreichbar unter **`http://adhscounter.local`** oder der im Settings-Screen angezeigten IP.

### 🖱️ Touch neu kalibrieren

Beim Booten Finger auf das Display halten bis „Neukalibrierung" erscheint, dann loslassen.

---

## ⚙️ Web-Konfiguration

Unter `http://adhscounter.local` (oder der IP-Adresse):

- **Status**: aktueller Timer-Zustand / Restzeit
- **Timer-Dauer**: 3 Minutenwerte, synchron mit den Einstellungen am Gerät
- **WLAN**: Netzwerk wechseln
- **Firmware-Update**: Auf Updates prüfen & installieren (OTA)

---

## 🛠️ Installation (Arduino IDE)

**Arduino IDE Board-Einstellungen:**

| Einstellung | Wert |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16 MB |
| Partition Scheme | **Custom** (`partitions.csv` im Sketch-Ordner) |

Benötigte Bibliotheken: `LovyanGFX`, `lvgl` (8.x)

1. Repo klonen / Sketch-Ordner öffnen
2. Board-Einstellungen wie oben setzen
3. Hochladen

---

## 🔌 Hardware & Verkabelung

- **MCU:** ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI-PSRAM)
- **Display:** ILI9488 3,5" 480×320 + XPT2046 Touch
- **Audio:** MAX98357A I2S-Verstärker + 4 Ω / 3 W Lautsprecher

Vollständige Pinbelegung siehe [WIRING.md](WIRING.md).

---

## 📄 Lizenz

Dieses Projekt steht unter der [CC BY-NC 4.0 Lizenz](LICENSE). Drittanbieter-Komponenten (LVGL, LovyanGFX, Schriften, Icons) siehe [CREDITS.md](CREDITS.md).
