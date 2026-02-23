# Production-Ready ESP32 Firmware Project

Complete ESP32 firmware with **WiFi provisioning**, **automatic OTA updates**, and **safe firmware rollback**. Designed for production deployment with professional-grade reliability.

## 📋 Table of Contents

- [Features](#-features)
- [Architecture Overview](#-architecture-overview)
- [Project Structure](#-project-structure)
- [Quick Start](#-quick-start)
- [WiFi Provisioning System](#-wifi-provisioning-system)
- [OTA Update System](#-ota-update-system)
- [Firmware Rollback Safety](#-firmware-rollback-safety)
- [CI/CD Deployment](#-cicd-deployment)
- [Hardware Control](#-hardware-control)
- [Production Best Practices](#-production-best-practices)
- [Troubleshooting](#-troubleshooting)
- [API Reference](#-api-reference)

## ✨ Features

### 🔐 WiFi Provisioning
- **Hidden ESP32 hotspot** (`ESP32_CONFIG`) for first-time setup
- **Web configuration portal** for WiFi network selection
- **Secure credential storage** in NVS flash memory
- **Auto-reconnect logic** with exponential backoff
- **Fallback provisioning** if WiFi connection fails

### 🔄 OTA (Over-The-Air) Updates
- **Automatic version checking** from GitHub Releases
- **HTTPS download** with certificate verification
- **Resume capability** on interrupted downloads
- **SHA256 verification** for integrity checks
- **Smart scheduling** (checks every hour, configurable)

### 🛡️ Firmware Safety & Rollback
- **Dual OTA partition scheme** for safe updates
- **Automatic rollback** if new firmware fails to boot
- **Boot validation** confirms firmware stability
- **Partition slots** for A/B firmware updates
- **Factory reset** capability

### ⚡ Smart Hardware Control
- **GPIO 23 digital output** example with LED blinking
- **Status indicators** (WiFi, OTA, errors)
- **Configurable timing** for all operations
- **Non-blocking I/O** for real-time responsiveness

### 📊 Production Reliability
- **Comprehensive error handling** and logging
- **Watchdog timer** to prevent hangs
- **Health checks** before confirming updates
- **Version management** for firmware tracking
- **Debug serial output** with timestamps

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────┐
│              ESP32 FIRMWARE BOOT                │
└─────────────────────────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  1. Firmware Validation      │
        │  - Check OTA state           │
        │  - Mark valid if pending     │
        │  - Enable rollback safety    │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  2. Initialize GPIO & Serial │
        │  - Set GPIO 23 as output     │
        │  - Start debug logging       │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  3. WiFi Provisioning        │
        │  - Load saved credentials    │
        │  - Auto-connect to network   │
        │  - Or start AP portal        │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  4. Main Loop Operation      │
        │  - Control GPIO outputs      │
        │  - Check for OTA updates     │
        │  - Handle WiFi reconnects    │
        └──────────────────────────────┘
```

### OTA Update Flow

```
┌────────────────────────────────────────────────────────┐
│        Check for Update (Every 1 hour)                │
└────────────────────────────────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  Fetch version.json from:    │
        │  (GitHub Raw Content)        │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  Compare versions:           │
        │  Remote vs. Local            │
        └──────────────────────────────┘
                        ↓ (if new version)
        ┌──────────────────────────────┐
        │  Download firmware.bin       │
        │  with SHA256 verification    │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  Write to OTA partition      │
        │  (alternate partition)       │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  Reboot into new firmware    │
        │  Boot marked as PENDING      │
        └──────────────────────────────┘
                        ↓
        ┌──────────────────────────────┐
        │  New firmware validates      │
        │  Marks itself as VALID       │
        │  Cancels rollback            │
        └──────────────────────────────┘

[IF CRASH] → Automatic rollback to factory/previous
```

## 📁 Project Structure

```
espota/
├── .github/
│   └── workflows/
│       └── build-release.yml          # CI/CD automation
├── include/
│   ├── config.h                       # Configuration constants
│   ├── ota_manager.h                  # OTA update interface
│   └── wifi_manager.h                 # WiFi provisioning interface
├── src/
│   └── main.cpp                       # Main firmware code
├── platformio.ini                     # PlatformIO configuration
├── partitions_ota.csv                 # OTA partition table
├── version.txt                        # Current firmware version
├── version.json.example               # Example version metadata
└── README.md                          # This file

### Binary Outputs
.pio/build/esp32/
├── firmware.bin                       # Compiled firmware
└── firmware.elf                       # Debug symbols
```

## 🚀 Quick Start

### Prerequisites

- **ESP32 Development Board** (ESP32-DevKit-V1 or compatible)
- **PlatformIO CLI** or **PlatformIO IDE** extension for VS Code
- **Python 3.8+** (included with PlatformIO)
- **USB-to-Serial adapter** (usually built-in on DevKit boards)

### Installation Steps

1. **Clone or download this project:**
   ```bash
   git clone <your-repo-url>
   cd espota
   ```

2. **Install PlatformIO (if not already installed):**
   ```bash
   pip install platformio
   ```

3. **Build the project:**
   ```bash
   platformio run -e esp32
   ```

4. **Upload initial firmware (via USB):**
   ```bash
   platformio run -e esp32 --target upload
   ```

5. **Monitor serial output:**
   ```bash
   platformio device monitor -b 115200
   ```

6. **First time setup - WiFi Provisioning:**
   
   a. Device will boot and search for saved WiFi
   
   b. If no credentials found, it starts a hidden AP:
      - SSID: `ESP32_CONFIG`
      - Password: `esp32config2024`
   
   c. Connect to this AP from your phone/computer
   
   d. Open browser → `192.168.1.1`
   
   e. Select your WiFi network and enter password
   
   f. Device saves credentials and reboots

### Serial Output Example

```
[INFO] 100 ms: ============================================
[INFO] 101 ms: ESP32 FIRMWARE BOOT
[INFO] 102 ms: ============================================
Firmware Version: 1.0.0
[INFO] 150 ms: Serial initialized successfully
[INFO] 200 ms: Initializing GPIO pin 23...
[INFO] 210 ms: GPIO 23 configured as output (OFF)
[INFO] 300 ms: Starting firmware validation...
Running partition: ota_0
[INFO] 350 ms: Validating current firmware...
[INFO] 400 ms: Firmware marked as valid - rollback cancelled
[INFO] 500 ms: Attempting WiFi connection...
[INFO] 600 ms: Connecting to saved WiFi...
[INFO] 1200 ms: WiFi connected!
Connected SSID: MyHomeNetwork
IP Address: 192.168.1.100
Signal Strength: -67 dBm
[INFO] 1300 ms: WiFi provisioning complete
[INFO] 1400 ms: Setup complete!
Uptime: 1450 ms
```

## 🔌 WiFi Provisioning System

### How It Works

1. **First Boot Detection:**
   - ESP32 checks NVS flash for stored WiFi credentials
   - If not found, enters **provisioning mode**

2. **Access Point Creation:**
   - Creates hidden WiFi AP: `ESP32_CONFIG`
   - Broadcasts on channel 1-13 (configurable)
   - Requires password: `esp32config2024`

3. **Web Portal:**
   - User connects to AP
   - Opens browser to `192.168.1.1` (or `captive portal`)
   - Portal shows available WiFi networks
   - User selects network and enters password

4. **Credential Storage:**
   - Credentials saved to NVS partition
   - Encrypted by ESP32 hardware security
   - Persists across power cycles

5. **Auto-Reconnection:**
   - Next boot, ESP connects to saved network
   - Implements exponential backoff (1s, 2s, 4s, 8s, etc.)
   - If connection fails > 30 seconds, re-enters provisioning mode

6. **Reset/Re-provision:**
   - Double-reset button press to trigger re-provisioning
   - Or call `wifiManager.triggerReset()` via serial command
   - Or erase NVS: `platformio run --target erase`

### Configuration in `config.h`

```cpp
#define AP_SSID "ESP32_CONFIG"              // AP name
#define AP_PASSWORD "esp32config2024"       // AP password
#define AP_HIDDEN true                      // Hidden SSID
#define WIFI_TIMEOUT 180                    // Setup timeout (seconds)
#define WIFI_AUTOCONNECT true               // Auto-connect on boot
#define WIFI_PORTAL_TIMEOUT 300             // Portal timeout
```

### Troubleshooting WiFi

| Problem | Solution |
|---------|----------|
| Can't find ESP32_CONFIG AP | Ensure device is powered, wait 30 seconds |
| Portal won't load at 192.168.1.1 | Try captive portal (auth dialog should pop up) |
| Password rejected | Default is `esp32config2024`, not device password |
| WiFi connects then disconnects | Check network password typo, try 2.4GHz band |
| Provisioning won't exit | Reset with button or erase NVS: `platformio run --target erase` |

## 🔄 OTA Update System

### How It Works

1. **Version Checking (Every Hour):**
   - Fetches `version.json` from GitHub
   - Contains latest version, download URL, SHA256 hash

2. **Version Comparison:**
   - Compares remote version with `FIRMWARE_VERSION` in code
   - If remote is newer, triggers download

3. **Secure Download:**
   - Uses HTTPS with certificate verification
   - Downloads `firmware.bin` from GitHub Release
   - Verifies SHA256 checksum
   - Stores in alternate OTA partition (A/B process)

4. **Installation:**
   - Marks new firmware as "pending verification"
   - Reboots into new partition
   - New firmware validates itself (runs successfully)
   - Marks self as valid, cancels rollback

5. **Automatic Rollback (if new firmware crashes):**
   - ESP32 detects crash/hang
   - Automatically reverts to previous partition
   - Device restarts with previous firmware
   - No user intervention needed

### Setting Up OTA Updates

#### Step 1: Create GitHub Release

Create a release with firmware binary:

```bash
# Build firmware locally
platformio run -e esp32

# Push to GitHub with version tag
git tag v1.0.1
git push origin v1.0.1
```

GitHub Actions automatically:
- Builds firmware
- Calculates SHA256
- Creates release with `firmware.bin`
- Generates `version.json`

#### Step 2: Create `version.json`

The GitHub Actions workflow creates this automatically. Manual example:

```json
{
  "version": "1.0.1",
  "download_url": "firmware.bin",
  "sha256": "abc123def456...",
  "release_notes": "Bug fixes and stability improvements",
  "min_version": "0.9.0",
  "force_update": false,
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**Host this at:** `https://raw.githubusercontent.com/YOUR_USER/YOUR_REPO/main/version.json`

#### Step 3: Update Configuration

Edit `config.h`:

```cpp
#define GITHUB_OWNER "your-username"
#define GITHUB_REPO "espota"
#define OTA_CHECK_INTERVAL_SECONDS 3600  // Check every hour
```

### Checking OTA Status

Serial output shows OTA progress:

```
[INFO] Checking for OTA updates...
[INFO] Fetching version info from GitHub...
Latest version: 1.0.1
[INFO] Version info fetched successfully
New firmware available: 1.0.1 (current: 1.0.0)
[INFO] Starting firmware download...
Download URL: https://github.com/...
Firmware size: 1048576 bytes
[INFO] Writing firmware to flash...
OTA Progress: 10240/1048576 bytes (0%)
OTA Progress: 20480/1048576 bytes (1%)
...
[INFO] Firmware download complete
[INFO] Restarting in 3 seconds...
```

### Manual OTA via Web API

If Wi-Fi is connected, you can manually trigger OTA:

```bash
# Via HTTP POST to device IP
curl -X POST http://192.168.1.100/api/update
```

## 🛡️ Firmware Rollback Safety

### Why Rollback is Critical

Old OTA systems had a problem:
- If new firmware crashed during boot
- Device would repeatedly reboot (boot loop)
- User had to physically recover device via UART
- This is dangerous for remote deployments

### Our Solution: Dual OTA Partitions

ESP32 supports **two OTA partitions**:

```
Factory partition:    Known good firmware (fallback)
OTA Partition 0:      Active (or standby)
OTA Partition 1:      Standby (or active)
```

When updating:
1. Device writes to **inactive** partition (doesn't affect current)
2. Reboots into new partition
3. New firmware runs and marks itself as valid
4. If crash detected, automatically reverts to previous partition

### Partition Configuration (`partitions_ota.csv`)

```csv
# Name             Type    Subtype     Offset  Size
nvs,               data,   nvs,        0x9000,  0x5000
phy_init,          data,   phy,        0xe000,  0x1000
factory,           app,    factory,    0x10000, 0x180000
ota_0,             app,    ota_0,      0x190000, 0x180000
ota_1,             app,    ota_1,      0x310000, 0x180000
```

**Total: 4MB = 1MB NVS + 1MB Factory + 1MB OTA0 + 1MB OTA1**

### Boot Validation Process (in main.cpp)

```cpp
void validateFirmware() {
    // Get current partition info
    esp_partition_t* partition = esp_ota_get_running_partition();
    
    // Check OTA state
    esp_ota_img_state_t ota_state;
    esp_ota_get_state_partition(partition, &ota_state);
    
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // New firmware - only reached if boot succeeded
        esp_ota_mark_app_valid_cancel_rollback();
        // Cancels automatic rollback
    }
}
```

### Rollback Scenarios

| Scenario | Behavior |
|----------|----------|
| Normal boot (no crash) | Firmware validates, continues operating |
| New firmware hangs on boot | ESP32 watchdog reboots, reverts to previous |
| New firmware crashes in setup() | Automatic rollback to factory firmware |
| Power loss during boot | Safely reverts to known-good firmware |
| Corrupted OTA download | SHA256 verification fails, no installation |

### Testing Rollback (Optional)

To test rollback functionality:

```cpp
// In setup(), after 5 seconds, intentionally crash:
if (millis() > 5000) {
    // This will trigger rollback
    esp_restart();  // Boot loop triggers watchdog
}
```

Then:
1. Verify new firmware reboots
2. Check watchdog timer triggers rollback
3. Device automatically recovers with previous firmware

## 📦 CI/CD Deployment

### GitHub Actions Workflow

The `.github/workflows/build-release.yml` automatically:

1. **Triggers on:** Push to `main` branch with changes to source code
2. **Builds:** Compiles firmware using PlatformIO
3. **Generates:** SHA256 checksum of binary
4. **Creates:** GitHub Release with tag `v<version>`
5. **Uploads:** `firmware.bin` as release asset
6. **Generates:** `version.json` for OTA system

### Setup Instructions

1. **Ensure GitHub Actions is enabled:**
   - Go to repository → Settings → Actions → General
   - Select "Allow all actions and reusable workflows"

2. **Create `.github/workflows/build-release.yml`** (already included in this project)

3. **Update `version.txt` with version:**
   ```
   1.0.1
   ```

4. **Commit and push:**
   ```bash
   echo "1.0.1" > version.txt
   git add version.txt
   git commit -m "Bump version to 1.0.1"
   git push origin main
   ```

5. **GitHub Actions automatically:**
   - Builds firmware
   - Creates Release `v1.0.1`
   - Uploads `firmware.bin`
   - Generates `version.json`

6. **Check release:**
   - Go to GitHub repo → Releases
   - Verify `v1.0.1` release exists
   - Download and inspect assets

### Workflow Diagram

```
┌─ Push to main branch ─┐
      (src/ or platformio.ini changed)
              ↓
    ┌────────────────────┐
    │ GitHub Actions     │
    │ Triggered          │
    └────────────────────┘
              ↓
    ┌────────────────────┐
    │ 1. Setup Python    │
    │ 2. Install PIO     │
    │ 3. Build firmware  │
    │ 4. Gen SHA256      │
    │ 5. Create Release  │
    │ 6. Upload assets   │
    │ 7. Gen version.json│
    └────────────────────┘
              ↓
    ┌────────────────────┐
    │ Release Published  │
    │ v1.0.1             │
    │ firmware.bin       │
    │ version.json       │
    └────────────────────┘
              ↓
    ┌────────────────────┐
    │ ESP32 fetches      │
    │ version.json       │
    │ Sees new version   │
    │ Auto-downloads     │
    │ firmware.bin       │
    │ Updates OTA        │
    │ Reboots            │
    └────────────────────┘
```

## ⚙️ Hardware Control

### GPIO 23 Output Example

The firmware controls GPIO 23 with this pattern:

- **LED ON** for 2 seconds
- **LED OFF** for 2 seconds
- Repeats continuously
- LED state shown in status indicator

### Code Locations

**Configuration:** [include/config.h](include/config.h#L37)
```cpp
#define GPIO_OUTPUT_PIN 23
#define LED_ON_DURATION_MS 2000
#define LED_OFF_DURATION_MS 2000
```

**Initialization:** [src/main.cpp](src/main.cpp#L70)
```cpp
void initializeGPIO() {
    pinMode(GPIO_OUTPUT_PIN, OUTPUT);
    digitalWrite(GPIO_OUTPUT_PIN, LOW);
}
```

**Main Loop:** [src/main.cpp](src/main.cpp#L490)
```cpp
void handleLED() {
    if (elapsed >= duration) {
        ledState = !ledState;
        digitalWrite(GPIO_OUTPUT_PIN, ledState ? HIGH : LOW);
    }
}
```

### Connecting an LED

```
ESP32 Board                External Circuit
================         ==================

GPIO 23 ─────────────┬──[470Ω resistor]──┬──┐
                     │                    │ ├─→ LED+ (Red)
                   [GND]   (Ground)    ──┘ │
                                          ├─→ LED- (Black)
                                          │
                                        [R 1kΩ]
                                          │
                                         ===
             (Optional: Use triode for higher current loads)
```

### Using with Relay or Mosfet

For high-power devices (motors, lights):

```
GPIO 23 ─────→ [Opto-isolator or Logic Level Convertor]
                              ↓
                         Relay Control Pin
                         or N-Channel MOSFET Gate
                              ↓
                    [High-Power Device / Load]
```

### Modifying Control Logic

To change the blink pattern, edit [src/main.cpp](src/main.cpp#L455):

```cpp
void handleLED() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastLedToggleTime;
    
    // Customize these durations (milliseconds):
    int onDuration = 1000;   // 1 second ON
    int offDuration = 3000;  // 3 seconds OFF
    
    int duration = ledState ? onDuration : offDuration;
    
    if (elapsed >= duration) {
        ledState = !ledState;
        digitalWrite(GPIO_OUTPUT_PIN, ledState ? HIGH : LOW);
        lastLedToggleTime = now;
    }
}
```

## 🔒 Production Best Practices

### 1. Security Recommendations

#### HTTPS Certificate Verification
```cpp
// In config.h - keep enabled in production
#define REQUIRE_HTTPS_CERT true

// Provides GitHub CA certificate
#define GITHUB_CERT_CA "-----BEGIN CERTIFICATE-----\n..."
```

#### Credential Protection
- WiFi passwords stored in NVS (encrypted by ESP32)
- HTTPS required for all OTA downloads
- Verify SHA256 checksums before installation

#### Secure OTA Process
```
1. ✓ HTTPS download with cert verification
2. ✓ SHA256 hash verification before flash write
3. ✓ Dual partition scheme (never overwrite running firmware)
4. ✓ Automatic rollback on crash
5. ✓ Firmware validation before commit
```

### 2. Error Handling

All critical operations have error checking:

```cpp
// Example: OTA error handling
if (!Update.begin(contentLength)) {
    logMessage("ERROR", "OTA begin failed");
    return false;  // Graceful failure
}

if (Update.write(buff, c) != c) {
    logMessage("ERROR", "OTA write failed");
    Update.abort();  // Stop immediately
    return false;
}
```

### 3. WiFi Reconnection Strategy

Implements exponential backoff:
- 1st attempt: immediate
- 2nd attempt: after 2 seconds
- 3rd attempt: after 4 seconds
- 4th attempt: after 8 seconds
- 5th+ attempts: every 30 seconds

Prevents network congestion and unnecessary power drain.

### 4. Watchdog Timer

Prevents firmware hangs:

```cpp
// Enable in config.h
#define ENABLE_WATCHDOG true
#define WATCHDOG_TIMEOUT_SECONDS 30

// Automatically reboots if firmware hangs > 30 seconds
// Triggers automatic rollback if new firmware problematic
```

### 5. Logging with Timestamps

All events logged with millisecond timestamps:

```
[INFO] 1000 ms: WiFi connected
[WARN] 5000 ms: Connection lost
[ERROR] 8000 ms: OTA failed
```

Helps with debugging and production monitoring.

### 6. Version Management

```cpp
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_NUMBER 1

// Logged at startup:
// Firmware Version: 1.0.0
// Build Number: 1

// Compare with remote version.json
// Only update if remote version different
```

### 7. Memory Management

- NVS partitions sized for WiFi credentials (5 sectors = 20KB)
- OTA partitions use most of available flash (1MB each)
- Heap monitoring via Serial output

### 8. Power Efficiency

- WiFi off when not connected
- OTA checks on schedule (not too frequent)
- GPIO operations non-blocking
- Serial output only if SERIAL_DEBUG enabled

### 9. Pre-Deployment Checklist

Before pushing to production:

- [ ] Update `version.txt` with new version
- [ ] Test WiFi provisioning locally
- [ ] Test OTA locally (if possible)
- [ ] Verify rollback mechanism works
- [ ] Check serial logs for errors
- [ ] Build release via GitHub Actions
- [ ] Verify firmware.bin in Release assets
- [ ] Test on at least 2 different ESP32 boards
- [ ] Document any custom modifications
- [ ] Set REQUIRE_HTTPS_CERT = true in production

## 🔧 Troubleshooting

### Serial Monitor Not Working

```bash
# List available COM ports
platformio device list

# Connect to specific port
platformio device monitor -p COM3 -b 115200

# Or use screen on Linux/Mac
screen /dev/ttyUSB0 115200
```

### Firmware Won't Upload

1. Check USB cable and driver
2. Verify ESP32 board selected in platformio.ini
3. Hold BOOT button while uploading
4. Erase flash first: `platformio run --target erase`

### WiFi Keeps Disconnecting

- Check router 2.4GHz band availability
- Verify password entered correctly
- Check signal strength (should be > -80 dBm)
- Restart your router
- Check firmware logs for WiFi errors

### OTA Update Fails

Check these in order:
1. WiFi connection is active
2. GitHub Release exists with firmware.bin
3. version.json is accessible
4. SHA256 in version.json matches actual binary
5. Flash has enough space (need 2MB+ free)
6. Device has enough time to download (slow networks)

### Stuck in Boot Loop

If device reboots repeatedly:
1. Erase flash: `platformio run --target erase`
2. Upload factory firmware again
3. Check for crash in setup()
4. Verify GPIO configuration no conflicts

### Device Won't Exit Provisioning Mode

- Credentials may not be saved properly
- Try manual reset: `wifiManager.resetSettings()`
- Erase NVS partition: `platformio run --target erase`
- Check NVS has enough space

## 📚 API Reference

### Main Functions

#### `void setup()`
- Initialize hardware (GPIO, Serial)
- Validate firmware (enables rollback)
- Connect WiFi (or start provisioning)
- Start OTA processes

#### `void loop()`
- Handle LED status indicator
- Check for OTA updates
- Manage WiFi connection
- Update wireless portal

#### `void validateFirmware()`
- Check if firmware is in pending state
- Mark valid if boot successful
- Enable rollback if needed
- Called at startup

#### `bool connectWiFi()`
- Load saved WiFi credentials
- Auto-connect to network
- Start provisioning portal if needed
- Return connection status

#### `void checkForOTAUpdates()`
- Fetch version.json from GitHub
- Compare versions
- Trigger update if new version available
- Called every OTA_CHECK_INTERVAL_SECONDS

#### `bool downloadFirmwareAndUpdate(String url)`
- Download firmware from URL
- Verify SHA256 checksum
- Write to OTA partition
- Reboot with new firmware

### Configuration Constants (config.h)

| Constant | Default | Purpose |
|----------|---------|---------|
| `FIRMWARE_VERSION` | "1.0.0" | Current firmware version |
| `AP_SSID` | "ESP32_CONFIG" | Provisioning hotspot name |
| `AP_PASSWORD` | "esp32config2024" | Provisioning AP password |
| `AP_HIDDEN` | true | Hide provisioning SSID |
| `WIFI_TIMEOUT` | 180s | Setup portal timeout |
| `OTA_CHECK_INTERVAL_SECONDS` | 3600s | Check for updates every hour |
| `GPIO_OUTPUT_PIN` | 23 | Output control GPIO |
| `LED_ON_DURATION_MS` | 2000ms | LED blink on duration |
| `LED_OFF_DURATION_MS` | 2000ms | LED blink off duration |
| `BOOT_TEST_DURATION_SECONDS` | 30s | Time to validate new firmware |

### GitHub Actions Workflow Outputs

When workflow runs, it produces:

1. **Create GitHub Release:**
   - Tag: `v<version>` (from version.txt)
   - Name: `Firmware v<version>`

2. **Upload Assets:**
   - `firmware.bin` - Built firmware binary
   - `firmware.bin.sha256` - SHA256 checksum
   - `version.json` - Metadata for OTA

3. **Generate Information:**
   - Build timestamp
   - Binary file size
   - SHA256 hash
   - Release URL

## 📝 License

This project is provided as-is for educational and commercial use.

## 🤝 Support

For issues or questions:
1. Check Troubleshooting section
2. Review serial output logs
3. Check GitHub Issues
4. Consult PlatformIO documentation

## 🎯 Next Steps

1. **Customize Hardware:**
   - Add more GPIO outputs
   - Add sensors (temperature, humidity)
   - Add buttons and inputs
   - Add LED matrix displays

2. **Extend Features:**
   - Add MQTT support
   - Add REST API endpoints
   - Add web dashboard
   - Add over-the-air settings updates

3. **Production Deployment:**
   - Set up GitHub release pipeline
   - Configure version.json hosting
   - Test rollback mechanism
   - Monitor firmware crashes

4. **Scale to Multiple Devices:**
   - Use canary deployments (update 1% first)
   - Monitor rollback events
   - Set forced update timers
   - Add fleet management

---

**Version:** 1.0.0  
**Last Updated:** 2024-01-15  
**Maintainer:** ESP32 Project Team
