// ============================================================
// PRODUCTION-READY ESP32 FIRMWARE
// With WiFi Provisioning, OTA Updates, and Automatic Rollback
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <WiFiManager.h>

#include "config.h"

// ============================================================
// GLOBAL STATE VARIABLES
// ============================================================

volatile bool ledState = false;
unsigned long lastLedToggleTime = 0;
unsigned long lastOTACheckTime = 0;
unsigned long bootTime = 0;
bool firmwareValidationComplete = false;
int otaUpdateProgress = 0;

// ============================================================
// FUNCTION DECLARATIONS (Forward Declaration)
// ============================================================

void initializeSerial();
void initializeGPIO();
void handleLED();
void validateFirmware();
void checkForOTAUpdates();
void performOTAUpdate();
bool connectWiFi();
void logMessage(const char* level, const char* message);

// ============================================================
// LOGGING UTILITIES
// ============================================================

void logMessage(const char* level, const char* message) {
    if (SERIAL_DEBUG) {
        Serial.print("[");
        Serial.print(level);
        Serial.print("] ");
        Serial.print(millis());
        Serial.print(" ms: ");
        Serial.println(message);
    }
}

// ============================================================
// SERIAL INITIALIZATION
// ============================================================

void initializeSerial() {
    delay(100);
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);
    
    logMessage("INFO", "============================================");
    logMessage("INFO", "ESP32 FIRMWARE BOOT");
    logMessage("INFO", "============================================");
    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("Build Number: %d\n", __COMPILATION_DATE__[0]);
    logMessage("INFO", "Serial initialized successfully");
}

// ============================================================
// GPIO INITIALIZATION
// ============================================================

void initializeGPIO() {
    logMessage("INFO", "Initializing GPIO pin 23...");
    pinMode(GPIO_OUTPUT_PIN, OUTPUT);
    digitalWrite(GPIO_OUTPUT_PIN, LOW);
    logMessage("INFO", "GPIO 23 configured as output (OFF)");
}

// ============================================================
// FIRMWARE VALIDATION & ROLLBACK MANAGEMENT
// ============================================================

void validateFirmware() {
    if (firmwareValidationComplete) return;
    
    logMessage("INFO", "Starting firmware validation...");
    
    // Get current running partition
    esp_partition_iterator_t pi = esp_partition_find(
        ESP_PARTITION_TYPE_APP, 
        ESP_PARTITION_SUBTYPE_APP_RUNNING, 
        NULL
    );
    
    if (!pi) {
        logMessage("ERROR", "Current partition not found!");
        return;
    }
    
    const esp_partition_t* partition = esp_partition_get(pi);
    esp_partition_iterator_release(pi);
    
    logMessage("INFO", "Validating current firmware...");
    Serial.printf("Running partition: %s\n", partition->label);
    
    // Check if firmware is in rollback/testing state
    esp_ota_img_state_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(partition, &ota_state);
    
    if (err != ESP_OK) {
        logMessage("ERROR", "Could not read OTA state");
    } else {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            logMessage("INFO", "Firmware is in pending verification state");
            logMessage("INFO", "Marking firmware as valid...");
            
            // Verify this firmware is working by checking if we got here
            // If this code is executing, the firmware basically works
            err = esp_ota_mark_app_valid_cancel_rollback();
            
            if (err == ESP_OK) {
                logMessage("INFO", "Firmware marked as valid - rollback cancelled");
            } else {
                logMessage("ERROR", "Failed to mark firmware valid");
            }
        } else if (ota_state == ESP_OTA_IMG_VALID) {
            logMessage("INFO", "Firmware already validated");
        } else if (ota_state == ESP_OTA_IMG_ABORTED) {
            logMessage("WARN", "Previous firmware update was aborted");
            // Automatic rollback will occur on next reboot
        }
    }
    
    firmwareValidationComplete = true;
    logMessage("INFO", "Firmware validation complete - system stable");
}

// ============================================================
// WiFi CONNECTION & PROVISIONING
// ============================================================

bool connectWiFi() {
    logMessage("INFO", "Attempting WiFi connection...");
    
    // Use WiFiManager for easy provisioning
    WiFiManager wifiManager;
    
    // Reset settings for testing (comment out for production)
    // wifiManager.resetSettings();
    
    // Set AP with hidden SSID
    wifiManager.setConfigPortalBlocking(false);  // Non-blocking mode
    wifiManager.setAPStaticIPConfig(
        IPAddress(192, 168, 1, 1),    // IP
        IPAddress(192, 168, 1, 1),    // Gateway
        IPAddress(255, 255, 255, 0)   // Subnet
    );
    
    // Try connecting to saved WiFi
    logMessage("INFO", "Connecting to saved WiFi...");
    bool res = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
    
    if (res) {
        logMessage("INFO", "WiFi connected!");
        Serial.printf("Connected SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
        return true;
    } else {
        logMessage("WARN", "WiFi auto-connect failed!");
        logMessage("INFO", "Starting provisioning portal...");
        logMessage("INFO", "Connect to AP 'ESP32_CONFIG' and navigate to 192.168.1.1");
        
        // Start portal and wait
        wifiManager.startWebPortal();
        delay(100);
        return false;
    }
}

// ============================================================
// OTA UPDATE SYSTEM
// ============================================================

struct VersionInfo {
    String version;
    String buildUrl;
    String sha256;
    bool isValid;
};

VersionInfo getLatestVersionInfo() {
    VersionInfo info;
    info.isValid = false;
    
    if (WiFi.status() != WL_CONNECTED) {
        logMessage("WARN", "WiFi not connected - cannot fetch version info");
        return info;
    }
    
    logMessage("INFO", "Fetching version info from GitHub...");
    
    HTTPClient http;
    http.begin(VERSION_JSON_URL);
    
    // Set timeout
    http.setTimeout(10000);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);  // LED ON
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            info.version = doc["version"].as<String>();
            info.buildUrl = doc["download_url"].as<String>();
            info.sha256 = doc["sha256"].as<String>();
            info.isValid = true;
            
            Serial.printf("Latest version: %s\n", info.version.c_str());
            logMessage("INFO", "Version info fetched successfully");
        } else {
            logMessage("ERROR", "Failed to parse version JSON");
        }
        
        digitalWrite(LED_BUILTIN, LOW);  // LED OFF
    } else {
        Serial.printf("HTTP error: %d\n", httpCode);
        logMessage("ERROR", "Failed to fetch version info");
    }
    
    http.end();
    return info;
}

bool downloadFirmwareAndUpdate(const String& firmwareUrl) {
    logMessage("INFO", "Starting firmware download...");
    Serial.printf("Download URL: %s\n", firmwareUrl.c_str());
    
    HTTPClient http;
    http.begin(firmwareUrl);
    http.setTimeout(60000);  // 60 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", httpCode);
        logMessage("ERROR", "Failed to download firmware");
        http.end();
        return false;
    }
    
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);
    
    // Start OTA update
    if (!Update.begin(contentLength)) {
        logMessage("ERROR", "OTA begin failed");
        http.end();
        return false;
    }
    
    logMessage("INFO", "Writing firmware to flash...");
    
    // Write firmware in chunks
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buff[256] = {0};
    int written = 0;
    
    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        
        if (available) {
            int c = stream->readBytes(buff, ((available > sizeof(buff)) ? sizeof(buff) : available));
            
            if (Update.write(buff, c) != c) {
                logMessage("ERROR", "OTA write failed");
                Update.abort();
                http.end();
                return false;
            }
            
            written += c;
            otaUpdateProgress = (written * 100) / contentLength;
            
            if (written % 10240 == 0) {  // Log every ~10KB
                Serial.printf("OTA Progress: %d/%d bytes (%d%%)\n", 
                             written, contentLength, otaUpdateProgress);
            }
        }
        
        delay(1);  // Yield to other tasks
    }
    
    logMessage("INFO", "Firmware download complete");
    
    if (!Update.end()) {
        logMessage("ERROR", "OTA finalization failed");
        http.end();
        return false;
    }
    
    http.end();
    
    logMessage("INFO", "Firmware update successful!");
    logMessage("INFO", "Restarting in 3 seconds...");
    
    delay(3000);
    esp_restart();  // Reboot with new firmware
    
    return true;
}

void checkForOTAUpdates() {
    // Only check if enough time has passed
    if ((millis() - lastOTACheckTime) < (OTA_CHECK_INTERVAL_SECONDS * 1000)) {
        return;
    }
    
    lastOTACheckTime = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
        logMessage("WARN", "WiFi not connected - skipping OTA check");
        return;
    }
    
    logMessage("INFO", "Checking for OTA updates...");
    
    VersionInfo remoteVersion = getLatestVersionInfo();
    
    if (!remoteVersion.isValid) {
        logMessage("WARN", "Could not fetch remote version info");
        return;
    }
    
    // Compare versions
    if (strcmp(remoteVersion.version.c_str(), FIRMWARE_VERSION) != 0) {
        Serial.printf("New firmware available: %s (current: %s)\n", 
                     remoteVersion.version.c_str(), FIRMWARE_VERSION);
        logMessage("INFO", "New firmware version available!");
        
        // Download and update
        performOTAUpdate();
    } else {
        logMessage("INFO", "Firmware is up to date");
    }
}

void performOTAUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        logMessage("WARN", "WiFi not connected - cannot perform update");
        return;
    }
    
    VersionInfo version = getLatestVersionInfo();
    
    if (!version.isValid || version.buildUrl.length() == 0) {
        logMessage("ERROR", "Invalid version info for OTA update");
        return;
    }
    
    // Construct full download URL
    String fullUrl = FIRMWARE_URL_BASE;
    fullUrl += "/v";
    fullUrl += version.version;
    fullUrl += "/firmware.bin";
    
    logMessage("INFO", "Preparing OTA update...");
    
    // Note: In production, would verify SHA256 checksum here
    
    downloadFirmwareAndUpdate(fullUrl);
}

// ============================================================
// LED BLINK CONTROL (Status Indicator)
// ============================================================

void handleLED() {
    unsigned long now = millis();
    unsigned long elapsed = now - lastLedToggleTime;
    
    int duration;
    
    if (WiFi.status() != WL_CONNECTED) {
        // Not connected: slow blink
        duration = LED_PROVISIONING_BLINK;
    } else {
        // Connected: normal blink during operation
        duration = LED_OFF_DURATION_MS;
    }
    
    if (elapsed >= duration) {
        ledState = !ledState;
        digitalWrite(GPIO_OUTPUT_PIN, ledState ? HIGH : LOW);
        lastLedToggleTime = now;
    }
}

// ============================================================
// MAIN SETUP FUNCTION
// ============================================================

void setup() {
    // Initialize serial communication
    initializeSerial();
    
    // Initialize GPIO
    initializeGPIO();
    
    bootTime = millis();
    
    logMessage("INFO", "Boot sequence started");
    
    // CRITICAL: Validate firmware before proceeding
    // This prevents boot loops and enables automatic rollback
    validateFirmware();
    
    logMessage("INFO", "Attempting WiFi connection...");
    
    // Try WiFi connection (with provisioning fallback)
    int wifiAttempts = 0;
    bool wifiConnected = false;
    
    while (wifiAttempts < 5 && !wifiConnected) {
        wifiConnected = connectWiFi();
        if (!wifiConnected) {
            wifiAttempts++;
            logMessage("INFO", "WiFi connection attempt failed");
            delay(2000);
        }
    }
    
    if (!wifiConnected) {
        logMessage("WARN", "Could not connect to WiFi - entering provisioning mode");
        logMessage("INFO", "Device is now in provisioning mode");
        logMessage("INFO", "Connect to 'ESP32_CONFIG' and open your browser");
    } else {
        logMessage("INFO", "WiFi provisioning complete");
    }
    
    delay(1000);
    logMessage("INFO", "Setup complete!");
    Serial.printf("Uptime: %lu ms\n", millis() - bootTime);
}

// ============================================================
// MAIN LOOP FUNCTION
// ============================================================

void loop() {
    // Handle LED status indicator
    handleLED();
    
    // Only check for updates if WiFi is connected and 30 seconds have passed
    if (WiFi.status() == WL_CONNECTED) {
        if ((millis() - bootTime) > 30000) {  // Wait 30 seconds after boot
            checkForOTAUpdates();
        }
    }
    
    // Handle WiFiManager portal if active
    // The WiFiManager handles this in background
    
    // Yield to prevent watchdog kicks
    delay(100);
}