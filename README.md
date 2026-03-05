# Universal YouTube Subscriber Tracker

A high-performance, mobile-responsive YouTube Subscriber Tracker designed for the ESP32 family (C6, C3, S3, and Classic). Featuring a non-blocking LED matrix engine, custom web dashboard, and robust WiFi handshaking logic designed to maintain stability across various home network architectures and WiFi extenders.

---

## Key Features

* **Auto-Hardware Sensing:** Automatically detects board type and assigns correct SPI pins via compiler flags.
* **WiFi Stability Engine:** Implements `WIFI_PS_NONE` to disable radio sleep, preventing disconnects on modern routers.
* **Web Dashboard:** Fully responsive interface for real-time configuration of API keys, Sleep/Wake schedules, and channel IDs.
* **Secure OTA Updates:** Integrated Over-The-Air firmware portal using optimized partition schemes.
* **Smart Power Management:** Automated display shutdown during user-defined sleep hours to preserve LED longevity.
* **Asynchronous Execution:** Non-blocking LED animation logic ensures the web server remains responsive even during heavy scrolling.

---

## Hardware & Pin Mapping

The firmware includes auto-detection logic that identifies your board at compile time. No manual pin editing is required for supported models.

| Board / SoC | CS Pin | MOSI | CLK |
| :--- | :---: | :---: | :---: |
| Seeed XIAO ESP32-C6 | 1 | 21 | 19 |
| Seeed XIAO ESP32-C3 | 3 | 10 | 8 |
| Seeed XIAO ESP32-S3 | 1 | 9 | 7 |
| ESP32-C3 SuperMini | 7 | 6 | 4 |
| Seeed XIAO ESP32 (Classic) | 5 | 23 | 18 |
| Adafruit Feather ESP32 | 33 | 18 | 5 |
| ESP32-S2 DevKit | 15 | 35 | 36 |
| ESP32-S3 DevKit | 10 | 11 | 12 |
| Standard DevKit V1 (30p/38p) | 5 | 23 | 18 |

---

## Setup & Configuration

### 1. Arduino IDE Requirements
* **Core:** ESP32 by Espressif (v3.0+ recommended for C6 support).
* **Board Settings:** Select your specific module (e.g., "XIAO_ESP32C6").
* **USB CDC On Boot:** Enabled (Required to view Serial Monitor diagnostics).
* **Partition Scheme:** Minimal SPIFFS (1.9MB APP with OTA). This is critical for OTA functionality.

### 2. Libraries
Ensure the following are installed via the Library Manager:
* `MD_Parola` & `MD_MAX72xx`
* `ArduinoJson`
* `WiFi`, `HTTPClient`, `WebServer`, `Update`, `Preferences`, `DNSServer`

### 3. Network Deployment
If the device cannot find a known network, it creates a Rescue Hotspot:
1. Connect to WiFi AP: `OneCircuit-Config`
2. Open Browser to: `192.168.4.1`
3. Enter your WiFi credentials, YouTube API Key, and Channel ID.

---

## About OneCircuit
Created by OneCircuit and Gemini for the maker community. This project focuses on professional-grade code for hobbyist hardware.

* **YouTube:** [OneCircuit YouTube Channel](https://www.youtube.com/@onecircuit-as)
* **Blog:** [OneCircuit Blog](https://onecircuit.blogspot.com/)
* **GitHub:** [OneCircuit Repositories](https://github.com/bovineck/)

---

### License
This project is licensed under the MIT License - feel free to build, modify, and share!
