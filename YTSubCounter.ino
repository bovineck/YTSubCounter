/*
 PROJECT: OneCircuit YouTube Subscriber Tracker
 AUTHOR:  OneCircuit and Gemini AI Sat 07 Mar 2026 12:39:22 AEDT

https://www.youtube.com/@onecircuit-as
https://onecircuit.blogspot.com/
https://github.com/bovineck/

COMPILE SETTINGS:
 Board: ESP32 Dev Module (or your specific variant)
 Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA) <-- CRITICAL
 
 * [SECTION 1] - Dependencies
 Loads the core libraries for WiFi, Web Server hosting, and JSON parsing.
 Ref: https://github.com/bblanchon/ArduinoJson (Excellent documentation for data parsing)

 * [SECTION 2] - Initial Configuration
 These strings are only used if the device has never been configured before. 
 Once saved via the dashboard, the device will prioritize the Flash memory settings.

 * [SECTION 3] - Auto-Hardware Detection
 Uses compiler "flags" to detect which ESP32 board you are using and automatically 
 assigns the correct SPI pins for the LED matrix. No manual pin editing required.
 Ref: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html

 * [SECTION 4] - Global Objects & State
 Sets up the Parola display engine, the Web Server on port 80, and the Preferences
 "Flash" storage. Also contains the custom pixel data for the "blinking dog" animation.

 * [SECTION 5] - CSS Styling
 The "Visual DNA" of the dashboard. Uses CSS Flexbox to ensure the interface looks
 professional on both desktop monitors and mobile phone screens.

 * [SECTION 6] - Dashboard Logic
 The main HTML engine. It reads the current status from the hardware and generates
 the interactive "Live" dashboard that the user sees in their browser.

 * [SECTION 7] - Utility Routes
 Handles the "behind the scenes" web requests for saving settings, showing the
 wiring map, and triggering the secure OTA (Over-The-Air) firmware update process.
 Ref: https://github.com/espressif/arduino-esp32/tree/master/libraries/Update

 * [SECTION 8] - YouTube API Engine
 The "Heart" of the device. Connects to Google's servers, requests your stats,
 and updates the local display. It also manages the Sleep/Wake power-saving schedule.
 Ref: https://developers.google.com/youtube/v3/docs/channels/list

 * [SECTION 9] - System Setup
 Runs once at power-on. It wakes up the display, tries to find your WiFi, and
 if it fails, creates a "Rescue Hotspot" (AP Mode) so you can fix the settings.

 * [SECTION 10] - Execution Loop
 The non-stop worker. Animates the LED matrix, checks the YouTube API every 60 seconds,
 and keeps the web server responsive to your clicks.
 */

// [SECTION 1] - Dependencies
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "time.h"

// [SECTION 2] - Initial Configuration
const char* INITIAL_SSID = "Your WiFi SSID";
const char* INITIAL_PASS = "Your WiFi Password";
const char* INITIAL_CNAME = "Your Channel Name";
const char* INITIAL_LOC = "Your Location";
const char* PROJECT_DESC = "A Universal YouTube Subscriber Tracker for ESP32. Featuring auto-hardware detection, OTA updates, and a mobile-friendly dashboard.";

// [SECTION 3] - Auto-Hardware Detection
#if defined(ARDUINO_SEEED_XIAO_ESP32C6) || defined(ARDUINO_XIAO_ESP32C6) || defined(ESP32C6)
#define CS_PIN 1
#define MOSI_PIN 21
#define CLK_PIN 19
const char* HW_NAME = "ESP32 C6";
#elif defined(ARDUINO_SEEED_XIAO_ESP32C3) || defined(ARDUINO_XIAO_ESP32C3) || defined(ESP32C3)
#define CS_PIN 3
#define MOSI_PIN 10
#define CLK_PIN 8
const char* HW_NAME = "ESP32 C3";
#elif defined(ARDUINO_SEEED_XIAO_ESP32S3) || defined(ARDUINO_XIAO_ESP32S3) || defined(ESP32S3)
#define CS_PIN 1
#define MOSI_PIN 9
#define CLK_PIN 7
const char* HW_NAME = "ESP32 S3";
#elif defined(ARDUINO_ESP32C3_DEV) || defined(ARDUINO_ESP32C3_SUPERMINI)
#define CS_PIN 7
#define MOSI_PIN 6
#define CLK_PIN 4
const char* HW_NAME = "ESP32-C3 SuperMini";
#elif defined(ARDUINO_SEEED_XIAO_ESP32)
#define CS_PIN 5
#define MOSI_PIN 23
#define CLK_PIN 18
const char* HW_NAME = "Seeed XIAO ESP32";
#elif defined(ARDUINO_FEATHER_ESP32)
#define CS_PIN 33
#define MOSI_PIN 18
#define CLK_PIN 5
const char* HW_NAME = "Adafruit Feather ESP32";
#elif defined(ARDUINO_ESP32S2_DEV)
#define CS_PIN 15
#define MOSI_PIN 35
#define CLK_PIN 36
const char* HW_NAME = "ESP32-S2 DevKit";
#elif defined(ARDUINO_ESP32S3_DEV)
#define CS_PIN 10
#define MOSI_PIN 11
#define CLK_PIN 12
const char* HW_NAME = "ESP32-S3 DevKit";
#elif defined(ARDUINO_ESP32_DEV) || defined(ESP32)
#define CS_PIN 5
#define MOSI_PIN 23
#define CLK_PIN 18
const char* HW_NAME = "DevKit V1 (Standard)";
#else
#define CS_PIN 5
#define MOSI_PIN 23
#define CLK_PIN 18
const char* HW_NAME = "Generic ESP32";
#endif

// [SECTION 4] - Global Objects & State
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 8
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WebServer server(80);
Preferences prefs;
DNSServer dnsServer;

String apiKey, channelId, channelName, location;
uint8_t sleepHour, wakeHour;
bool isAPMode = false, isBlinking = false;
long currentSubs = 0;
uint32_t lastUpdate = 0, lastBlink = 0;
char displayMsg[48] = "READY";  // keep small so the display doesn't choke up
String lastTimeCheck = "Never";
unsigned long wifiTimeout = 0;
uint8_t curTR = 10;
uint8_t scrollState = 0;

void handleRoot();
void handleSave();
void handleHelp();
void handlePins();
void updateYouTubeData();
void handleUpdate();

// [SECTION 5] - CSS Styling
String getCSS() {
  String css = "<style>body{font-family:'Segoe UI',sans-serif; background:#1e293b; color:#f1f5f9; padding:15px; max-width:600px; margin:auto;} ";
  css += ".box{background:#334155; padding:18px; border-radius:12px; margin-bottom:20px; border-left:6px solid #38bdf8; ";
  css += "box-shadow: 0 10px 15px -3px rgba(0,0,0,0.4), 0 4px 6px -2px rgba(0,0,0,0.2); box-sizing:border-box;} ";
  css += ".faq-box{background:#334155; padding:18px; border-radius:12px; margin-bottom:15px; border-left:6px solid #fbbf24; box-shadow: 0 10px 15px -3px rgba(0,0,0,0.4);} ";
  css += "input{display:block; width:100%; padding:14px; margin:10px 0; border-radius:8px; border:1px solid #475569; background:#1e293b; color:#fff; box-sizing: border-box;} ";
  css += "input:focus{outline:none; border-color:#38bdf8; box-shadow:0 0 0 2px rgba(56,189,248,0.2);} ";
  css += ".btn{padding:14px; border-radius:8px; color:#fff; cursor:pointer; text-decoration:none; display:inline-block; font-weight:bold; margin-top:10px; text-align:center; border:none; transition:0.2s;} ";
  css += ".btn:active{transform:scale(0.98);} .save{background:#10b981; width:100%; font-size:1.1em; margin-bottom:10px; box-shadow:0 4px 6px rgba(0,0,0,0.2);} ";
  css += ".hbtn{background:#f59e0b; flex:1; margin:5px;} .pbtn{background:#64748b; flex:1; margin:5px;} .reboot{background:#ef4444; flex:1; margin:5px;} ";
  css += ".pass-toggle{background:#475569; font-size:0.8em; padding:8px 12px; margin-bottom:15px; width:auto;} ";
  css += ".footer-nav{display:flex; flex-wrap:wrap; gap:10px; margin-top:25px; border-top:1px solid #475569; padding-top:15px;} ";
  css += ".footer-nav .btn{flex:1 1 140px; margin:0;} ";
  css += "a{color:#38bdf8; text-decoration:none;} table{width:100%; border-collapse: collapse; background:#334155; border-radius:8px; overflow:hidden;} ";
  css += "th, td{padding:12px; border-bottom:1px solid #475569; text-align:left;} th{background:#1e293b; color:#38bdf8;} .active{background:#0c4a6e; font-weight:bold;}</style>";
  return css;
}

// [SECTION 6] - Dashboard Logic
void handleRoot() {
  prefs.begin("config", true);
  String curSSID = prefs.getString("ssid", INITIAL_SSID);
  String curPASS = prefs.getString("pass", INITIAL_PASS);
  String curCN = prefs.getString("cname", INITIAL_CNAME);
  String curLO = prefs.getString("loc", INITIAL_LOC);
  String curAP = prefs.getString("api", "");
  String curCI = prefs.getString("cid", "");
  int curSL = prefs.getUChar("sleep", 23);
  int curWA = prefs.getUChar("wake", 7);
  curTR = prefs.getUChar("trigger", 10);
  prefs.end();

  String html = "<html><head><title>Dashboard</title><meta name='viewport' content='width=device-width, initial-scale=1'>" + getCSS();
  html += "<script>function togPass(){var p=document.getElementById('p'); p.type=(p.type==='password')?'text':'password';}</script></head><body>";
  html += "<h1>OneCircuit YouTube Subscriber Tracker</h1>";
  html += "<div class='box'><small>" + String(PROJECT_DESC) + "</small>";
  html += "<div style='margin-top:12px; border-top:1px solid #475569; padding-top:10px;'>";
  html += "<div style='font-size:0.8em; color:#94a3b8; margin-bottom:8px;'>Links:</div>";
  html += "<div style='display:flex; flex-wrap:wrap; gap:10px;'>";
  html += "<a href='https://www.youtube.com/@onecircuit-as' target='_blank' style='color:#38bdf8; font-size:0.85em; text-decoration:none;'>[ YouTube ]</a>";
  html += "<a href='https://onecircuit.blogspot.com/' target='_blank' style='color:#38bdf8; font-size:0.85em; text-decoration:none;'>[ Blog ]</a>";
  html += "<a href='https://github.com/bovineck/' target='_blank' style='color:#38bdf8; font-size:0.85em; text-decoration:none;'>[ GitHub ]</a>";
  html += "</div></div></div>";
  String statusColor = (WiFi.status() == WL_CONNECTED) ? "#10b981" : "#ef4444";  // Green if WiFi OK, else Red
  String statusText = (WiFi.status() == WL_CONNECTED) ? "Online" : "Offline";
  html += "<div class='box' style='border-left-color: " + statusColor + "; text-align: center; background: #0f172a; padding: 20px;'>";
  html += "<div style='display: flex; align-items: center; justify-content: center; gap: 8px; margin-bottom: 15px;'>";
  html += "<span style='height: 10px; width: 10px; background-color: " + statusColor + "; border-radius: 50%; display: inline-block; box-shadow: 0 0 8px " + statusColor + ";'></span>";
  html += "<span style='font-size: 0.75em; color: #94a3b8; text-transform: uppercase; letter-spacing: 1px;'>" + statusText + "</span>";
  html += "</div>";
  html += "<div style='font-size: 1.1em; font-weight: bold; color: #38bdf8; margin-bottom: 5px; letter-spacing: 1px;'>" + curCN + " : " + curLO + "</div>";
  html += "<div style='font-size: 2.8em; font-weight: 800; color: #fff; margin: 5px 0;'>" + String(currentSubs) + "</div>";
  html += "<div style='font-size: 0.8em; color: #94a3b8;'>Last Sync: " + lastTimeCheck + "</div>";
  html += "<div style='margin-top: 15px; padding-top: 10px; border-top: 1px solid #1e293b; font-family: monospace; font-size: 0.85em; color: #64748b;'>";
  html += "IP: " + WiFi.localIP().toString() + "<br>";
  html += "URL: <a href='http://onecircuit.local' style='color: #38bdf8;'>http://onecircuit.local</a>";
  html += "</div>";
  html += "<a href='/refresh' class='btn' style='margin-top: 15px; text-decoration: none; display: block; background: #059669; color: white; font-weight: bold; padding: 10px; border-radius: 8px;'>Check YouTube Now</a>";
  html += "</div>";
  html += "<form action='/save' method='POST'>";
  html += "<h3>Identity</h3>";
  html += "<input name='cn' placeholder='Your Channel Name' value='" + curCN + "'>";
  html += "<input name='lo' placeholder='Your Location' value='" + curLO + "'>";
  html += "<h3>Network</h3>";
  html += "<input name='ss' placeholder='Your Wifi SSID' value='" + curSSID + "'>";
  html += "<input type='password' id='p' name='pa' placeholder='Your WiFi Password' value='" + curPASS + "'>";
  html += "<button type='button' class='btn pass-toggle' onclick='togPass()'>Show/Hide Password</button>";
  html += "<h3>Your YouTube API Key</h3>";
  html += "<input name='ap' placeholder='Your Channel API Key' value='" + curAP + "'>";
  html += "<h3>YouTube Channel ID</h3>";
  html += "<input name='ci' placeholder='Your Channel ID' value='" + curCI + "'>";
  html += "<h3>Schedule</h3>";
  html += "Sleep (0-23): <input type='number' name='sl' value='" + String(curSL) + "'>";
  html += "Wake (0-23): <input type='number' name='wa' value='" + String(curWA) + "'>";
  html += "<h3>Settings</h3>";
  html += "Scroll Cycle (Mins): <input type='number' name='tr' value='" + String(curTR) + "'>";
  html += "<input type='submit' value='Save & Preview Receipt' class='btn save'>";
  html += "</form>";
  html += "<div class='footer-nav'>";
  html += "<a href='/help' class='btn hbtn'>Help</a>";
  html += "<a href='/pins' class='btn pbtn'>Pin Wiring</a>";
  html += "<a href='/update' class='btn pbtn' style='background:#6366f1;'>Update Firmware</a>";
  html += "</div>";
  html += "<a href='/reboot_exec' class='btn reboot' style='width:100%; box-sizing:border-box; margin-top:10px;' onclick='return confirm(\"Reboot the device now?\")'>System Reboot</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// [SECTION 7] - Utility Routes
void handleUpdate() {
  String html = "<html><head><title>System Update</title><meta name='viewport' content='width=device-width, initial-scale=1'>" + getCSS();
  html += "<script>";
  html += "function startUpdate() {";
  html += "  document.getElementById('updater').style.display='none';";
  html += "  document.getElementById('status').style.display='block';";
  html += "}";
  html += "</script></head><body>";
  html += "<h1>Firmware Update</h1>";
  html += "<div id='status' class='box' style='display:none; text-align:center; border-left-color:#fbbf24;'>";
  html += "<h3>Flash in Progress...</h3>";
  html += "<p>Uploading binary to ESP32. <b>Do not power off.</b></p>";
  html += "<div style='margin:20px; font-weight:bold; color:#fbbf24;'>[ UPLOADING... ]</div></div>";
  html += "<div id='updater' class='box' style='border-left-color: #ef4444;'>";
  html += "<h3>Select .bin File</h3>";
  html += "<form method='POST' action='/update_exec' enctype='multipart/form-data' onsubmit='startUpdate()'>";
  html += "<input type='file' name='update' accept='.bin' style='padding:10px; border: 1px dashed #475569; background: #1e293b; margin-bottom:15px;'>";
  html += "<input type='submit' value='Begin Update' class='btn reboot' style='width:100%;'>";
  html += "</form></div>";
  html += "<a href='/' class='btn pbtn' style='width:100%; box-sizing:border-box;'>&larr; Cancel</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/// Saves settings and generates printable receipt.
void handleSave() {
  // 1. Capture and Save Settings to Flash
  String s_cn = server.arg("cn"), s_lo = server.arg("lo"), s_ss = server.arg("ss"),
         s_ap = server.arg("ap"), s_ci = server.arg("ci"), s_tr = server.arg("tr"),
         s_sl = server.arg("sl"), s_wa = server.arg("wa");

  prefs.begin("config", false);
  prefs.putString("ssid", s_ss);
  prefs.putString("pass", server.arg("pa"));
  prefs.putString("cname", s_cn);
  prefs.putString("loc", s_lo);
  prefs.putString("api", s_ap);
  prefs.putString("cid", s_ci);
  prefs.putUChar("sleep", (uint8_t)s_sl.toInt());
  prefs.putUChar("wake", (uint8_t)s_wa.toInt());
  prefs.putUChar("trigger", (uint8_t)s_tr.toInt());
  curTR = (uint8_t)s_tr.toInt();
  lastUpdate = millis();
  prefs.end();

  // 2. Build the HTML String
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>" + getCSS() + "</head><body>";

  // Receipt Box
  html += "<div class='box' style='background:#f8fafc; color:#1e293b; border-left-color:#10b981; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5);'>";
  html += "<h1>Config Receipt</h1><p>Verify details before rebooting.</p>";

  auto row = [&](String k, String v) {
    html += "<div style='border-bottom:1px solid #e2e8f0; padding:10px 0;'><b>" + k + ":</b> " + v + "</div>";
  };

  row("Channel Name", s_cn);
  row("Channel ID", s_ci);
  row("Wi-Fi SSID", s_ss);
  row("Scroll Cycle", s_tr + " minutes");
  row("Schedule", "Sleep at " + s_sl + ":00 / Wake at " + s_wa + ":00");

  html += "<div style='padding:10px 0;'><b>YouTube API Key:</b><br><small style='color:#64748b; word-break:break-all;'>" + s_ap + "</small></div>";

  // Action Buttons
  html += "<div style='display:flex; flex-direction:column; gap:10px; margin-top:15px;'>";
  html += "<button class='btn pbtn' onclick='window.print()'>1. Print / Save PDF</button>";
  html += "<a href='/reboot_exec' class='btn reboot'>2. Commit & Reboot Device</a>";
  html += "<a href='/' class='btn hbtn'>&larr; Return to Edit</a>";
  html += "</div></div>";

  // 3. System Health Check
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t sketchSize = ESP.getSketchSize();

  html += "<div class='box' style='background:#0f172a; border-left-color:#38bdf8; margin-top:20px; font-family:monospace; font-size:0.85em;'>";
  html += "<h3 style='color:#38bdf8; margin-top:0;'>System Health</h3>";
  html += "Free RAM: " + String(freeHeap / 1024) + " KB<br>";
  html += "Binary Size: " + String(sketchSize / 1024) + " KB / 1920 KB";
  html += "</div>";

  // 4. Danger Zone
  html += "<div class='box' style='border-left-color: #ef4444; margin-top: 40px; background:#1e293b;'>";
  html += "<h2>Danger Zone</h2><p style='font-size: 0.8em; color: #94a3b8;'>Wipe all WiFi credentials and API keys.</p>";
  html += "<a href='/wipe_exec' class='btn reboot' style='width:100%;' onclick=\"return confirm('Are you sure?')\">Factory Reset Device</a>";
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

/// Displays FAQ and partitioning guide.
void handleHelp() {
  String html = "<html><head><meta charset='UTF-8'><title>FAQ</title><meta name='viewport' content='width=device-width, initial-scale=1'>" + getCSS() + "</head><body>";
  html += "<h1>User Guide</h1>";

  // 1. OTA Troubleshooting
  html += "<div class='faq-box'><h2>OTA Update Information</h2>";
  html += "<b>1. Partition Scheme:</b> In Arduino IDE, please select: <br><i>Tools -> Partition Scheme -> Minimal SPIFFS. e.g. 1.9MB APP with OTA</i>.<br><br>";
  html += "Ref: <a href='https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html' target='_blank'>Espressif Partition Guide</a>.<br><br>";
  html += "<b>2. The right file:</b> Upload <b>YourVersion.ino.bin</b>. <span style='color:#ef4444;'>⚠️ DO NOT upload 'YourVersion.merged.bin' or 'YourVersion.bootloader.bin'</span> ... they will be too large for OTA and will likely cause an 'UPDATE FAIL' message.</div>";

  // 2. YouTube API
  html += "<div class='faq-box'><h2>YouTube API Key</h2>Enable 'YouTube Data API v3' at the <a href='https://console.cloud.google.com/' target='_blank'>Google Cloud Console</a>. Make sure your key has no IP restrictions that might block the ESP32.</div>";

  // 3. Channel ID
  html += "<div class='faq-box'><h2>Channel ID</h2>Your ID starts with 'UC...'. You can find it in your <a href='https://www.youtube.com/account_advanced' target='_blank'>YouTube Advanced Settings</a> or by clicking your profile icon > Settings > Advanced.</div>";

  // 4. Scroll Cycle
  html += "<div class='faq-box'><h2>Scroll Cycle</h2>The number of minutes between info scrolls. The display will show the Subscriber count primarily, then cycle through Location and Local Time based on this setting.</div>";

  // 5. Sleep & Wake
  html += "<div class='faq-box'><h2>Sleep & Wake Hr</h2>Uses 24-hour format (0-23). To extend the life of your 32x8 LED matrices (and save power!), the display will turn off during the Sleep Hour and resume at the Wake Hour.</div>";

  // 6. AP Mode
  html += "<div class='faq-box'><h2>AP Mode</h2>If the device cannot connect to your Wi-Fi, it creates its own hotspot: 'OneCircuit-Config'. <br>Connect with your phone and browse to 192.168.4.1 to update settings.</div>";

  html += "<a href='/' class='btn hbtn' style='width:100%; box-sizing:border-box;'>&larr; Back to Dashboard</a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

/// Displays wiring table.
void handlePins() {
  String html = "<html><head><title>Wiring Map</title><meta name='viewport' content='width=device-width, initial-scale=1'>" + getCSS() + "</head><body>";
  html += "<h1>Wiring Map</h1><div class='box'>Detected: <b>" + String(HW_NAME) + "</b></div><table><tr><th>Model</th><th>CS</th><th>MOSI</th><th>CLK</th></tr>";
  auto addR = [&](String n, int c, int m, int cl) {
    html += "<tr class='" + String(n == HW_NAME ? "active" : "") + "'><td>" + n + "</td><td>" + String(c) + "</td><td>" + String(m) + "</td><td>" + String(cl) + "</td></tr>";
  };
  addR("Seeed XIAO C6", 1, 21, 19);
  addR("Seeed XIAO C3", 3, 10, 8);
  addR("Seeed XIAO S3", 1, 9, 7);
  addR("ESP32-C3 SuperMini", 7, 6, 4);
  addR("Seeed XIAO ESP32", 5, 23, 18);
  addR("Adafruit Feather", 33, 18, 5);
  addR("ESP32-S2 DevKit", 15, 35, 36);
  addR("ESP32-S3 DevKit", 10, 11, 12);
  addR("DevKit V1 (30p)", 5, 23, 18);
  addR("Generic ESP32", 5, 23, 18);
  html += "</table><p>Official docs: <a href='https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/index.html' target='_blank'>Espressif Hardware Reference</a>.</p>";
  html += "<a href='/' class='btn pbtn'>&larr; Back</a></body></html>";
  server.send(200, "text/html", html);
}


/// Persistent redirect countdown page.
void sendTransitionPage(String title, String msg, int duration) {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<script>var seconds = " + String(duration) + "; function countdown(){seconds--; var el=document.getElementById('timer'); if(el) el.textContent=seconds; ";
  html += "if(seconds<=0){ document.getElementById('status-msg').innerHTML='<h2 style=\"color:#f59e0b;\">Action Complete</h2><p>Waiting for device to reconnect...</p><p style=\"font-size:0.8em; color:#94a3b8;\">If this takes too long, please press the <b>RESET</b> button, or cycle the power to the ESP32.</p><a href=\"/\" style=\"color:#38bdf8; text-decoration:none;\">Return to Dashboard</a>'; ";
  html += "setInterval(function(){window.location.href='/';},2000); } else {setTimeout(countdown,1000);}} setTimeout(countdown,500);</script>";
  html += "<style>body{background:#0f172a;color:white;font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center;padding:20px;} .loader{border:4px solid #1e293b;border-top:4px solid #38bdf8;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;margin:0 auto 20px auto;} @keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}</style></head><body>";
  html += "<div id='status-msg'><div class='loader'></div><h2>" + title + "</h2><p style='color:#94a3b8;'>" + msg + " in <span id='timer' style='color:#38bdf8; font-weight:bold;'>" + String(duration) + "</span> seconds...</p></div></body></html>";
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
}

// [SECTION 8] - YouTube API Engine
void updateYouTubeData() {
  Serial.println("\n>>> [SYNC START]");
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(3000);

  String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + channelId + "&key=" + apiKey;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    StaticJsonDocument<1536> doc;
    deserializeJson(doc, http.getString());
    const char* subString = doc["items"][0]["statistics"]["subscriberCount"];

    if (subString) {
      currentSubs = atol(subString);

      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeStringBuff[50];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M (%d %b)", &timeinfo);
        lastTimeCheck = String(timeStringBuff);
      }
      Serial.println(">>> DATA FETCHED SUCCESSFULLY.");
    }
  } else {
    Serial.printf(">>> API ERROR: %d\n", httpCode);
  }

  http.end();
  delay(100);
  Serial.println(">>> [SYNC COMPLETE]");
}

// [SECTION 9] - System Setup
void setup() {
  // 1. Start Serial Debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- DIAGNOSTIC BOOT ---");
  Serial.print("Hardware Detected: ");
  Serial.println(HW_NAME);

  // 2. Load Configuration from Flash
  prefs.begin("config", false);
  channelName = prefs.getString("cname", INITIAL_CNAME);
  location = prefs.getString("loc", INITIAL_LOC);
  apiKey = prefs.getString("api", "");
  channelId = prefs.getString("cid", "");
  sleepHour = prefs.getUChar("sleep", 23);
  wakeHour = prefs.getUChar("wake", 7);
  curTR = prefs.getUChar("trigger", 10);  // Load the saved value on boot
  String cSSID = prefs.getString("ssid", INITIAL_SSID);
  String cPASS = prefs.getString("pass", INITIAL_PASS);
  prefs.end();



  // 3. Initialize Hardware Display
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // Ensure the display is "Deselected" to start

  SPI.begin(CLK_PIN, -1, MOSI_PIN, CS_PIN);
  SPI.setFrequency(200000);  // slow SPI for stability
  myDisplay.begin();
  myDisplay.setIntensity(2);
  myDisplay.displayClear();
  myDisplay.print("READY");
  Serial.println("Display Initialized.");

  // 4. Robust WiFi Handshake
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Disable WiFi sleep to prevent the router from dropping the ESP32
  WiFi.setSleep(WIFI_PS_NONE);

  Serial.print("Connecting to WiFi: ");
  Serial.println(cSSID);
  WiFi.begin(cSSID.c_str(), cPASS.c_str());

  // Wait 15 seconds for connection with visual feedback
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 15000) {
    delay(500);
    Serial.print(".");
    myDisplay.print(".");
  }

  // 5. Post-Connection Logic
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nCONNECTED!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Start mDNS: http://onecircuit.local
    if (MDNS.begin("onecircuit")) {
      Serial.println("mDNS responder started.");
    }

    // Scroll IP Address across the matrix once
    String ipAddr = WiFi.localIP().toString();
    myDisplay.displayClear();
    // IN: Scroll Left | OUT: Scroll Up
    myDisplay.displayText(ipAddr.c_str(), PA_CENTER, 80, 2000, PA_SCROLL_LEFT, PA_SCROLL_UP);
    while (!myDisplay.displayAnimate()) { yield(); }

    // Sync Time for Sleep/Wake Schedule
    configTzTime("AEST-10AEDT,M10.1.0,M4.1.0/3", "pool.ntp.org");
    updateYouTubeData();
  } else {
    // Access Point Fallback (Rescue Mode)
    Serial.println("\nCONNECTION FAILED. Starting AP Mode.");
    isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OneCircuit-Config", NULL, 1, 0, 4);
    dnsServer.start(53, "*", WiFi.softAPIP());
    myDisplay.displayText("AP MODE", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  }

  // 6. Web Server Routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/help", handleHelp);
  server.on("/pins", handlePins);

  server.on("/refresh", []() {
    updateYouTubeData();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/wipe_exec", []() {
    prefs.begin("config", false);
    prefs.clear();
    prefs.end();
    sendTransitionPage("Factory Reset", "Wiping NVS and restarting...", 20);
    delay(1000);
    ESP.restart();
  });

  server.on("/reboot_exec", []() {
    sendTransitionPage("System Reboot", "Restarting device...", 20);
    delay(1000);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(2500);
    ESP.restart();
  });

  server.on("/update", handleUpdate);

  server.on(
    "/update_exec", HTTP_POST, []() {
      if (Update.hasError()) {
        server.send(200, "text/html", "Update Failed. Check Serial Monitor.");
      } else {
        sendTransitionPage("Update Successful", "Rebooting into new firmware...", 60);
        delay(2000);
        ESP.restart();
      }
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        myDisplay.displayShutdown(true);  // Turn off LEDs during flash
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("Update Success: %u bytes\n", upload.totalSize);
        else Update.printError(Serial);
      }
    });

  server.begin();
  Serial.println("HTTP Server Started.");
  Serial.println("Setup Finished.");
}

// [SECTION 10] - Execution Loop (v12.5 "The Interceptor")
void loop() {
  // 1. GLOBAL PRIORITY: Always check the server first
  server.handleClient();
  yield(); 

  if (isAPMode) {
    dnsServer.processNextRequest();
  } else {
    // 2. CHECK THE ANIMATION
    // We check this every single loop iteration, even if it's not finished
    bool isFinished = myDisplay.displayAnimate();

    if (isFinished) {
      // 3. MANDATORY BREATHER
      // We check the server again right here in case a request came in 
      // during the millisecond the display was "between" states.
      server.handleClient();
      
      myDisplay.displayShutdown(true); 
      delay(50); // Reduced delay to keep loop snappy

      // --- LOGIC: SEQUENCE vs NORMAL ---
      bool inSequence = (scrollState > 0 || (millis() - lastUpdate > (curTR * 60000UL)));

      if (inSequence) {
        if (scrollState == 0) { 
          updateYouTubeData(); 
          scrollState = 1; 
        }

        switch (scrollState) {
          case 1: snprintf(displayMsg, 48, "CH: %s", channelName.c_str()); scrollState = 2; break;
          case 2: snprintf(displayMsg, 48, "LOC: %s", location.c_str()); scrollState = 3; break;
          case 3: snprintf(displayMsg, 48, "SYNC: %s", lastTimeCheck.c_str()); scrollState = 4; break;
          case 4: lastUpdate = millis(); scrollState = 0; break;
        }
        myDisplay.displayText(displayMsg, PA_CENTER, 80, 2000, PA_SCROLL_LEFT, PA_SCROLL_UP);
      } else {
        static bool toggleName = true; 
        if (toggleName) {
          snprintf(displayMsg, 48, "%s", channelName.c_str());
          myDisplay.displayText(displayMsg, PA_CENTER, 80, 2000, PA_SCROLL_LEFT, PA_SCROLL_UP);
        } else {
          snprintf(displayMsg, 48, "Subs: %ld", currentSubs);
          myDisplay.displayText(displayMsg, PA_CENTER, 80, 2000, PA_SCROLL_UP, PA_SCROLL_UP);
        }
        toggleName = !toggleName;
      }

      myDisplay.displayClear();
      myDisplay.displayShutdown(false);
      myDisplay.displayReset();
    }

    // 4. HEARTBEAT
    if (millis() - lastBlink > (isBlinking ? 150 : 3500)) {
      isBlinking = !isBlinking;
      lastBlink = millis();
    }
  }
}