# Entwicklungs-Log

## 2026-08-16 – v0.1.1-beta

### Erstes OTA-Update vorbereitet

`docs/firmware/` angelegt (Zielordner für den Arduino-IDE-Export von
`bootloader.bin`/`partitions.bin`/`boot_app0.bin`/`firmware.bin`), Version per
`bump_version.sh 0.1.0 0.1.1-beta` hochgezogen. Noch nicht getestet, ob der
komplette OTA-Zyklus (Check → Download → `Update.begin/writeStream/end` →
Neustart) auf echter Hardware sauber durchläuft – erster echter Test steht aus.

### Echter Alarm-Sound statt Synthese

Nutzer wollte einen "besseren" Ton als die Sinus/Glockenspiel-Synthese –
stattdessen eine echte WAV/MP3 eingebettet (`freesound_community-beep-beep-43875.mp3`,
3s). `ffmpeg` (per `brew install ffmpeg`, war lokal nicht vorhanden) nach
44,1kHz Mono 16-bit PCM gewandelt, dann per kleinem Python-Script
(`wave`/`struct`) als `const int16_t alarm_sound_pcm[]` in `alarm_sound.h`
eingebettet (~268KB Flash, unkritisch bei 16MB). Playback in Bloecken (`I2S_CHUNK`)
über I2S, Lautstaerke per Settings-Slider (`slidervol`, 0-100) skaliert.

**Falle:** Sample-Wiedergabe blockiert ca. 3s am Stück – ohne Gegenmassnahme
hätte "Antippen stoppt Alarm" bis zu 3s spät reagiert (LVGL-Touch-Events werden
nur über `lv_timer_handler()` erkannt, das waehrend der blockierenden
`i2s_write()`-Schleife nicht laeuft). Fix: alle ~130ms direkt `tft.getTouch()`
aufgerufen (an LVGL vorbei, roher LovyanGFX-Touch-Read) und bei Treffer sofort
abgebrochen. Gleiches Muster liesse sich auf andere lange blockierende Aktionen
übertragen, falls noetig.

Alte `playBellTone()`-Synthese-Funktion komplett entfernt (kein toter Code).

### Bug: Countdown & Alarm eingefroren im Portal-Modus

Nutzer meldete "Countdown zählt nicht mehr runter" nach dem Start. Root Cause:
`loop()` hatte einen fruehen `return` im `if (portal_modus)`-Zweig, der die
komplette Sekunden-Tick-Logik übersprang – **inklusive** der Pruefung
`if (remaining <= 0) enterAlarm();`. Sobald das Geraet aus irgendeinem Grund im
Captive-Portal-Fallback haengt (WLAN nicht verbunden), friert nicht nur die
Uhrzeit ein, sondern auch der komplette Countdown/Alarm-Mechanismus, obwohl der
button-Tap selbst (der synchron ueber LVGL-Events laeuft) weiterhin funktioniert
– dadurch wirkte es so, als waere "nur die Anzeige" kaputt.

**Lehre:** Zeitkritische Spiellogik sollte nie hinter einem fruehen
Netzwerk-Status-`return` in `loop()` versteckt werden. Fix: `portal_modus`
steuert nur noch, *ob* `dnsServer.processNextRequest()` zusaetzlich aufgerufen
wird, die restliche Loop-Logik (inkl. Sekunden-Tick) laeuft immer.

### Lautstaerke-Slider

`slidervol` (LVGL-Slider, Range 0-100, in PicoPixel vom Nutzer selbst
hinzugefuegt und exportiert) mit `cfg.volume` verknuepft, persistiert über
`Preferences`. `alarmVolume()` mappt 0-100 auf einen tatsaechlichen Gain
(0.0-0.9) fuer die I2S-Wiedergabe.

### Wiederkehrende Falle: Bild-Duplikate nach jedem PicoPixel-Export

Jeder neue PicoPixel-Export überschreibt `src/ui/images/*.c` wieder mit den
Original-`.c`-Dateien (statt der von `deploy_ui.sh` umbenannten `.inc`-Dateien)
→ Linker-Fehler "multiple definition" (Arduino kompiliert `.c`-Dateien in
Unterordnern automatisch mit, *und* `images.c` bindet sie zusätzlich per
`#include` ein). **Merke: nach jedem PicoPixel-Re-Export immer zuerst
`deploy_ui.sh` laufen lassen, bevor kompiliert wird** – ist im Session-Verlauf
mehrfach vergessen worden und hat jedes Mal zum gleichen Fehlerbild gefuehrt.

---

## 2026-08-16 – v0.1.0

### Hardware-Wahl: MAX98357A + 4Ω/3W-Lautsprecher

Kompatibilitaet bestaetigt (MAX98357A ist fuer genau diese Last ausgelegt,
5V-Versorgung fuer volle 3W). Erste Verkabelungs-Idee war das ESP32-2432S028R
(CYD) – dort aber zu wenige freie, unkritische GPIOs fuer I2S (BCLK/LRC/DIN)
gefunden (Backlight- und Input-only-Pin-Konflikte). Projekt stattdessen auf das
etablierte ESP32-S3-N16R8-Gespann (WetterCubePlus/OrgaCube) umgestellt – dort
waren GPIO 1/4/5 bereits als frei dokumentiert (ehemals fuer einen entfernten
Analog-Joystick vorgesehen).

### I2S-Audiotest: drei Iterationen bis zum sauberen Ton

1. Reiner Sinuston, Sample-fuer-Sample `i2s_write()` → "sehr schrill und laut".
2. Fade-In/Out ergaenzt, Lautstaerke reduziert → "immer noch rauschig". Ursache:
   einzelne `i2s_write()`-Aufrufe pro Sample (44.100×/s) sind zu langsam fuer
   den DMA-Puffer, fuehren zu Aussetzern.
3. Auf Block-Writes umgestellt (`I2S_CHUNK`-grosse Puffer statt Einzelsamples)
   → sauber. **Lehre fuer künftige I2S-Projekte: Audio-Samples immer in
   Bloecken schreiben, nie einzeln.**

### PicoPixel-MCP-Workflow etabliert

Neu fuer dieses Projekt (im Gegensatz zu SquareLine Studio bei
cyd_lichtsteuerung): UI-Design per MCP-Tools direkt im Browser-Projekt des
Nutzers, nicht mehr nur Export+Nachbearbeitung. Wichtige Erkenntnisse:

- **`letters`-Parameter bei `convert_font` ist ein Preset-Name** (`"standard"`,
  `"latin"` etc.), keine freie Zeichenliste! Eigene Zeichen (z.B. nur Ziffern)
  müssen über `custom_ranges` (Hex-Codepoints, z.B. `"0x30-0x39,0x3A"`) angegeben
  werden. Falsch benutzt fuehrte dazu, dass der Countdown-Font komplett ohne
  Ziffern gebunden war (`check_font_text_coverage` deckte es auf) – Symptom auf
  dem Geraet: `lv_draw_sw_letter: glyph dsc. not found for U+3X` im Serial-Log.
- **Label-Widgets defaulten auf `label_size_mode: "hug"`** (schrumpft auf
  Content-Groesse, Anker bleibt oben-links) – bei zentrierten, sich aendernden
  Texten (Countdown-Zahl) driftet das optisch aus der Mitte. Fix:
  `label_size_mode` explizit auf `"fixed"` setzen + feste Breite/Hoehe +
  `text_align: center`.
- Mutation-Operationen sind nach Kategorie getrennt zu senden (Screen-Ops,
  Resource-Prep wie Font/Icon, Struktur-Hierarchie-Ops jeweils in eigenen
  `lvgl_apply_changes`-Aufrufen) – gemischte Batches werden mit
  `lvgl.mutation.screen-or-project-change-must-be-separate` abgelehnt.
- Einzelne Satzzeichen wie `"-"`/`"+"` als Button-Text werden von PicoPixel als
  "Icon-artiger Text" geblockt (Schutz gegen Unicode-Symbol-Missbrauch). Fix:
  `"-5"`/`"+5"` statt nackter Vorzeichen.
- `revn` (Dokument-Revision) aendert sich auch durch reine User-Interaktion im
  Editor (Auswahl, Navigation) – vor jedem `lvgl_apply_changes`-Aufruf lieber
  frisch `get_context` abfragen statt die Revision manuell mitzuzaehlen.

### Farb-Bug: `LV_COLOR_16_SWAP` in `build_opt.h` vergessen

Nach Erst-Deploy auf echte Hardware: Farben komplett verdreht (Blau→Gruen).
`build_opt.h` mit `-DLV_COLOR_16_SWAP=1` (aus WetterCubePlus bekanntes Pflicht-Flag
fuer den SPI-Farbversand) war beim initialen Aufsetzen schlicht vergessen worden
– Kopiervorlage nicht vollstaendig übernommen. **Lehre: bei neuen Projekten nach
dem Standard-Gespann-Muster IMMER `build_opt.h` mit `LV_COLOR_16_SWAP=1` von
Anfang an mit anlegen, nicht erst bei Bug-Report nachtragen.**

### Minesweeper-Minispiel

Nach dem Bubblebreaker-Muster aus WetterCubePlus gebaut: eigene, programmatisch
angelegte Screens (`lv_obj_create(nullptr)`, nicht Teil des PicoPixel-Exports),
Spielfeld als eine `lv_canvas` mit PSRAM-Puffer statt 70 Einzel-Widgets, Highscore
per `Preferences` (eigener Namespace), ein Undo-Schritt per Board-Snapshot
(`memcpy`), Abbrechen-Button. 10×7-Feld, 10 Minen, fairer erster Klick (Minen
werden erst nach dem ersten Tap gesetzt, nie auf dem angetippten Feld/Nachbarn).
Zeit statt Punkte als Highscore-Metrik (aufsteigend sortiert, schnellste zuerst).

Ton-Zahlen im Spielfeld: `lv_canvas_draw_text()` mit vorhandenem
PicoPixel-Font (`font_montserrat_18`, per `#include "src/ui/fonts/fonts.h"`
direkt im `.ino` nutzbar) – kein eigener Canvas-Font noetig, kein PicoPixel-
Re-Export fuer das Minispiel selbst erforderlich.

**Layout-Falle:** Game-Over-Panel-Buttons zunaechst mit festen x-Koordinaten
positioniert – rechnerisch symmetrisch, aber durch fehlende explizite Breite auf
dem Beschreibungs-Label (Auto-Groesse konnte ueber den Panel-Rand hinauswachsen)
wirkte es auf dem Geraet trotzdem schief. Fix: `lv_obj_set_width()` auf dem
Label + Buttons per `lv_obj_align(..., LV_ALIGN_BOTTOM_LEFT/RIGHT, ...)` relativ
zum Panel statt absoluter Pixelwerte – garantiert symmetrisch unabhaengig von
Panel-/Textgroesse.

### Sonstiges

- Touch-Kalibrierung, Captive Portal, WebUI-Struktur, OTA-Handler 1:1 aus
  WetterCubePlus uebernommen (bewaehrtes Muster, siehe dortiges `WetterCubePlus.ino`).
- Kein Co-Author-Zusatz in Commits – explizite Nutzer-Vorgabe, gilt dauerhaft.
