# Deployment & Setup Guide

Complete step-by-step guide for deploying the ESP32 firmware to production.

## 📋 Pre-Flight Checklist

Before deployment, ensure:

- [ ] ESP32 board is available and working
- [ ] USB cable is functioning
- [ ] PlatformIO is installed (`pip install platformio`)
- [ ] GitHub repository is ready (for CI/CD)
- [ ] WiFi network available for testing
- [ ] 30 minutes for initial setup and testing

## 🔹 Phase 1: Local Development & Testing

### Step 1.1: Clone and Navigate to Project

```bash
cd espota
ls -la
```

You should see:
```
platformio.ini
version.txt
version.json.example
partitions_ota.csv
README.md
DEPLOYMENT.md
include/
src/
.github/
```

### Step 1.2: Connect ESP32 via USB

Connect your ESP32 board to computer via USB-to-Serial cable.

```bash
# Verify connection
platformio device list

# Should show something like:
# /dev/ttyUSB0 - USB2.0-Serial [vid=1a86:7523]
```

### Step 1.3: Build Firmware Locally

```bash
# Clean previous builds
platformio run -e esp32 --target clean

# Build new firmware
platformio run -e esp32

# Should end with:
# ====== [SUCCESS] Took 45.23 seconds ======
```

Check output:
```
.pio/build/esp32/firmware.bin    (size: ~1.2MB)
.pio/build/esp32/firmware.elf    (debug symbols)
```

### Step 1.4: Upload to ESP32

```bash
# Upload via USB (first time)
platformio run -e esp32 --target upload

# Should show:
# Serial port: /dev/ttyUSB0
# Speed: 115200 baud
# Looking for upload port...
# Uploading .pio/build/esp32/firmware.bin
# [=============================] 100%
```

### Step 1.5: View Serial Output

```bash
# Open serial monitor
platformio device monitor -b 115200

# You should see:
# [INFO] ============================================
# [INFO] ESP32 FIRMWARE BOOT
# [INFO] ============================================
# Firmware Version: 1.0.0
# ...
```

Press `Ctrl+C` to exit monitor.

### Step 1.6: Test WiFi Provisioning

1. **See no saved WiFi credentials:**
   - Firmware starts hidden AP: `ESP32_CONFIG`
   
2. **Connect to setup portal:**
   - On phone/laptop: Find WiFi network `ESP32_CONFIG`
   - Enter password: `esp32config2024`
   
3. **Configure WiFi:**
   - Open browser → `192.168.1.1` or look for captive portal
   - Select your home WiFi network
   - Enter WiFi password
   - Device reboots
   
4. **Verify connection:**
   - Check serial output: `[INFO] WiFi connected!`
   - Shows: Connected SSID, IP address, signal strength

Example successful output:
```
[INFO] Attempting WiFi connection...
[INFO] Connecting to saved WiFi...
[INFO] WiFi connected!
Connected SSID: MyHomeNetwork
IP Address: 192.168.1.100
Signal Strength: -67 dBm
[INFO] WiFi provisioning complete
```

### Step 1.7: Test GPIO Control

The firmware controls GPIO 23 with a 2-second blink pattern.

**With an LED:**
- Connect LED+ to GPIO 23 (via 470Ω resistor)
- Connect LED- to GND
- Should blink: ON 2 sec, OFF 2 sec, repeat

**Via Serial Monitor:**
- Watch GPIO state changes in logs
- No LED needed for basic testing

### Step 1.8: Test Local OTA (Optional)

To test OTA locally without GitHub:

1. **Modify config.h** - temporarily disable version fetch:
   ```cpp
   #define OTA_CHECK_INTERVAL_SECONDS 60  // Check every minute
   ```

2. **Host firmware locally:**
   ```bash
   # Create HTTP server in build directory
   cd .pio/build/esp32/
   python -m http.server 8000
   ```

3. **Connect ESP32 to WiFi** pointing to local server
4. **Wait 60 seconds** - check for OTA trigger
5. **Monitor download progress**

---

## 🔹 Phase 2: GitHub CI/CD Setup

### Step 2.1: Push to GitHub

```bash
# Create git repo
git init
git add .
git commit -m "Initial production-ready ESP32 firmware"
git branch -M main
git remote add origin https://github.com/YOUR_USER/espota.git
git push -u origin main
```

### Step 2.2: Enable GitHub Actions

1. Go to GitHub repository
2. Click **Settings** → **Actions** → **General**
3. Select: **"Allow all actions and reusable workflows"**
4. Save

### Step 2.3: Create First Release

Update version and trigger build:

```bash
# Update version
echo "1.0.1" > version.txt

# Commit
git add version.txt
git commit -m "Release v1.0.1 - production ready"

# Push (triggers GitHub Actions)
git push origin main

# Wait for GitHub Actions to complete...
# (check Actions tab in GitHub UI)
```

### Step 2.4: Verify Release Created

In GitHub:
1. Go to repository
2. Click **Releases** tab
3. Should see: **v1.0.1** release
4. Contains:
   - `firmware.bin` (main binary)
   - `firmware.bin.sha256` (checksum)
   - `version.json` (OTA metadata)

Example `version.json`:
```json
{
  "version": "1.0.1",
  "download_url": "firmware.bin",
  "sha256": "abc123...",
  "release_notes": "Release v1.0.1 - Automatic build",
  "timestamp": "2024-01-15T10:30:00Z"
}
```

### Step 2.5: Set Up version.json Hosting

The OTA system needs to fetch `version.json` from GitHub.

**Option A: Use Raw GitHub URL (Recommended)**

```cpp
// In config.h - already configured
#define GITHUB_OWNER "your-username"
#define GITHUB_REPO "espota"
#define VERSION_JSON_URL "https://raw.githubusercontent.com/your-username/espota/main/version.json"
```

Workflow automatically updates `version.json` in main branch.

**Option B: Manual Update**

```bash
# After release is created:
# Download version.json from release assets
# Or generate manually:

cat > version.json << EOF
{
  "version": "1.0.1",
  "download_url": "firmware.bin",
  "sha256": "$(sha256sum .pio/build/esp32/firmware.bin | awk '{print $1}')",
  "release_notes": "Release v1.0.1",
  "timestamp": "$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
}
EOF

# Commit to main branch
git add version.json
git commit -m "Update version.json"
git push origin main
```

---

## 🔹 Phase 3: Deploy to Production Device

### Step 3.1: Factory Reset Device

```bash
# Erase all flash memory (WiFi credentials, NVS, etc.)
platformio run -e esp32 --target erase

# Or via serial command (if connected):
# [From monitor] Type: erase_flash
```

### Step 3.2: Upload Latest Firmware

```bash
platformio run -e esp32 --target upload
```

### Step 3.3: First Boot Setup

Device boots and searches for WiFi credentials:
1. No credentials found → starts `ESP32_CONFIG` hotspot
2. User connects to hotspot
3. Opens portal → enters WiFi credentials
4. Device saves and reboots

### Step 3.4: Verify Firmware Validation

Serial output should show:
```
[INFO] Starting firmware validation...
[INFO] Validating current firmware...
[INFO] Firmware marked as valid - rollback cancelled
```

This confirms **rollback protection is active**.

### Step 3.5: Verify OTA Check Starts

After ~30 seconds in main loop:
```
[INFO] Checking for OTA updates...
[INFO] Fetching version info from GitHub...
Latest version: 1.0.1
[INFO] Version info fetched successfully
```

If device version matches remote: `Firmware is up to date`

### Step 3.6: Test Production OTA Update

To test update on production device:

1. **Upload new firmware locally:**
   - Change `FIRMWARE_VERSION "1.0.2"` in config.h
   - `platformio run -e esp32`
   - `platformio run -e esp32 --target upload`

2. **Or wait for next scheduled check** (every 1 hour)

Device automatically:
- Fetches version.json
- Sees new version
- Downloads firmware.bin via HTTPS
- Verifies SHA256
- Updates OTA partition
- Reboots with new firmware
- Validates new firmware
- Continues operation

---

## 🔹 Phase 4: Production Monitoring

### Recommended Monitoring

**Option 1: Serial Output Logging**
```bash
# Log to file with timestamp
platformio device monitor -b 115200 > esp32_$(date +%s).log
```

**Option 2: Remote Logging**
- Modify firmware to send logs to cloud service
- Monitor via Grafana, Datadog, or similar

**Option 3: GitHub Releases Dashboard**
- Check recent releases for latest firmware
- Monitor rollback events
- Track update adoption

### Health Checks

Monitor these indicators:

```
✓ WiFi connection status
✓ OTA check frequency (every 1 hour)
✓ No repeated reboots (indicates crash)
✓ GPIO outputs toggling as expected
✓ Serial logs without errors
```

---

## ⚠️ Emergency Procedures

### If Device Won't Boot

1. **Try factory reset:**
   ```bash
   platformio run -e esp32 --target erase
   platformio run -e esp32 --target upload
   ```

2. **Check serial output for errors:**
   ```bash
   platformio device monitor -b 115200
   ```

3. **Manual rollback:**
   - Device uses factory partition as fallback
   - If OTA partitions corrupted, factory firmware boots
   - This is automatic - no user action needed

### If WiFi Won't Connect

1. **Re-provision:**
   - Power cycle device
   - Connect to `ESP32_CONFIG` hotspot
   - Re-enter WiFi credentials

2. **Or erase NVS and try again:**
   ```bash
   platformio run -e esp32 --target erase
   ```

### If OTA Update Fails

1. **Check WiFi connection** - must be stable
2. **Verify version.json is accessible:**
   ```bash
   curl https://raw.githubusercontent.com/YOUR_USER/espota/main/version.json
   ```

3. **Check GitHub Release exists** with firmware.bin
4. **Verify SHA256 matches:**
   ```bash
   sha256sum firmware.bin
   cat firmware.bin.sha256
   ```

5. **Device automatically retries** in 1 hour

---

## 📊 Version Management Strategy

### Versioning Scheme

Use [Semantic Versioning](https://semver.org/): `MAJOR.MINOR.PATCH`

```
1.0.0  → Initial release
1.0.1  → Bug fix
1.1.0  → New feature (minor)
2.0.0  → Breaking change (major)
```

### Update Frequency

Recommended update schedule:
- **Security patches**: Immediate (PATCH)
- **Bug fixes**: Every 1-2 weeks (PATCH/MINOR)
- **New features**: Every 1 month (MINOR)
- **Major releases**: Quarterly (MAJOR)

### Rollback Policy

Automatic rollback triggers:
- Watchdog timeout (30 seconds)
- Exception/crash in setup()
- Stack overflow
- Memory allocation failure

If rollback occurs:
1. Device restarts with previous firmware
2. Check serial logs for crash cause
3. Fix and release new version
4. Re-deploy to affected devices

---

## 🚀 Multiple Device Deployment

### Canary Deployment (5-10% first)
1. Deploy to 5-10% of fleet
2. Monitor for 24 hours
3. Check rollback events
4. If stable → deploy to 50%
5. If stable → deploy to 100%

### Forced Update
```cpp
// In version.json
"force_update": true,
"min_version": "0.9.0"

// Devices with version < 0.9.0 must update
```

### Staged Rollout
```
Day 1: Deploy to 10% (canary)
Day 2: Deploy to 30% (if stable)  
Day 3: Deploy to 70% (if stable)
Day 4: Deploy to 100% (if stable)
```

---

## 📝 Deployment Checklist

Before going live:

- [ ] Local build successful
- [ ] Serial monitor shows no errors
- [ ] WiFi provisioning works
- [ ] GPIO outputs responding
- [ ] GitHub Actions workflow enabled
- [ ] First release created on GitHub
- [ ] version.json is accessible
- [ ] Tested OTA update (local or staged)
- [ ] Rollback tested (optional but recommended)
- [ ] Documentation updated
- [ ] Team trained on monitoring
- [ ] Monitoring/alerting configured
- [ ] Emergency procedures documented
- [ ] Backup firmware saved
- [ ] Success criteria defined

---

## 📞 Support & Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| Can't upload via USB | Check USB driver, try `platformio run --target erase` first |
| WiFi won't connect | Wrong password? Check 2.4GHz band. Try re-provisioning. |
| OTA fails silently | Check WiFi, verify version.json URL, check GitHub Release exists |
| Device in boot loop | Erase flash: `platformio run --target erase` |
| Rollback not working | Update to latest ESP-IDF version, check partition config |

### Debugging

Enable detailed logging:

```cpp
// In config.h
#define CURRENT_LOG_LEVEL LOG_DEBUG  // Most verbose
#define SERIAL_DEBUG true             // Enable all serial output
```

Then analyze logs for errors/warnings.

---

## ✅ Success Indicators

After deployment, verify:

✓ **WiFi:** Device connects automatically  
✓ **OTA:** Updates download when new version available  
✓ **Rollback:** Fails gracefully with automatic recovery  
✓ **GPIO:** Outputs respond as programmed  
✓ **Serial:** Logs show normal operation, no errors  
✓ **Updates:** Version increments in GitHub Releases  
✓ **Stability:** Device runs continuously for weeks  

---

**You're ready for production! 🚀**

For detailed information, see [README.md](README.md)
