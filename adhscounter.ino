// ============================================================
//  adhscounter.ino
//  ESP32-S3 N16R8 | ILI9488 3.5" 480x320 | XPT2046 Touch
//  LVGL 8.x | LovyanGFX | PicoPixel-UI | MAX98357A I2S-Alarm
//  WebUI | Captive Portal | OTA
// ============================================================

#include "webui_html.h"
#include "alarm_sound.h"

// ---- Versions-Define ----
#define FIRMWARE_VERSION "0.1.1-beta"
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/JPPeterson-lab/adhscounter/main/docs/version.json"
#define OTA_BIN_URL      "https://jppeterson-lab.github.io/adhscounter/firmware/firmware.bin"
#define MDNS_NAME        "adhscounter"

// ============================================================
//  Pin-Definitionen (siehe WIRING.md)
// ============================================================
// -- Display SPI (SPI2_HOST) --
#define TFT_SCLK  14
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_DC     2
#define TFT_CS    15
#define TFT_RST   16
#define TFT_BL    17

// -- Touch XPT2046 (eigener SPI-Bus) --
#define TOUCH_CS   21
#define TOUCH_IRQ  18
#define TOUCH_CLK   6
#define TOUCH_MOSI  7
#define TOUCH_MISO  8

// -- I2S / MAX98357A --
#define I2S_BCLK   1
#define I2S_LRC    4
#define I2S_DOUT   5
#define I2S_PORT   I2S_NUM_0
#define I2S_SAMPLE_RATE 44100
#define I2S_VOLUME 0.35f

// -- Display-Auflösung --
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// ============================================================
//  Includes
// ============================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "src/ui/ui.h"
#include "src/ui/fonts/fonts.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include <driver/i2s.h>
#include <math.h>

// ============================================================
//  LovyanGFX – ILI9488 + XPT2046
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488  _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Touch_XPT2046  _touch;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.pin_sclk    = TFT_SCLK;
      cfg.pin_mosi    = TFT_MOSI;
      cfg.pin_miso    = TFT_MISO;
      cfg.pin_dc      = TFT_DC;
      cfg.freq_write  = 40000000;
      cfg.freq_read   =  8000000;
      _bus.config(cfg);
      _panel.setBus(&_bus); }

    { auto cfg = _panel.config();
      cfg.pin_cs    = TFT_CS;
      cfg.pin_rst   = TFT_RST;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.rgb_order = false;  // ILI9488 = BGR; wenn Farben invertiert -> true
      cfg.invert    = false;
      _panel.config(cfg); }

    { auto cfg = _touch.config();
      cfg.spi_host   = SPI3_HOST;   // eigener SPI-Host fuer Touch
      cfg.pin_sclk   = TOUCH_CLK;
      cfg.pin_mosi   = TOUCH_MOSI;
      cfg.pin_miso   = TOUCH_MISO;
      cfg.pin_cs     = TOUCH_CS;
      cfg.pin_int    = -1;          // Polling
      cfg.freq       = 1000000;
      cfg.x_min      = 300;
      cfg.x_max      = 3800;
      cfg.y_min      = 300;
      cfg.y_max      = 3800;
      cfg.bus_shared = false;
      _touch.config(cfg);
      _panel.setTouch(&_touch); }

    setPanel(&_panel);
  }
};

LGFX tft;

// ============================================================
//  Globale Objekte
// ============================================================
Preferences  prefs;
WebServer    server(80);
DNSServer    dnsServer;
bool         portal_modus = false;

// ============================================================
//  Konfiguration (aus Preferences geladen)
// ============================================================
struct Config {
  String ssid;
  String pass;
  int    dauer1 = 5;
  int    dauer2 = 15;
  int    dauer3 = 25;
  int    volume = 60;   // 0-100, steuert den Alarmton
};
Config cfg;

void ladeCfg() {
  prefs.begin("adhs", true);
  cfg.ssid   = prefs.getString("ssid", "");
  cfg.pass   = prefs.getString("pass", "");
  cfg.dauer1 = prefs.getInt("dauer1", 5);
  cfg.dauer2 = prefs.getInt("dauer2", 15);
  cfg.dauer3 = prefs.getInt("dauer3", 25);
  cfg.volume = prefs.getInt("volume", 60);
  prefs.end();
}

void speichereCfg() {
  prefs.begin("adhs", false);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putInt("dauer1", cfg.dauer1);
  prefs.putInt("dauer2", cfg.dauer2);
  prefs.putInt("dauer3", cfg.dauer3);
  prefs.putInt("volume", cfg.volume);
  prefs.end();
}

// ============================================================
//  LVGL-Grundsetup
// ============================================================
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lvgl_buf1 = nullptr;
static lv_color_t* lvgl_buf2 = nullptr;
#define LVGL_BUF_LINES 40

static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((lgfx::swap565_t*)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  static uint16_t lx = 0, ly = 0;
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = lx = x;
    data->point.y = ly = y;
  } else {
    data->state   = LV_INDEV_STATE_REL;
    data->point.x = lx;
    data->point.y = ly;
  }
}

void lvgl_flush(uint32_t ms = 80) {
  uint32_t t = millis();
  while (millis() - t < ms) { lv_timer_handler(); delay(5); }
}

// ============================================================
//  Touch-Kalibrierung (1:1 aus WetterCubePlus uebernommen)
// ============================================================
void touchKalibriereJetzt(uint16_t* calData) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println("Touch-Kalibrierung");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 55);
  tft.println("Bitte nacheinander die");
  tft.setCursor(10, 70);
  tft.println("4 Kreuzmarkierungen antippen.");
  delay(1200);
  tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 15);
  prefs.putBytes("data", calData, 8 * sizeof(uint16_t));
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, TFT_HEIGHT / 2 - 10);
  tft.println("Kalibriert! OK");
  delay(800);
}

void touchKalibrierung() {
  prefs.begin("touch_cal", false);
  bool calSaved = (prefs.getBytesLength("data") == 8 * sizeof(uint16_t));
  uint16_t calData[8];

  if (calSaved) {
    prefs.getBytes("data", calData, sizeof(calData));
    tft.setTouchCalibrate(calData);
    Serial.println("[Touch] Kalibrierung geladen");

    // 2,5 s Fenster: Finger halten -> Neukalibrierung
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, TFT_HEIGHT / 2 - 12);
    tft.println("Finger halten =");
    tft.setCursor(10, TFT_HEIGHT / 2 + 12);
    tft.println("Neukalibrierung");

    bool forceRecal = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 2500) {
      uint16_t tx, ty;
      if (tft.getTouch(&tx, &ty)) { forceRecal = true; break; }
      delay(30);
    }
    if (forceRecal) {
      touchKalibriereJetzt(calData);
      Serial.println("[Touch] Neukalibrierung gespeichert");
    }
  } else {
    Serial.println("[Touch] Erststart - Kalibrierung noetig");
    touchKalibriereJetzt(calData);
    Serial.println("[Touch] Erstkalibrierung gespeichert");
  }

  prefs.end();
}

// ============================================================
//  I2S / MAX98357A Audio
// ============================================================
void i2sInit() {
  i2s_config_t i2s_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
  };
  i2s_driver_install(I2S_PORT, &i2s_cfg, 0, nullptr);

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(I2S_PORT, &pins);
}

// I2S-Schreibvorgaenge in Bloecken statt einzeln - einzelne i2s_write()-Aufrufe
// pro Sample sind zu langsam fuer 44.1kHz und fuehren zu DMA-Aussetzern (Rauschen).
#define I2S_CHUNK 256

// cfg.volume (0-100, per Slider auf dem Settings-Screen) -> tatsaechlicher Gain (0.0-0.9)
float alarmVolume() {
  return (cfg.volume / 100.0f) * 0.9f;
}

volatile bool alarmDismissFlag = false;

// Eingebetteter Alarm-Sample (44,1kHz mono PCM), in Bloecken ueber I2S ausgegeben.
// Prueft alle paar Bloecke direkt per tft.getTouch() (an LVGL vorbei, da hier
// kein lv_timer_handler() laeuft) auf Touch, damit "Antippen stoppt Alarm"
// auch waehrend der ~3s-Sample-Wiedergabe sofort reagiert statt erst danach.
void playAlarmChime() {
  float vol = alarmVolume();
  int16_t chunk[I2S_CHUNK * 2];
  size_t written;
  uint32_t i = 0;
  int chunkCount = 0;
  while (i < ALARM_SOUND_SAMPLES) {
    if (++chunkCount >= 20) {
      chunkCount = 0;
      uint16_t tx, ty;
      if (tft.getTouch(&tx, &ty)) { alarmDismissFlag = true; return; }
    }
    uint32_t n = min((uint32_t)I2S_CHUNK, ALARM_SOUND_SAMPLES - i);
    for (uint32_t k = 0; k < n; k++) {
      int16_t v = (int16_t)(alarm_sound_pcm[i + k] * vol);
      chunk[k * 2]     = v;
      chunk[k * 2 + 1] = v;
    }
    i2s_write(I2S_PORT, chunk, n * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    i += n;
  }
}

// ============================================================
//  Countdown-Zustandsmaschine
// ============================================================
enum CounterState { STATE_IDLE, STATE_RUNNING, STATE_ALARM };
CounterState state = STATE_IDLE;
int      activeTimerIdx = -1;
uint32_t countdownEndMillis = 0;
int      letzteAngezeigteSekunde = -1;

int getDauerMinuten(int idx) {
  if (idx == 0) return cfg.dauer1;
  if (idx == 1) return cfg.dauer2;
  return cfg.dauer3;
}

void updateHomeButtonLabels() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer1);
  if (objects.buttonstart1_label) lv_label_set_text(objects.buttonstart1_label, buf);
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer2);
  if (objects.buttonstart2_label) lv_label_set_text(objects.buttonstart2_label, buf);
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer3);
  if (objects.buttonstart3_label) lv_label_set_text(objects.buttonstart3_label, buf);
}

void updateSettingsValueLabels() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer1);
  if (objects.labeldauer1value) lv_label_set_text(objects.labeldauer1value, buf);
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer2);
  if (objects.labeldauer2value) lv_label_set_text(objects.labeldauer2value, buf);
  snprintf(buf, sizeof(buf), "%d min", cfg.dauer3);
  if (objects.labeldauer3value) lv_label_set_text(objects.labeldauer3value, buf);

  if (objects.labelversion) lv_label_set_text(objects.labelversion, FIRMWARE_VERSION);
  if (objects.labelip) lv_label_set_text(objects.labelip, WiFi.localIP().toString().c_str());

  if (objects.slidervol) lv_slider_set_value(objects.slidervol, cfg.volume, LV_ANIM_OFF);
  if (objects.labelvol) lv_label_set_text_fmt(objects.labelvol, "Vol %d%%", cfg.volume);
}

void setCountdownLabel(long remainingSec) {
  char buf[16];
  if (remainingSec < 0) remainingSec = 0;
  int mm = remainingSec / 60;
  int ss = remainingSec % 60;
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  if (objects.labelcountdown) lv_label_set_text(objects.labelcountdown, buf);
}

// Bei laufendem Timer: Start-Buttons ausblenden, Stop-Button einblenden (und umgekehrt).
void zeigeStartButtons(bool zeigen) {
  if (objects.buttonstart1) { if (zeigen) lv_obj_clear_flag(objects.buttonstart1, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(objects.buttonstart1, LV_OBJ_FLAG_HIDDEN); }
  if (objects.buttonstart2) { if (zeigen) lv_obj_clear_flag(objects.buttonstart2, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(objects.buttonstart2, LV_OBJ_FLAG_HIDDEN); }
  if (objects.buttonstart3) { if (zeigen) lv_obj_clear_flag(objects.buttonstart3, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(objects.buttonstart3, LV_OBJ_FLAG_HIDDEN); }
  if (objects.buttonstop)   { if (zeigen) lv_obj_add_flag(objects.buttonstop, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(objects.buttonstop, LV_OBJ_FLAG_HIDDEN); }
}

void startCountdown(int idx) {
  activeTimerIdx = idx;
  countdownEndMillis = millis() + (uint32_t)getDauerMinuten(idx) * 60000UL;
  letzteAngezeigteSekunde = -1;
  state = STATE_RUNNING;
  setCountdownLabel(getDauerMinuten(idx) * 60L);
  zeigeStartButtons(false);
  Serial.printf("[Timer] Start %d: %d Minuten\n", idx + 1, getDauerMinuten(idx));
}

void stopCountdown() {
  state = STATE_IDLE;
  activeTimerIdx = -1;
  lv_label_set_text(objects.labelcountdown, "00:00");
  zeigeStartButtons(true);
  Serial.println("[Timer] Gestoppt");
}

// Blockiert bis der Alarm-Screen angetippt wird (lv_timer_handler laeuft mit).
void enterAlarm() {
  state = STATE_ALARM;
  alarmDismissFlag = false;
  loadScreen(SCREEN_ID_ALARM);
  lvgl_flush(50);

  bool rot = true;
  uint32_t letzterBlink = millis();

  while (!alarmDismissFlag) {
    if (millis() - letzterBlink > 600) {
      rot = !rot;
      lv_obj_set_style_bg_color(objects.alarm,
        rot ? lv_color_hex(0xE63946) : lv_color_hex(0x7A1620), LV_PART_MAIN);
      letzterBlink = millis();
    }
    playAlarmChime();
    // ~800ms Pause zwischen den Chimes, dabei alle 5ms auf Touch reagieren.
    for (int i = 0; i < 160 && !alarmDismissFlag; i++) { lv_timer_handler(); delay(5); }
  }

  state = STATE_IDLE;
  activeTimerIdx = -1;
  lv_label_set_text(objects.labelcountdown, "00:00");
  zeigeStartButtons(true);
  loadScreen(SCREEN_ID_HOME);
  lvgl_flush(50);
  Serial.println("[Alarm] Beendet durch Touch");
}

// ============================================================
//  UI-Event-Callbacks
// ============================================================
#define REG_CB(obj, cb, evt) do { if (obj) lv_obj_add_event_cb(obj, cb, evt, nullptr); } while(0)

void cbStart1(lv_event_t* e) { if (state != STATE_ALARM) startCountdown(0); }
void cbStart2(lv_event_t* e) { if (state != STATE_ALARM) startCountdown(1); }
void cbStart3(lv_event_t* e) { if (state != STATE_ALARM) startCountdown(2); }

void cbOpenSettings(lv_event_t* e) {
  if (state == STATE_ALARM) return;
  updateSettingsValueLabels();
  loadScreen(SCREEN_ID_SETTINGS);
}

void cbDauer1Minus(lv_event_t* e) { cfg.dauer1 = max(1, cfg.dauer1 - 5); updateSettingsValueLabels(); }
void cbDauer1Plus(lv_event_t* e)  { cfg.dauer1 = min(180, cfg.dauer1 + 5); updateSettingsValueLabels(); }
void cbDauer2Minus(lv_event_t* e) { cfg.dauer2 = max(1, cfg.dauer2 - 5); updateSettingsValueLabels(); }
void cbDauer2Plus(lv_event_t* e)  { cfg.dauer2 = min(180, cfg.dauer2 + 5); updateSettingsValueLabels(); }
void cbDauer3Minus(lv_event_t* e) { cfg.dauer3 = max(1, cfg.dauer3 - 5); updateSettingsValueLabels(); }
void cbDauer3Plus(lv_event_t* e)  { cfg.dauer3 = min(180, cfg.dauer3 + 5); updateSettingsValueLabels(); }

void cbVolumeChanged(lv_event_t* e) {
  cfg.volume = lv_slider_get_value(objects.slidervol);
  if (objects.labelvol) lv_label_set_text_fmt(objects.labelvol, "Vol %d%%", cfg.volume);
}

void cbSave(lv_event_t* e) {
  speichereCfg();
  updateHomeButtonLabels();
  loadScreen(SCREEN_ID_HOME);
}

void cbBack(lv_event_t* e) {
  loadScreen(SCREEN_ID_HOME);
}

void cbAlarmTap(lv_event_t* e) {
  alarmDismissFlag = true;
}

void cbStop(lv_event_t* e) {
  if (state == STATE_RUNNING) stopCountdown();
}

void registriereCallbacks() {
  REG_CB(objects.buttonstart1,     cbStart1,       LV_EVENT_CLICKED);
  REG_CB(objects.buttonstart2,     cbStart2,       LV_EVENT_CLICKED);
  REG_CB(objects.buttonstart3,     cbStart3,       LV_EVENT_CLICKED);
  REG_CB(objects.buttonstop,       cbStop,         LV_EVENT_CLICKED);
  REG_CB(objects.buttonsettings,   cbOpenSettings, LV_EVENT_CLICKED);
  REG_CB(objects.buttongame,       cbOpenMinesweeper, LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer1minus, cbDauer1Minus, LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer1plus,  cbDauer1Plus,  LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer2minus, cbDauer2Minus, LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer2plus,  cbDauer2Plus,  LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer3minus, cbDauer3Minus, LV_EVENT_CLICKED);
  REG_CB(objects.buttondauer3plus,  cbDauer3Plus,  LV_EVENT_CLICKED);
  REG_CB(objects.slidervol,        cbVolumeChanged, LV_EVENT_VALUE_CHANGED);
  REG_CB(objects.buttonsave,       cbSave,         LV_EVENT_CLICKED);
  REG_CB(objects.buttonback,       cbBack,         LV_EVENT_CLICKED);
  REG_CB(objects.alarm,            cbAlarmTap,     LV_EVENT_CLICKED);
}

// ============================================================
//  Datum/Uhrzeit auf dem Home-Screen
// ============================================================
void updateDatumUhrzeit() {
  struct tm ti;
  if (!getLocalTime(&ti, 0)) return;
  char buf[16];
  strftime(buf, sizeof(buf), "%d.%m.%Y", &ti);
  if (objects.labeldatum) lv_label_set_text(objects.labeldatum, buf);
  strftime(buf, sizeof(buf), "%H:%M", &ti);
  if (objects.labeluhrzeit) lv_label_set_text(objects.labeluhrzeit, buf);
}

// ============================================================
//  Captive Portal - WLAN-Ersteinrichtung
// ============================================================
void handlePortalRoot() {
  int n = WiFi.scanNetworks();
  String nets = "";
  for (int i = 0; i < n; i++) {
    nets += "<option value=\"" + WiFi.SSID(i) + "\">" +
            WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>\n";
  }
  String html = FPSTR(PORTAL_HTML);
  html.replace("%NETZWERKE%", nets);
  server.send(200, "text/html", html);
}

void handlePortalSave() {
  if (server.hasArg("ssid")) cfg.ssid = server.arg("ssid");
  if (server.hasArg("pass")) cfg.pass = server.arg("pass");
  speichereCfg();
  server.send(200, "text/html", FPSTR(PORTAL_OK_HTML));
  delay(2000);
  ESP.restart();
}

void startePortal() {
  portal_modus = true;
  WiFi.disconnect(true); delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("adhscounter-Setup");
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/",          HTTP_GET,  handlePortalRoot);
  server.on("/speichern", HTTP_POST, handlePortalSave);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();

  Serial.println("[Portal] AP gestartet: adhscounter-Setup");
}

// ============================================================
//  WebUI (Normalbetrieb) - adhscounter.local
// ============================================================
String baueStatus() {
  if (state == STATE_RUNNING) {
    long remaining = (countdownEndMillis - millis()) / 1000;
    if (remaining < 0) remaining = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "Timer %d laeuft - noch %02ld:%02ld min",
             activeTimerIdx + 1, remaining / 60, remaining % 60);
    return String(buf);
  } else if (state == STATE_ALARM) {
    return "Alarm aktiv - bitte am Geraet bestaetigen";
  }
  return "Bereit";
}

void handleWebRoot() {
  String html = FPSTR(WEBUI_HTML);
  html.replace("%VERSION%", FIRMWARE_VERSION);
  html.replace("%IP%", WiFi.localIP().toString());
  html.replace("%STATUS%", baueStatus());
  html.replace("%DAUER1%", String(cfg.dauer1));
  html.replace("%DAUER2%", String(cfg.dauer2));
  html.replace("%DAUER3%", String(cfg.dauer3));
  html.replace("%SSID%", cfg.ssid);
  server.send(200, "text/html", html);
}

void handleWebSave() {
  if (server.hasArg("dauer1")) cfg.dauer1 = constrain(server.arg("dauer1").toInt(), 1, 180);
  if (server.hasArg("dauer2")) cfg.dauer2 = constrain(server.arg("dauer2").toInt(), 1, 180);
  if (server.hasArg("dauer3")) cfg.dauer3 = constrain(server.arg("dauer3").toInt(), 1, 180);
  speichereCfg();
  updateHomeButtonLabels();
  updateSettingsValueLabels();
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleWebWlanAendern() {
  int n = WiFi.scanNetworks();
  String nets = "";
  for (int i = 0; i < n; i++) {
    nets += "<option value=\"" + WiFi.SSID(i) + "\">" +
            WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>\n";
  }
  String html = FPSTR(WLAN_HTML);
  html.replace("%NETZWERKE%", nets);
  server.send(200, "text/html", html);
}

void handleWebWlanSave() {
  if (server.hasArg("ssid")) cfg.ssid = server.arg("ssid");
  if (server.hasArg("pass")) cfg.pass = server.arg("pass");
  speichereCfg();
  server.send(200, "text/html", FPSTR(PORTAL_OK_HTML));
  delay(2000);
  ESP.restart();
}

// ============================================================
//  OTA-Update (1:1 Muster aus WetterCubePlus)
// ============================================================
void handleWebOtaCheck() {
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, OTA_VERSION_URL);
  String latest = FIRMWARE_VERSION;
  bool updateAvailable = false;
  if (http.GET() == 200) {
    String body = http.getString();
    int idx = body.indexOf("\"version\"");
    if (idx >= 0) {
      int q1 = body.indexOf('"', idx + 9);
      int q2 = body.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1) {
        int q3 = body.indexOf('"', q2 + 1);
        latest = body.substring(q2 + 1, q3);
        updateAvailable = (latest != FIRMWARE_VERSION);
      }
    }
  }
  http.end();
  String json = "{\"current\":\"" + String(FIRMWARE_VERSION) + "\",\"latest\":\"" + latest +
                "\",\"update_available\":" + (updateAvailable ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleWebOtaDoUpdate() {
  WiFiClientSecure sc; sc.setInsecure();
  HTTPClient http;
  http.begin(sc, OTA_BIN_URL);
  http.setTimeout(30000);
  int code = http.GET();
  if (code != 200) {
    server.send(500, "text/plain", "Download fehlgeschlagen (HTTP " + String(code) + ")");
    http.end();
    return;
  }
  int contentLen = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  if (!Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
    server.send(500, "text/plain", "Update.begin fehlgeschlagen");
    http.end();
    return;
  }
  Update.writeStream(*stream);
  bool success = Update.end(true) && !Update.hasError();
  http.end();
  if (success) {
    server.send(200, "text/plain", "ok");
    delay(1500);
    ESP.restart();
  } else {
    server.send(500, "text/plain", Update.errorString());
  }
}

void starteWebUI() {
  server.on("/",               HTTP_GET,  handleWebRoot);
  server.on("/speichern",      HTTP_POST, handleWebSave);
  server.on("/wlan",           HTTP_GET,  handleWebWlanAendern);
  server.on("/wlan_speichern", HTTP_POST, handleWebWlanSave);
  server.on("/ota_check",      HTTP_GET,  handleWebOtaCheck);
  server.on("/ota_update",     HTTP_POST, handleWebOtaDoUpdate);
  server.begin();
}

// ============================================================
//  Minesweeper Minispiel (Muster wie "Bubblebreaker" in WetterCubePlus:
//  eigene, programmatisch gebaute Screens + Canvas-Rendering statt vieler
//  Einzel-Widgets, Highscore per Preferences, ein Schritt zurueck per
//  Board-Snapshot, Abbrechen-Button)
// ============================================================
#define MS_COLS      10
#define MS_ROWS       7
#define MS_MINES     10
#define MS_CELL_W    48
#define MS_CELL_H    40
#define MS_CANVAS_W  (MS_COLS * MS_CELL_W)   // 480
#define MS_CANVAS_H  (MS_ROWS * MS_CELL_H)   // 280

struct MsCell {
  bool mine;
  bool revealed;
  bool flagged;
  int8_t adjacent;
};

static MsCell   msBoard[MS_ROWS][MS_COLS];
static MsCell   msBoardUndo[MS_ROWS][MS_COLS];
static bool     msUndoVerfuegbar = false;
static bool     msAktiv = false;
static bool     msMinenGesetzt = false;
static uint32_t msStartZeit = 0;
static uint32_t msVergangeneSek = 0;
static bool     msTimerLaeuft = false;

static lv_obj_t*   msStartScreen = nullptr;
static lv_obj_t*   msGameScreen = nullptr;
static lv_obj_t*   msCanvas = nullptr;
static lv_color_t* msCanvasBuf = nullptr;
static lv_obj_t*   msLblZeit = nullptr;
static lv_obj_t*   msLblMinen = nullptr;
static lv_obj_t*   msUndoBtn = nullptr;
static lv_obj_t*   msGameOverPanel = nullptr;
static lv_obj_t*   msLblGameOverTitel = nullptr;
static lv_obj_t*   msLblGameOverText = nullptr;
static lv_obj_t*   msHsRows[5];

static int32_t msHighscores[5];      // Sekunden, schnellste zuerst
static uint8_t msHighscoreCount = 0;

// -- Highscore-Persistenz (NVS, eigener Namespace) --
static void msLadeHighscores() {
  Preferences p; p.begin("adhs_mine", true);
  msHighscoreCount = p.getUChar("cnt", 0);
  char key[4];
  for (int i = 0; i < 5; i++) { snprintf(key, sizeof(key), "s%d", i); msHighscores[i] = p.getInt(key, 0); }
  p.end();
}

static void msSpeichereHighscores() {
  Preferences p; p.begin("adhs_mine", false);
  p.putUChar("cnt", msHighscoreCount);
  char key[4];
  for (int i = 0; i < 5; i++) { snprintf(key, sizeof(key), "s%d", i); p.putInt(key, msHighscores[i]); }
  p.end();
}

// Zeit-Highscore: kleiner ist besser (schnell gewonnen), aufsteigend sortiert.
static bool msFuegeHighscoreEin(int32_t sekunden) {
  if (msHighscoreCount < 5) {
    msHighscores[msHighscoreCount++] = sekunden;
  } else if (sekunden < msHighscores[4]) {
    msHighscores[4] = sekunden;
  } else {
    return false;
  }
  for (int i = msHighscoreCount - 1; i > 0 && msHighscores[i] < msHighscores[i - 1]; i--) {
    int32_t tmp = msHighscores[i]; msHighscores[i] = msHighscores[i - 1]; msHighscores[i - 1] = tmp;
  }
  msSpeichereHighscores();
  return true;
}

static void msAktualisiereHighscoreAnzeige() {
  for (int i = 0; i < 5; i++) {
    if (i < msHighscoreCount) lv_label_set_text_fmt(msHsRows[i], "%d. %ld s", i + 1, (long)msHighscores[i]);
    else lv_label_set_text_fmt(msHsRows[i], "%d. --", i + 1);
  }
}

// -- Brett-Logik --
static void msLeeresBrett() {
  memset(msBoard, 0, sizeof(msBoard));
  msMinenGesetzt = false;
}

// Minen erst nach dem ersten Tap setzen, nie auf dem angetippten Feld oder
// seinen direkten Nachbarn - fairer Einstieg (kein Sofort-Verlust im 1. Zug).
static void msSetzeMinen(int excludeR, int excludeC) {
  int gesetzt = 0;
  while (gesetzt < MS_MINES) {
    int r = random(MS_ROWS);
    int c = random(MS_COLS);
    if (msBoard[r][c].mine) continue;
    if (abs(r - excludeR) <= 1 && abs(c - excludeC) <= 1) continue;
    msBoard[r][c].mine = true;
    gesetzt++;
  }
  for (int r = 0; r < MS_ROWS; r++) {
    for (int c = 0; c < MS_COLS; c++) {
      if (msBoard[r][c].mine) continue;
      int n = 0;
      for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
          if (dr == 0 && dc == 0) continue;
          int nr = r + dr, nc = c + dc;
          if (nr < 0 || nr >= MS_ROWS || nc < 0 || nc >= MS_COLS) continue;
          if (msBoard[nr][nc].mine) n++;
        }
      }
      msBoard[r][c].adjacent = (int8_t)n;
    }
  }
  msMinenGesetzt = true;
}

// Iterative Flood-Fill fuer Felder ohne Nachbarminen (kein Rekursions-Stack-Risiko)
static void msRevealFlood(int startR, int startC) {
  static int stackR[MS_ROWS * MS_COLS];
  static int stackC[MS_ROWS * MS_COLS];
  int sp = 0;
  stackR[sp] = startR; stackC[sp] = startC; sp++;
  while (sp > 0) {
    sp--;
    int r = stackR[sp], c = stackC[sp];
    if (msBoard[r][c].revealed || msBoard[r][c].flagged) continue;
    msBoard[r][c].revealed = true;
    if (msBoard[r][c].adjacent != 0) continue;
    for (int dr = -1; dr <= 1; dr++) {
      for (int dc = -1; dc <= 1; dc++) {
        if (dr == 0 && dc == 0) continue;
        int nr = r + dr, nc = c + dc;
        if (nr < 0 || nr >= MS_ROWS || nc < 0 || nc >= MS_COLS) continue;
        if (msBoard[nr][nc].revealed || msBoard[nr][nc].mine || msBoard[nr][nc].flagged) continue;
        stackR[sp] = nr; stackC[sp] = nc; sp++;
      }
    }
  }
}

static void msAlleMinenAufdecken() {
  for (int r = 0; r < MS_ROWS; r++)
    for (int c = 0; c < MS_COLS; c++)
      if (msBoard[r][c].mine) msBoard[r][c].revealed = true;
}

static bool msIstGewonnen() {
  for (int r = 0; r < MS_ROWS; r++)
    for (int c = 0; c < MS_COLS; c++)
      if (!msBoard[r][c].mine && !msBoard[r][c].revealed) return false;
  return true;
}

// Zeichnet das komplette Brett auf die Canvas-Flaeche (PSRAM-Puffer).
static const uint32_t MS_ZAHLFARBEN[9] = {
  0x000000, 0x1976D2, 0x388E3C, 0xD32F2F, 0x7B1FA2, 0xFF8F00, 0x0097A7, 0x424242, 0x757575
};

static void msZeichneBrett() {
  if (!msCanvas) return;
  lv_canvas_fill_bg(msCanvas, lv_color_hex(0xBDBDBD), LV_OPA_COVER);

  lv_draw_rect_dsc_t rdsc; lv_draw_rect_dsc_init(&rdsc);
  rdsc.radius = 4; rdsc.bg_opa = LV_OPA_COVER;
  lv_draw_label_dsc_t ldsc; lv_draw_label_dsc_init(&ldsc);
  ldsc.font = &font_montserrat_18;
  ldsc.align = LV_TEXT_ALIGN_CENTER;

  for (int r = 0; r < MS_ROWS; r++) {
    for (int c = 0; c < MS_COLS; c++) {
      MsCell& cell = msBoard[r][c];
      int x = c * MS_CELL_W, y = r * MS_CELL_H;

      if (!cell.revealed) {
        rdsc.bg_color = lv_color_hex(cell.flagged ? 0xFFC107 : 0x9E9E9E);
        lv_canvas_draw_rect(msCanvas, x + 2, y + 2, MS_CELL_W - 4, MS_CELL_H - 4, &rdsc);
        if (cell.flagged) {
          ldsc.color = lv_color_white();
          lv_canvas_draw_text(msCanvas, x, y + (MS_CELL_H - 18) / 2, MS_CELL_W, &ldsc, "F");
        }
      } else if (cell.mine) {
        rdsc.bg_color = lv_color_hex(0xD32F2F);
        lv_canvas_draw_rect(msCanvas, x + 2, y + 2, MS_CELL_W - 4, MS_CELL_H - 4, &rdsc);
      } else {
        rdsc.bg_color = lv_color_hex(0xECEFF1);
        lv_canvas_draw_rect(msCanvas, x + 2, y + 2, MS_CELL_W - 4, MS_CELL_H - 4, &rdsc);
        if (cell.adjacent > 0) {
          char buf[2]; snprintf(buf, sizeof(buf), "%d", cell.adjacent);
          ldsc.color = lv_color_hex(MS_ZAHLFARBEN[cell.adjacent]);
          lv_canvas_draw_text(msCanvas, x, y + (MS_CELL_H - 18) / 2, MS_CELL_W, &ldsc, buf);
        }
      }
    }
  }
}

static void msSpielEnde(bool gewonnen) {
  msAktiv = false;
  msTimerLaeuft = false;
  if (gewonnen) {
    bool neuerHighscore = msFuegeHighscoreEin((int32_t)msVergangeneSek);
    lv_label_set_text(msLblGameOverTitel, "Gewonnen!");
    lv_label_set_text_fmt(msLblGameOverText, "Zeit: %lu s%s",
                           (unsigned long)msVergangeneSek, neuerHighscore ? "  (Highscore!)" : "");
  } else {
    msAlleMinenAufdecken();
    msZeichneBrett();
    lv_label_set_text(msLblGameOverTitel, "Boom!");
    lv_label_set_text(msLblGameOverText, "Leider eine Mine erwischt.");
  }
  lv_obj_clear_flag(msGameOverPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(msGameOverPanel);
}

static void msSicherAufdecken(int r, int c) {
  if (!msAktiv) return;
  if (msBoard[r][c].flagged || msBoard[r][c].revealed) return;

  if (!msMinenGesetzt) msSetzeMinen(r, c);

  memcpy(msBoardUndo, msBoard, sizeof(msBoard));
  msUndoVerfuegbar = true;
  if (msUndoBtn) lv_obj_set_style_bg_color(msUndoBtn, lv_color_hex(0x2980b9), 0);

  if (msBoard[r][c].mine) {
    msBoard[r][c].revealed = true;
    msZeichneBrett();
    msSpielEnde(false);
    return;
  }
  msRevealFlood(r, c);
  msZeichneBrett();
  if (msIstGewonnen()) msSpielEnde(true);
}

static bool msKoordAusEvent(lv_event_t* e, int* rOut, int* cOut) {
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return false;
  lv_point_t p; lv_indev_get_point(indev, &p);
  lv_area_t coords; lv_obj_get_coords(msCanvas, &coords);
  int lx = p.x - coords.x1, ly = p.y - coords.y1;
  if (lx < 0 || ly < 0 || lx >= MS_CANVAS_W || ly >= MS_CANVAS_H) return false;
  *cOut = lx / MS_CELL_W;
  *rOut = ly / MS_CELL_H;
  if (*rOut < 0 || *rOut >= MS_ROWS || *cOut < 0 || *cOut >= MS_COLS) return false;
  return true;
}

// Kurzer Tap = Feld aufdecken, langes Antippen = Flagge setzen/entfernen.
static void cbMsCanvasTap(lv_event_t* e) {
  int r, c;
  if (!msKoordAusEvent(e, &r, &c)) return;
  msSicherAufdecken(r, c);
}

static void cbMsCanvasLongPress(lv_event_t* e) {
  if (!msAktiv) return;
  int r, c;
  if (!msKoordAusEvent(e, &r, &c)) return;
  if (msBoard[r][c].revealed) return;
  msBoard[r][c].flagged = !msBoard[r][c].flagged;
  msZeichneBrett();
}

static void cbMsUndoTap(lv_event_t*) {
  if (!msAktiv || !msUndoVerfuegbar) return;
  memcpy(msBoard, msBoardUndo, sizeof(msBoard));
  msUndoVerfuegbar = false;
  if (msUndoBtn) lv_obj_set_style_bg_color(msUndoBtn, lv_color_hex(0xbdc3c7), 0);
  msZeichneBrett();
}

static void msTimerCb(lv_timer_t*) {
  if (!msTimerLaeuft) return;
  msVergangeneSek = (millis() - msStartZeit) / 1000;
  lv_label_set_text_fmt(msLblZeit, "Zeit: %lu s", (unsigned long)msVergangeneSek);
}

static void msStarteSpiel() {
  msLeeresBrett();
  msUndoVerfuegbar = false;
  if (msUndoBtn) lv_obj_set_style_bg_color(msUndoBtn, lv_color_hex(0xbdc3c7), 0);
  lv_obj_add_flag(msGameOverPanel, LV_OBJ_FLAG_HIDDEN);
  msVergangeneSek = 0;
  msStartZeit = millis();
  msTimerLaeuft = true;
  lv_label_set_text(msLblZeit, "Zeit: 0 s");
  lv_label_set_text_fmt(msLblMinen, "Minen: %d", MS_MINES);
  msZeichneBrett();
  msAktiv = true;
  lv_scr_load(msGameScreen);
}

static void msZeigeStartScreen() {
  msLadeHighscores();
  msAktualisiereHighscoreAnzeige();
  lv_scr_load(msStartScreen);
}

static void cbOpenMinesweeper(lv_event_t*) { msZeigeStartScreen(); }
static void cbMsStartTap(lv_event_t*)      { msStarteSpiel(); }
static void cbMsBackTap(lv_event_t*)       { loadScreen(SCREEN_ID_HOME); }
static void cbMsExitTap(lv_event_t*)       { msAktiv = false; msTimerLaeuft = false; loadScreen(SCREEN_ID_HOME); }
static void cbMsNochmalTap(lv_event_t*)    { msStarteSpiel(); }
static void cbMsZurueckMenuTap(lv_event_t*) { msAktiv = false; msTimerLaeuft = false; msZeigeStartScreen(); }

static lv_obj_t* msTextButton(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                               uint32_t bgColor, const char* text) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_color(btn, lv_color_hex(bgColor), 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
  lv_obj_center(lbl);
  return btn;
}

// Eigenstaendiger Start-/Bestenliste-Screen.
static void msErstelleStartScreen() {
  msStartScreen = lv_obj_create(nullptr);
  lv_obj_set_size(msStartScreen, 480, 320);
  lv_obj_set_style_bg_color(msStartScreen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(msStartScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(msStartScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* titel = lv_label_create(msStartScreen);
  lv_label_set_text(titel, "Minesweeper");
  lv_obj_set_style_text_font(titel, &font_montserrat_22, 0);
  lv_obj_set_style_text_color(titel, lv_color_hex(0x1a1a1a), 0);
  lv_obj_align(titel, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* hsTitel = lv_label_create(msStartScreen);
  lv_label_set_text(hsTitel, "Bestenliste (schnellste Zeit)");
  lv_obj_set_style_text_font(hsTitel, &font_montserrat_18, 0);
  lv_obj_set_style_text_color(hsTitel, lv_color_hex(0x1a1a1a), 0);
  lv_obj_align(hsTitel, LV_ALIGN_TOP_MID, 0, 55);

  for (int i = 0; i < 5; i++) {
    msHsRows[i] = lv_label_create(msStartScreen);
    lv_obj_set_style_text_font(msHsRows[i], &font_montserrat_18, 0);
    lv_obj_set_style_text_color(msHsRows[i], lv_color_hex(0x333333), 0);
    lv_obj_align(msHsRows[i], LV_ALIGN_TOP_MID, 0, 90 + i * 22);
    lv_label_set_text_fmt(msHsRows[i], "%d. --", i + 1);
  }

  lv_obj_t* zurueckBtn = msTextButton(msStartScreen, 20, 255, 120, 45, 0x7f8c8d, "Zurueck");
  lv_obj_add_event_cb(zurueckBtn, cbMsBackTap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startBtn = msTextButton(msStartScreen, 300, 255, 160, 45, 0x2ecc71, "Start");
  lv_obj_add_event_cb(startBtn, cbMsStartTap, LV_EVENT_CLICKED, nullptr);
}

// Eigenstaendiger Spiel-Screen mit Canvas-Spielfeld.
static void msErstelleGameScreen() {
  msGameScreen = lv_obj_create(nullptr);
  lv_obj_set_size(msGameScreen, 480, 320);
  lv_obj_set_style_bg_color(msGameScreen, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(msGameScreen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(msGameScreen, LV_OBJ_FLAG_SCROLLABLE);

  msLblZeit = lv_label_create(msGameScreen);
  lv_label_set_text(msLblZeit, "Zeit: 0 s");
  lv_obj_set_style_text_font(msLblZeit, &font_montserrat_18, 0);
  lv_obj_set_style_text_color(msLblZeit, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_pos(msLblZeit, 10, 6);

  msLblMinen = lv_label_create(msGameScreen);
  lv_label_set_text_fmt(msLblMinen, "Minen: %d", MS_MINES);
  lv_obj_set_style_text_font(msLblMinen, &font_montserrat_18, 0);
  lv_obj_set_style_text_color(msLblMinen, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_pos(msLblMinen, 170, 6);

  msUndoBtn = lv_btn_create(msGameScreen);
  lv_obj_set_pos(msUndoBtn, 330, 2);
  lv_obj_set_size(msUndoBtn, 75, 28);
  lv_obj_set_style_bg_color(msUndoBtn, lv_color_hex(0xbdc3c7), 0);
  lv_obj_set_style_radius(msUndoBtn, 6, 0);
  lv_obj_t* undoLbl = lv_label_create(msUndoBtn);
  lv_label_set_text(undoLbl, "Undo");
  lv_obj_set_style_text_color(undoLbl, lv_color_white(), 0);
  lv_obj_center(undoLbl);
  lv_obj_add_event_cb(msUndoBtn, cbMsUndoTap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* exitBtn = lv_btn_create(msGameScreen);
  lv_obj_set_pos(exitBtn, 430, 2);
  lv_obj_set_size(exitBtn, 44, 30);
  lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0xcc0000), 0);
  lv_obj_set_style_radius(exitBtn, 6, 0);
  lv_obj_t* exitLbl = lv_label_create(exitBtn);
  lv_label_set_text(exitLbl, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_color(exitLbl, lv_color_white(), 0);
  lv_obj_center(exitLbl);
  lv_obj_add_event_cb(exitBtn, cbMsExitTap, LV_EVENT_CLICKED, nullptr);

  // Spielfeld: eine Canvas statt vieler Einzel-Widgets, Puffer im PSRAM.
  uint32_t bufSize = LV_CANVAS_BUF_SIZE_TRUE_COLOR(MS_CANVAS_W, MS_CANVAS_H);
  msCanvasBuf = (lv_color_t*)ps_malloc(bufSize);
  if (msCanvasBuf) {
    msCanvas = lv_canvas_create(msGameScreen);
    lv_canvas_set_buffer(msCanvas, msCanvasBuf, MS_CANVAS_W, MS_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(msCanvas, 0, 34);
    lv_obj_clear_flag(msCanvas, LV_OBJ_FLAG_SCROLLABLE);
    // lv_canvas erbt von lv_img, dessen Konstruktor LV_OBJ_FLAG_CLICKABLE entfernt -
    // ohne dieses Add reagiert die Canvas nie auf Touch (siehe WetterCubePlus-Lessons-Learned).
    lv_obj_add_flag(msCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(msCanvas, cbMsCanvasTap, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(msCanvas, cbMsCanvasLongPress, LV_EVENT_LONG_PRESSED, nullptr);
  } else {
    Serial.println("[Minesweeper] ps_malloc Canvas-Puffer fehlgeschlagen!");
  }

  msGameOverPanel = lv_obj_create(msGameScreen);
  lv_obj_set_size(msGameOverPanel, 320, 170);
  lv_obj_align(msGameOverPanel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(msGameOverPanel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(msGameOverPanel, 220, 0);
  lv_obj_set_style_radius(msGameOverPanel, 10, 0);
  lv_obj_set_style_pad_all(msGameOverPanel, 0, 0);
  lv_obj_clear_flag(msGameOverPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(msGameOverPanel, LV_OBJ_FLAG_HIDDEN);

  msLblGameOverTitel = lv_label_create(msGameOverPanel);
  lv_label_set_text(msLblGameOverTitel, "Gewonnen!");
  lv_obj_set_width(msLblGameOverTitel, 320);
  lv_obj_set_style_text_font(msLblGameOverTitel, &font_montserrat_22, 0);
  lv_obj_set_style_text_color(msLblGameOverTitel, lv_color_white(), 0);
  lv_obj_set_style_text_align(msLblGameOverTitel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(msLblGameOverTitel, LV_ALIGN_TOP_MID, 0, 16);

  msLblGameOverText = lv_label_create(msGameOverPanel);
  lv_label_set_text(msLblGameOverText, "");
  lv_obj_set_width(msLblGameOverText, 280);
  lv_obj_set_style_text_font(msLblGameOverText, &font_montserrat_18, 0);
  lv_obj_set_style_text_color(msLblGameOverText, lv_color_white(), 0);
  lv_obj_set_style_text_align(msLblGameOverText, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(msLblGameOverText, LV_ALIGN_TOP_MID, 0, 54);

  // Bündig an den Panelrand ausgerichtet (statt fester x-Koordinaten) - so
  // bleiben beide Buttons garantiert symmetrisch, egal wie breit das Panel ist.
  lv_obj_t* nochmalBtn = msTextButton(msGameOverPanel, 0, 0, 130, 44, 0x2ecc71, "Nochmal");
  lv_obj_align(nochmalBtn, LV_ALIGN_BOTTOM_LEFT, 20, -18);
  lv_obj_add_event_cb(nochmalBtn, cbMsNochmalTap, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* menuBtn = msTextButton(msGameOverPanel, 0, 0, 130, 44, 0x7f8c8d, "Menue");
  lv_obj_align(menuBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -18);
  lv_obj_add_event_cb(menuBtn, cbMsZurueckMenuTap, LV_EVENT_CLICKED, nullptr);

  lv_timer_create(msTimerCb, 1000, nullptr);
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[adhscounter] Start v" FIRMWARE_VERSION);

  ladeCfg();

  // Display initialisieren
  tft.begin();
  tft.setRotation(1);  // Landscape (480x320)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.fillScreen(TFT_BLACK);

  // Touch-Kalibrierung (Finger halten beim Start = Neukalibrierung)
  touchKalibrierung();

  // I2S-Alarm-Ausgang
  i2sInit();

  // LVGL initialisieren
  lv_init();
  lvgl_buf1 = (lv_color_t*)ps_malloc(TFT_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t));
  lvgl_buf2 = (lv_color_t*)ps_malloc(TFT_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t));
  if (!lvgl_buf1 || !lvgl_buf2) {
    static lv_color_t fallback1[TFT_WIDTH * 10];
    static lv_color_t fallback2[TFT_WIDTH * 10];
    lv_disp_draw_buf_init(&draw_buf, fallback1, fallback2, TFT_WIDTH * 10);
  } else {
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf1, lvgl_buf2, TFT_WIDTH * LVGL_BUF_LINES);
  }

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = TFT_WIDTH;
  disp_drv.ver_res  = TFT_HEIGHT;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = disp_flush;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read;
  lv_indev_drv_register(&indev_drv);

  lv_png_init();  // PicoPixel-Icons sind PNG-Assets

  // PicoPixel-UI initialisieren; ui_init() laedt standardmaessig den Alarm-
  // Screen (letzter bearbeiteter Screen im Editor) - hier explizit auf Home.
  ui_init();
  updateHomeButtonLabels();
  updateSettingsValueLabels();
  lv_label_set_text(objects.labelcountdown, "00:00");
  zeigeStartButtons(true);

  // Minesweeper-Minispiel: eigene Screens programmatisch anlegen
  msErstelleStartScreen();
  msErstelleGameScreen();
  msLadeHighscores();
  msAktualisiereHighscoreAnzeige();

  registriereCallbacks();
  loadScreen(SCREEN_ID_HOME);
  lvgl_flush();

  // WiFi verbinden oder Portal starten
  if (cfg.ssid.isEmpty()) {
    Serial.println("[WiFi] Keine SSID konfiguriert - starte Portal");
    startePortal();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    lvgl_flush(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Verbindung fehlgeschlagen - starte Portal");
    startePortal();
    return;
  }

  Serial.println("[WiFi] Verbunden: " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] adhscounter.local aktiv");
  }

  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  { struct tm ti; int r = 0; while (!getLocalTime(&ti) && r++ < 30) { delay(300); lv_timer_handler(); } }
  updateDatumUhrzeit();

  starteWebUI();
  Serial.println("[Setup] Fertig");
}

void loop() {
  if (portal_modus) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  lv_timer_handler();
  delay(5);

  static uint32_t letzteSekunde = 0;
  if (millis() - letzteSekunde >= 1000) {
    letzteSekunde = millis();
    updateDatumUhrzeit();

    if (state == STATE_RUNNING) {
      long remaining = (countdownEndMillis - (long)millis()) / 1000;
      if (remaining <= 0) {
        enterAlarm();
      } else if (remaining != letzteAngezeigteSekunde) {
        letzteAngezeigteSekunde = remaining;
        setCountdownLabel(remaining);
      }
    }
  }
}
