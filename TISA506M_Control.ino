/*
 * TISA506M Thermal Imager Control Panel
 * ======================================
 * Platform  : ESP32
 * Framework : Arduino IDE
 * Libraries : WiFi.h, ESPAsyncWebServer.h, AsyncTCP.h, ArduinoJson.h
 *
 * Description:
 *   Hosts a mobile-friendly web interface over WiFi Access Point (SoftAP).
 *   Users adjust thermal imager parameters via a browser, then press
 *   "SAVE SETTINGS" to transmit all values as UART2 packets.
 *
 * Network:
 *   SSID     : TISA506M
 *   Password : 12345678
 *   IP       : 192.168.4.1
 *
 * UART2:
 *   TX : GPIO17
 *   RX : GPIO16
 *   Baud : 115200
 */

// ─────────────────────────────────────────────
//  Library Includes
// ─────────────────────────────────────────────
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────
//  Network Configuration
// ─────────────────────────────────────────────
const char* AP_SSID     = "TISA506M";
const char* AP_PASSWORD = "12345678";

// ─────────────────────────────────────────────
//  UART2 Configuration
// ─────────────────────────────────────────────
#define UART2_BAUD  115200
#define UART2_TX    17
#define UART2_RX    16

// ─────────────────────────────────────────────
//  Server & WebSocket
// ─────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ─────────────────────────────────────────────
//  Global Parameter Storage
// ─────────────────────────────────────────────
int    g_brightness = 50;
int    g_contrast   = 50;
int    g_denoise    = 5;
int    g_sharpness  = 5;
String g_agc        = "AutoBG";
int    g_zoom       = 1;
int    g_boresightX = 0;
int    g_boresightY = 0;

// ─────────────────────────────────────────────
//  HTML Page (stored in PROGMEM)
// ─────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  <title>TISA506M Control Panel</title>
  <style>
    /* ── Reset & Base ── */
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    :root {
      --bg:          #0d0f14;
      --surface:     #181c26;
      --surface2:    #1e2330;
      --border:      #2a3045;
      --accent:      #00c8ff;
      --accent-dim:  #007fa8;
      --accent-glow: rgba(0,200,255,0.18);
      --text:        #e4eaf5;
      --text-muted:  #7a8aab;
      --save-bg:     #00c8ff;
      --save-text:   #0d0f14;
      --danger:      #ff4e6a;
      --radius:      12px;
      --radius-sm:   8px;
      --shadow:      0 4px 24px rgba(0,0,0,0.5);
    }

    html { scroll-behavior: smooth; }

    body {
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      padding-bottom: 100px;
    }

    /* ── Header ── */
    header {
      background: linear-gradient(135deg, #0a1628 0%, #657fc8ff 100%);
      border-bottom: 1px solid var(--border);
      padding: 18px 20px 14px;
      display: flex;
      align-items: center;
      gap: 14px;
      position: sticky;
      top: 0;
      z-index: 100;
      box-shadow: var(--shadow);
    }

    .header-icon {
      width: 38px;
      height: 38px;
      background: linear-gradient(135deg, var(--accent), var(--accent-dim));
      border-radius: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 20px;
      flex-shrink: 0;
      box-shadow: 0 0 16px var(--accent-glow);
    }

    .header-text h1 {
      font-size: 17px;
      font-weight: 700;
      letter-spacing: 0.5px;
      color: var(--text);
    }

    .header-text p {
      font-size: 11px;
      color: var(--text-muted);
      letter-spacing: 0.3px;
      margin-top: 1px;
    }

    /* ── Status Badge ── */
    .ws-status {
      margin-left: auto;
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      color: var(--text-muted);
      background: var(--surface2);
      padding: 5px 10px;
      border-radius: 20px;
      border: 1px solid var(--border);
      flex-shrink: 0;
    }

    .ws-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #ff4e6a;
      transition: background 0.4s;
    }

    .ws-dot.connected { background: #00e676; box-shadow: 0 0 6px #00e676; }

    /* ── Main Content ── */
    main {
      max-width: 520px;
      margin: 0 auto;
      padding: 16px 16px 0;
    }

    /* ── Section Card ── */
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 18px;
      margin-bottom: 14px;
      box-shadow: var(--shadow);
    }

    .card-title {
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 1.2px;
      text-transform: uppercase;
      color: var(--accent);
      margin-bottom: 16px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .card-title::after {
      content: '';
      flex: 1;
      height: 1px;
      background: var(--border);
    }

    /* ── Slider Rows ── */
    .slider-row {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 16px;
    }

    .slider-row:last-child { margin-bottom: 0; }

    .slider-label {
      font-size: 13px;
      color: var(--text-muted);
      width: 80px;
      flex-shrink: 0;
    }

    .slider-wrap {
      flex: 1;
      position: relative;
    }

    input[type="range"] {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: var(--border);
      outline: none;
      cursor: pointer;
    }

    input[type="range"]::-webkit-slider-runnable-track {
      height: 6px;
      border-radius: 3px;
    }

    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: var(--accent);
      box-shadow: 0 0 8px var(--accent-glow);
      cursor: pointer;
      margin-top: -7px;
      transition: transform 0.15s, box-shadow 0.15s;
    }

    input[type="range"]::-webkit-slider-thumb:active {
      transform: scale(1.2);
      box-shadow: 0 0 14px rgba(0,200,255,0.5);
    }

    .slider-val {
      font-size: 14px;
      font-weight: 700;
      color: var(--accent);
      width: 32px;
      text-align: right;
      flex-shrink: 0;
    }

    /* ── Button Groups ── */
    .btn-group {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }

    .btn-opt {
      flex: 1;
      min-width: 60px;
      padding: 10px 8px;
      border: 1.5px solid var(--border);
      border-radius: var(--radius-sm);
      background: var(--surface2);
      color: var(--text-muted);
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      text-align: center;
      transition: all 0.2s;
      -webkit-tap-highlight-color: transparent;
      user-select: none;
    }

    .btn-opt:hover { border-color: var(--accent-dim); color: var(--text); }

    .btn-opt.active {
      background: linear-gradient(135deg, rgba(0,200,255,0.15), rgba(0,200,255,0.05));
      border-color: var(--accent);
      color: var(--accent);
      box-shadow: 0 0 10px var(--accent-glow);
    }

    /* ── Zoom Buttons ── */
    .zoom-group {
      display: flex;
      gap: 0;
    }

    .zoom-group .btn-opt {
      border-radius: 0;
      flex: 1;
      border-right-width: 0;
    }

    .zoom-group .btn-opt:first-child { border-radius: var(--radius-sm) 0 0 var(--radius-sm); }
    .zoom-group .btn-opt:last-child  { border-radius: 0 var(--radius-sm) var(--radius-sm) 0; border-right-width: 1.5px; }

    /* ── Input Fields ── */
    .input-row {
      display: flex;
      gap: 12px;
    }

    .input-group {
      flex: 1;
      display: flex;
      flex-direction: column;
      gap: 8px;
    }

    .input-group label {
      font-size: 12px;
      color: var(--text-muted);
      font-weight: 600;
      letter-spacing: 0.3px;
    }

    .input-group input[type="number"] {
      background: var(--surface2);
      border: 1.5px solid var(--border);
      border-radius: var(--radius-sm);
      color: var(--text);
      font-size: 16px;
      font-weight: 600;
      padding: 12px 14px;
      width: 100%;
      outline: none;
      text-align: center;
      /* hide spinners */
      -moz-appearance: textfield;
      transition: border-color 0.2s, box-shadow 0.2s;
    }

    .input-group input[type="number"]::-webkit-outer-spin-button,
    .input-group input[type="number"]::-webkit-inner-spin-button { -webkit-appearance: none; }

    .input-group input[type="number"]:focus {
      border-color: var(--accent);
      box-shadow: 0 0 10px var(--accent-glow);
    }

    /* ── Toast ── */
    #toast {
      position: fixed;
      bottom: 90px;
      left: 50%;
      transform: translateX(-50%) translateY(20px);
      background: #1e2d20;
      border: 1px solid #00e676;
      color: #00e676;
      padding: 10px 22px;
      border-radius: 24px;
      font-size: 13px;
      font-weight: 600;
      opacity: 0;
      pointer-events: none;
      transition: all 0.3s;
      z-index: 200;
      white-space: nowrap;
    }

    #toast.show {
      opacity: 1;
      transform: translateX(-50%) translateY(0);
    }

    #toast.error {
      background: #2d1e1e;
      border-color: var(--danger);
      color: var(--danger);
    }

    /* ── Save Button ── */
    #saveBar {
      position: fixed;
      bottom: 0;
      left: 0;
      right: 0;
      padding: 12px 16px;
      background: linear-gradient(to top, #0d0f14 60%, transparent);
      z-index: 150;
    }

    #saveBtn {
      display: block;
      width: 100%;
      max-width: 520px;
      margin: 0 auto;
      padding: 16px;
      background: linear-gradient(135deg, #00c8ff, #0096cc);
      color: #0d0f14;
      border: none;
      border-radius: var(--radius);
      font-size: 15px;
      font-weight: 800;
      letter-spacing: 1px;
      text-transform: uppercase;
      cursor: pointer;
      box-shadow: 0 4px 20px rgba(0,200,255,0.35);
      transition: transform 0.15s, box-shadow 0.15s, filter 0.15s;
      -webkit-tap-highlight-color: transparent;
    }

    #saveBtn:active {
      transform: scale(0.97);
      box-shadow: 0 2px 10px rgba(0,200,255,0.2);
      filter: brightness(0.92);
    }

    #saveBtn:disabled {
      background: var(--border);
      color: var(--text-muted);
      box-shadow: none;
      cursor: not-allowed;
    }
  </style>
</head>

<body>

  <!-- ── Header ── -->
  <header>
    <div class="header-icon">🌡️</div>
    <div class="header-text">
      <h1>TISA506M</h1>
      <p>Thermal Imager Control Panel</p>
    </div>
    <div class="ws-status" id="wsStatus">
      <div class="ws-dot" id="wsDot"></div>
      <span id="wsLabel">Connecting…</span>
    </div>
  </header>

  <!-- ── Main ── -->
  <main>

    <!-- Sliders -->
    <div class="card">
      <div class="card-title">📊 Image Parameters</div>

      <div class="slider-row">
        <span class="slider-label">Brightness</span>
        <div class="slider-wrap">
          <input type="range" id="brightness" min="1" max="100" value="50" />
        </div>
        <span class="slider-val" id="brightnessVal">50</span>
      </div>

      <div class="slider-row">
        <span class="slider-label">Contrast</span>
        <div class="slider-wrap">
          <input type="range" id="contrast" min="1" max="100" value="50" />
        </div>
        <span class="slider-val" id="contrastVal">50</span>
      </div>

      <div class="slider-row">
        <span class="slider-label">Denoise</span>
        <div class="slider-wrap">
          <input type="range" id="denoise" min="1" max="10" value="5" />
        </div>
        <span class="slider-val" id="denoiseVal">5</span>
      </div>

      <div class="slider-row">
        <span class="slider-label">Sharpness</span>
        <div class="slider-wrap">
          <input type="range" id="sharpness" min="1" max="10" value="5" />
        </div>
        <span class="slider-val" id="sharpnessVal">5</span>
      </div>
    </div>

    <!-- AGC Mode -->
    <div class="card">
      <div class="card-title">⚙️ AGC Mode</div>
      <div class="btn-group" id="agcGroup">
        <button class="btn-opt active" data-agc="AutoBG">AutoBG</button>
        <button class="btn-opt" data-agc="AutoFG">AutoFG</button>
        <button class="btn-opt" data-agc="EWBG">EWBG</button>
        <button class="btn-opt" data-agc="EWFG">EWFG</button>
        <button class="btn-opt" data-agc="Manual">Manual</button>
      </div>
    </div>

    <!-- Zoom -->
    <div class="card">
      <div class="card-title">🔍 Zoom Level</div>
      <div class="zoom-group" id="zoomGroup">
        <button class="btn-opt active" data-zoom="1">1X</button>
        <button class="btn-opt" data-zoom="2">2X</button>
        <button class="btn-opt" data-zoom="4">4X</button>
      </div>
    </div>

    <!-- Boresight -->
    <div class="card">
      <div class="card-title">🎯 Boresight</div>
      <div class="input-row">
        <div class="input-group">
          <label for="boresightX">X Offset</label>
          <input type="number" id="boresightX" value="0" placeholder="0" />
        </div>
        <div class="input-group">
          <label for="boresightY">Y Offset</label>
          <input type="number" id="boresightY" value="0" placeholder="0" />
        </div>
      </div>
    </div>

  </main>

  <!-- ── Toast ── -->
  <div id="toast">✅ Settings Saved!</div>

  <!-- ── Save Bar ── -->
  <div id="saveBar">
    <button id="saveBtn">💾 Save Settings</button>
  </div>

  <!-- ── JavaScript ── -->
  <script>
    // ── Global State ──────────────────────────────
    const state = {
      brightness: 50,
      contrast:   50,
      denoise:    5,
      sharpness:  5,
      agc:        'AutoBG',
      zoom:       1,
      x:          0,
      y:          0
    };

    // ── WebSocket ─────────────────────────────────
    let ws;
    let reconnectTimer = null;

    function connectWS() {
      ws = new WebSocket('ws://192.168.4.1/ws');

      ws.onopen = () => {
        setWsStatus(true);
        if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
      };

      ws.onclose = () => {
        setWsStatus(false);
        reconnectTimer = setTimeout(connectWS, 3000);
      };

      ws.onerror = () => {
        ws.close();
      };

      ws.onmessage = (evt) => {
        // Future: handle ACK or feedback from ESP32
        console.log('WS msg:', evt.data);
      };
    }

    function setWsStatus(connected) {
      document.getElementById('wsDot').classList.toggle('connected', connected);
      document.getElementById('wsLabel').textContent = connected ? 'Connected' : 'Offline';
      document.getElementById('saveBtn').disabled = !connected;
    }

    // ── Sliders ───────────────────────────────────
    const sliders = ['brightness', 'contrast', 'denoise', 'sharpness'];
    sliders.forEach(id => {
      const el  = document.getElementById(id);
      const val = document.getElementById(id + 'Val');

      // Gradient fill on slider track
      function updateTrack() {
        const pct = ((el.value - el.min) / (el.max - el.min)) * 100;
        el.style.background =
          `linear-gradient(to right, var(--accent) ${pct}%, var(--border) ${pct}%)`;
        val.textContent = el.value;
        state[id] = parseInt(el.value);
      }

      el.addEventListener('input', updateTrack);
      updateTrack(); // init
    });

    // ── AGC Buttons ───────────────────────────────
    document.getElementById('agcGroup').addEventListener('click', e => {
      const btn = e.target.closest('.btn-opt');
      if (!btn) return;
      document.querySelectorAll('#agcGroup .btn-opt').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.agc = btn.dataset.agc;
    });

    // ── Zoom Buttons ──────────────────────────────
    document.getElementById('zoomGroup').addEventListener('click', e => {
      const btn = e.target.closest('.btn-opt');
      if (!btn) return;
      document.querySelectorAll('#zoomGroup .btn-opt').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.zoom = parseInt(btn.dataset.zoom);
    });

    // ── Boresight Fields ──────────────────────────
    document.getElementById('boresightX').addEventListener('input', e => {
      state.x = parseInt(e.target.value) || 0;
    });
    document.getElementById('boresightY').addEventListener('input', e => {
      state.y = parseInt(e.target.value) || 0;
    });

    // ── Toast Helper ──────────────────────────────
    let toastTimer = null;
    function showToast(msg, isError = false) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.classList.toggle('error', isError);
      t.classList.add('show');
      if (toastTimer) clearTimeout(toastTimer);
      toastTimer = setTimeout(() => t.classList.remove('show'), 2800);
    }

    // ── Save Button ───────────────────────────────
    document.getElementById('saveBtn').addEventListener('click', () => {
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        showToast('❌ Not connected to device!', true);
        return;
      }

      // Sync boresight from inputs (in case user typed without triggering input event)
      state.x = parseInt(document.getElementById('boresightX').value) || 0;
      state.y = parseInt(document.getElementById('boresightY').value) || 0;

      const payload = JSON.stringify(state);
      ws.send(payload);
      console.log('Sent:', payload);
      showToast('✅ Settings Transmitted!');
    });

    // ── Init ──────────────────────────────────────
    setWsStatus(false);
    connectWS();
  </script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────────
//  UART Packet Functions
// ─────────────────────────────────────────────

/**
 * Send a 4-byte brightness packet: [0xAA, 0x01, value, 0xFF]
 * value range: 1–100
 */
void setBrightness(uint8_t value) {
  Serial2.write(0xAA);
  Serial2.write(0x01);
  Serial2.write(value);
  Serial2.write(0xFF);
  Serial.printf("[UART] Brightness packet: [AA 01 %02X FF]\n", value);
}

/**
 * Send a 4-byte contrast packet: [0xAA, 0x02, value, 0xFF]
 * value range: 1–100
 */
void setContrast(uint8_t value) {
  Serial2.write(0xAA);
  Serial2.write(0x02);
  Serial2.write(value);
  Serial2.write(0xFF);
  Serial.printf("[UART] Contrast packet:   [AA 02 %02X FF]\n", value);
}

/**
 * Send a 4-byte denoise packet: [0xAA, 0x03, value, 0xFF]
 * value range: 1–10
 */
void setDenoise(uint8_t value) {
  Serial2.write(0xAA);
  Serial2.write(0x03);
  Serial2.write(value);
  Serial2.write(0xFF);
  Serial.printf("[UART] Denoise packet:    [AA 03 %02X FF]\n", value);
}

/**
 * Send a 4-byte sharpness packet: [0xAA, 0x04, value, 0xFF]
 * value range: 1–10
 */
void setSharpness(uint8_t value) {
  Serial2.write(0xAA);
  Serial2.write(0x04);
  Serial2.write(value);
  Serial2.write(0xFF);
  Serial.printf("[UART] Sharpness packet:  [AA 04 %02X FF]\n", value);
}

/**
 * Send AGC mode packet: [0xAA, 0x05, mode, 0xFF]
 * AutoBG=1, AutoFG=2, EWBG=3, EWFG=4, Manual=5
 */
void setAGC(const String& modeName) {
  uint8_t mode = 1;
  if      (modeName == "AutoBG") mode = 1;
  else if (modeName == "AutoFG") mode = 2;
  else if (modeName == "EWBG")   mode = 3;
  else if (modeName == "EWFG")   mode = 4;
  else if (modeName == "Manual") mode = 5;
  else {
    Serial.println("[UART] ERROR: Unknown AGC mode: " + modeName);
    return;
  }
  Serial2.write(0xAA);
  Serial2.write(0x05);
  Serial2.write(mode);
  Serial2.write(0xFF);
  Serial.printf("[UART] AGC packet:        [AA 05 %02X FF] (%s)\n", mode, modeName.c_str());
}

/**
 * Send zoom level packet: [0xAA, 0x06, level, 0xFF]
 * 1X=1, 2X=2, 4X=3
 */
void setZoom(int zoomFactor) {
  uint8_t level = 1;
  if      (zoomFactor == 1) level = 1;
  else if (zoomFactor == 2) level = 2;
  else if (zoomFactor == 4) level = 3;
  else {
    Serial.println("[UART] ERROR: Unknown zoom factor");
    return;
  }
  Serial2.write(0xAA);
  Serial2.write(0x06);
  Serial2.write(level);
  Serial2.write(0xFF);
  Serial.printf("[UART] Zoom packet:       [AA 06 %02X FF] (%dX)\n", level, zoomFactor);
}

/**
 * Send boresight packet: [0xAA, 0x07, X (as int8_t), Y (as int8_t), 0xFF]
 * Supports negative offsets via signed cast.
 */
void setBoresight(int x, int y) {
  // Cast to int8_t to transmit negative values as two's complement
  int8_t bx = (int8_t)constrain(x, -127, 127);
  int8_t by = (int8_t)constrain(y, -127, 127);
  Serial2.write(0xAA);
  Serial2.write(0x07);
  Serial2.write((uint8_t)bx);
  Serial2.write((uint8_t)by);
  Serial2.write(0xFF);
  Serial.printf("[UART] Boresight packet:  [AA 07 %02X %02X FF] (X=%d, Y=%d)\n",
                (uint8_t)bx, (uint8_t)by, x, y);
}

// ─────────────────────────────────────────────
//  Transmit All Settings via UART
// ─────────────────────────────────────────────
void transmitAllSettings() {
  Serial.println("─────────────────────────────────");
  Serial.println("[UART] === Transmitting All Settings ===");
  Serial.printf ("[UART] Brightness=%d, Contrast=%d, Denoise=%d, Sharpness=%d\n",
                 g_brightness, g_contrast, g_denoise, g_sharpness);
  Serial.printf ("[UART] AGC=%s, Zoom=%dX, BoresightX=%d, BoresightY=%d\n",
                 g_agc.c_str(), g_zoom, g_boresightX, g_boresightY);
  Serial.println("─────────────────────────────────");

  setBrightness((uint8_t)g_brightness);
  setContrast  ((uint8_t)g_contrast);
  setDenoise   ((uint8_t)g_denoise);
  setSharpness ((uint8_t)g_sharpness);
  setAGC       (g_agc);
  setZoom      (g_zoom);
  setBoresight (g_boresightX, g_boresightY);

  Serial.println("[UART] === Transmission Complete ===");
  Serial.println("─────────────────────────────────");
}

// ─────────────────────────────────────────────
//  WebSocket Event Handler
// ─────────────────────────────────────────────
void onEvent(AsyncWebSocket*       server,
             AsyncWebSocketClient* client,
             AwsEventType          type,
             void*                 arg,
             uint8_t*              data,
             size_t                len)
{
  switch (type) {

    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connected from %s\n",
                    client->id(),
                    client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u disconnected\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;

      // Only process complete, text frames
      if (info->final && info->index == 0 && info->len == len &&
          info->opcode == WS_TEXT) {

        // Null-terminate for string operations
        char buf[512];
        size_t msgLen = min(len, (size_t)511);
        memcpy(buf, data, msgLen);
        buf[msgLen] = '\0';

        Serial.printf("[WS] Received from #%u: %s\n", client->id(), buf);

        // Parse JSON
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, buf);

        if (err) {
          Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
          client->text("{\"status\":\"error\",\"msg\":\"Invalid JSON\"}");
          break;
        }

        // ── Store parsed values ──
        if (doc.containsKey("brightness"))
          g_brightness = constrain((int)doc["brightness"], 1, 100);

        if (doc.containsKey("contrast"))
          g_contrast = constrain((int)doc["contrast"], 1, 100);

        if (doc.containsKey("denoise"))
          g_denoise = constrain((int)doc["denoise"], 1, 10);

        if (doc.containsKey("sharpness"))
          g_sharpness = constrain((int)doc["sharpness"], 1, 10);

        if (doc.containsKey("agc"))
          g_agc = doc["agc"].as<String>();

        if (doc.containsKey("zoom"))
          g_zoom = (int)doc["zoom"];

        if (doc.containsKey("x"))
          g_boresightX = (int)doc["x"];

        if (doc.containsKey("y"))
          g_boresightY = (int)doc["y"];

        Serial.println("[WS] Parameters stored. Triggering UART transmission...");

        // ── Trigger UART only on Save (this is the Save payload) ──
        transmitAllSettings();

        // ── Acknowledge to browser ──
        client->text("{\"status\":\"ok\",\"msg\":\"Settings saved\"}");
      }
      break;
    }

    case WS_EVT_ERROR:
      Serial.printf("[WS] Error on client #%u\n", client->id());
      break;

    default:
      break;
  }
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
  // Debug serial (USB)
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n╔══════════════════════════════════╗");
  Serial.println("║  TISA506M Control Panel v1.0     ║");
  Serial.println("╚══════════════════════════════════╝");

  // UART2 (to thermal imager)
  Serial2.begin(UART2_BAUD, SERIAL_8N1, UART2_RX, UART2_TX);
  Serial.printf("[UART2] Initialized on TX=GPIO%d, RX=GPIO%d @ %d baud\n",
                UART2_TX, UART2_RX, UART2_BAUD);

  // WiFi Access Point
  Serial.printf("[WiFi] Starting AP: SSID=%s\n", AP_SSID);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] AP IP: %s\n", ip.toString().c_str());
  Serial.println("[WiFi] Connect to SSID 'TISA506M' then open http://192.168.4.1");

  // WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // HTTP root — serve index page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // 404 handler
  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
  });

  // Start server
  server.begin();
  Serial.println("[HTTP] Server started on port 80");
  Serial.println("────────────── Ready ──────────────");
}

// ─────────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────────
void loop() {
  // Clean up disconnected WebSocket clients periodically
  // (ESPAsyncWebServer handles most work in callbacks)
  ws.cleanupClients();

  // No blocking delay() — all logic is event-driven
}
