#pragma once
// HTML-Strings in .h ausgelagert, da der Arduino-Präprozessor
// Raw-String-Literals in .ino falsch parst (beendet String bei " inmitten von R"...").

const char PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>adhscounter – Einrichtung</title>
<style>
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:20px}
  h1{color:#58a6ff;font-size:1.4em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;max-width:420px;margin:auto}
  label{display:block;margin-top:12px;color:#8b949e;font-size:.9em}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid #30363d;
    background:#21262d;color:#e6edf3;box-sizing:border-box;margin-top:4px}
  button{margin-top:20px;width:100%;padding:10px;background:#238636;color:#fff;
    border:none;border-radius:4px;font-size:1em;cursor:pointer}
  button:hover{background:#2ea043}
</style></head><body>
<div class="card">
  <h1>adhscounter Einrichtung</h1>
  <form action="/speichern" method="post">
    <label>WLAN-Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>WLAN-Passwort</label>
    <input type="password" name="pass" placeholder="Passwort eingeben">
    <button type="submit">Speichern &amp; Neustart</button>
  </form>
</div></body></html>
)rawhtml";

const char PORTAL_OK_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Gespeichert</title>
<meta http-equiv="refresh" content="5;url=/">
<style>
  body{font-family:Arial;background:#0d1117;color:#e6edf3;text-align:center;padding-top:60px}
  .ok{color:#2ea043;font-size:2em}
</style></head><body>
<div class="ok">Gespeichert</div>
<p>Geraet wird neu gestartet...</p>
</body></html>
)rawhtml";

const char WEBUI_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>adhscounter</title>
<style>
  *{box-sizing:border-box}
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:16px}
  h1{color:#58a6ff;margin-bottom:4px}
  .ver{color:#8b949e;font-size:.85em;margin-bottom:20px}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:16px;margin-bottom:16px}
  h2{color:#e6edf3;font-size:1em;margin:0 0 12px}
  label{display:block;color:#8b949e;font-size:.85em;margin-top:10px}
  input[type=number]{width:100%;padding:7px;border-radius:4px;
    border:1px solid #30363d;background:#21262d;color:#e6edf3;margin-top:3px}
  .btn{display:inline-block;padding:9px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:6px}
  .btn-gruen{background:#238636;color:#fff}.btn-gruen:hover{background:#2ea043}
  .btn-blau{background:#1f6feb;color:#fff}.btn-blau:hover{background:#388bfd}
  .status{font-size:.85em;color:#8b949e;margin-top:6px}
  .row{display:flex;gap:10px;margin-top:8px}
  .col{flex:1}
  input[type=range]{width:100%;margin-top:6px}
</style></head><body>
<h1>adhscounter</h1>
<div class="ver">Firmware: %VERSION% | IP: %IP% | adhscounter.local</div>

<div class="card">
  <h2>Status</h2>
  <div class="status">%STATUS%</div>
</div>

<form action="/speichern" method="post">
<div class="card">
  <h2>Timer-Dauer (Minuten)</h2>
  <div class="row">
    <div class="col"><label>Timer 1</label><input type="number" name="dauer1" min="1" max="180" value="%DAUER1%"></div>
    <div class="col"><label>Timer 2</label><input type="number" name="dauer2" min="1" max="180" value="%DAUER2%"></div>
    <div class="col"><label>Timer 3</label><input type="number" name="dauer3" min="1" max="180" value="%DAUER3%"></div>
  </div>
  <button type="submit" class="btn btn-gruen">Speichern</button>
</div>

<div class="card">
  <h2>Alarm-Lautstärke</h2>
  <input type="range" name="volume" min="0" max="100" value="%VOLUME%"
         oninput="document.getElementById('vol_val').textContent=this.value">
  <div class="status">Lautstärke: <span id="vol_val">%VOLUME%</span>%</div>
</div>
</form>

<div class="card">
  <h2>WLAN</h2>
  <div class="status">Verbunden mit: %SSID%</div>
  <a href="/wlan"><button class="btn btn-blau">WLAN ändern</button></a>
</div>

<div class="card">
  <h2>Firmware-Update</h2>
  <div class="status">Aktuelle Version: %VERSION%</div>
  <button class="btn btn-blau" onclick="checkOta()">Auf Updates prüfen</button>
  <div id="ota_status" class="status"></div>
</div>

<script>
function checkOta(){
  fetch('/ota_check').then(r=>r.json()).then(d=>{
    const el=document.getElementById('ota_status');
    if(d.update_available){
      el.innerHTML='Neue Version verfügbar: '+d.latest+' <button class="btn btn-gruen" onclick="doUpdate()">Jetzt installieren</button>';
    } else {
      el.textContent='Firmware ist aktuell.';
    }
  }).catch(()=>{document.getElementById('ota_status').textContent='Fehler beim Prüfen.';});
}
function doUpdate(){
  document.getElementById('ota_status').textContent='Update läuft... bitte warten (ca. 30 Sek.)';
  fetch('/ota_update',{method:'POST'}).then(r=>r.text()).then(t=>{
    document.getElementById('ota_status').textContent=t;
  }).catch(()=>{document.getElementById('ota_status').textContent='Update fehlgeschlagen.';});
}
</script>
</body></html>
)rawhtml";

const char WLAN_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>adhscounter – WLAN</title>
<style>
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:20px}
  h1{color:#58a6ff;font-size:1.4em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;max-width:420px;margin:auto}
  label{display:block;margin-top:12px;color:#8b949e;font-size:.9em}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid #30363d;
    background:#21262d;color:#e6edf3;box-sizing:border-box;margin-top:4px}
  button{margin-top:20px;width:100%;padding:10px;background:#238636;color:#fff;
    border:none;border-radius:4px;font-size:1em;cursor:pointer}
</style></head><body>
<div class="card">
  <h1>WLAN ändern</h1>
  <form action="/wlan_speichern" method="post">
    <label>WLAN-Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>WLAN-Passwort</label>
    <input type="password" name="pass" placeholder="Passwort eingeben">
    <button type="submit">Speichern &amp; Neustart</button>
  </form>
</div></body></html>
)rawhtml";
