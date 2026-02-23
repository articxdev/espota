// ============================================================
// ESP32 FIRMWARE WITH WIFI PROVISIONING & OTA UPDATES
// ============================================================

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"

// Global variables
WiFiManager wifiManager;
unsigned long lastOTACheck = 0;
bool provisioningMode = false;

// Function declarations
void initializeWiFi();
void setupWiFiCallbacks();
void checkForUpdates();
void performOTAUpdate(String firmwareUrl, String expectedSha256);
void blinkLED(int times, int delayMs);
void printStartupInfo();

// ============================================================
// SETUP
// ============================================================
void setup() {
    delay(1000);  // Let hardware stabilize
    Serial.begin(115200);
    delay(500);
    
    // Print startup banner
    Serial.println("\n\n" "================================");
    Serial.println("ESP32 FIRMWARE v" FIRMWARE_VERSION);
    Serial.println("Author: " FIRMWARE_AUTHOR);
    Serial.println("================================\n");
    
    // Initialize GPIO
    pinMode(23, OUTPUT);
    digitalWrite(23, LOW);
    Serial.println("[OK] GPIO 23 (LED) initialized");
    
    // Initialize WiFi with provisioning
    Serial.println("[*] Initializing WiFi provisioning...");
    initializeWiFi();
    
    // Setup OTA check timer
    lastOTACheck = millis();
    
    Serial.println("[OK] Setup complete - entering main loop");
    Serial.println("================================\n");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    // Handle WiFiManager background tasks
    wifiManager.process();
    
    // Monitor WiFi connection
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 5000) {  // Check every 5 seconds
        lastWiFiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED && !provisioningMode) {
            Serial.println("[!] WiFi disconnected! Attempting to reconnect...");
            wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[OK] WiFi reconnected!");
                Serial.print("[OK] IP Address: ");
                Serial.println(WiFi.localIP());
            }
        }
    }
    
    // Check for OTA updates every OTA_CHECK_INTERVAL_SECONDS
    if (millis() - lastOTACheck > (OTA_CHECK_INTERVAL_SECONDS * 1000)) {
        lastOTACheck = millis();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[*] Checking for firmware updates...");
            checkForUpdates();
        } else if (!provisioningMode) {
            Serial.println("[!] WiFi not connected - will reconnect automatically");
        }
    }
    
    // Blink LED to show device is alive
    digitalWrite(23, HIGH);
    delay(5000);
    digitalWrite(23, LOW);
    delay(300);
}

// ============================================================
// WiFi INITIALIZATION & PROVISIONING
// ============================================================
void initializeWiFi() {
    // Set WiFiManager configuration
    wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
    wifiManager.setConnectTimeout(WIFI_TIMEOUT);
    
    // Set to try auto-connect first before starting portal
    wifiManager.setEnableConfigPortal(true);
    
    // Setup WiFi callbacks
    setupWiFiCallbacks();
    
    // Try to connect to previously saved WiFi SSID
    Serial.println("[*] Attempting to reconnect to last known WiFi network...");
    
    // First attempt: try saved credentials with timeout
    int attemptCount = 0;
    int maxAttempts = 3;
    bool connected = false;
    
    while (attemptCount < maxAttempts && !connected) {
        Serial.print("[*] Connection attempt ");
        Serial.print(attemptCount + 1);
        Serial.print(" of ");
        Serial.println(maxAttempts);
        
        // Try to auto-connect to saved WiFi
        connected = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
        
        if (connected) {
            break;
        }
        attemptCount++;
        
        if (attemptCount < maxAttempts) {
            Serial.println("[!] Connection failed, retrying in 5 seconds...");
            delay(5000);
        }
    }
    
    if (connected) {
        Serial.println("[OK] WiFi connected!");
        Serial.print("[OK] SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("[OK] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[OK] Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        provisioningMode = false;
    } else {
        Serial.println("[!] Failed to reconnect to last WiFi network");
        Serial.println("[*] Starting WiFi provisioning portal...");
        Serial.print("[*] Connect to hotspot: ");
        Serial.println(AP_SSID);
        Serial.print("[*] Open browser at: http://192.168.1.1");
        Serial.println("[*] Portal will timeout in " + String(WIFI_PORTAL_TIMEOUT) + " seconds");
        Serial.println();
        provisioningMode = true;
    }
}

void setupWiFiCallbacks() {
    // Called when WiFi settings are saved
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("[OK] WiFi credentials saved!");
        provisioningMode = false;
    });
    
    // Called when provisioning portal starts
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("[*] Provisioning portal started");
        Serial.print("[*] AP SSID: ");
        Serial.println(myWiFiManager->getConfigPortalSSID());
        blinkLED(3, 200);  // 3 quick blinks to indicate provisioning
    });
}

// ============================================================
// OTA UPDATE CHECKING & INSTALLATION
// ============================================================
void checkForUpdates() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[!] WiFi not connected, skipping OTA check");
        return;
    }
    
    HTTPClient http;
    http.setConnectTimeout(10000);  // 10 second timeout
    
    Serial.print("[*] Fetching version info from: ");
    Serial.println(VERSION_JSON_URL);
    
    if (!http.begin(VERSION_JSON_URL)) {
        Serial.println("[!] Failed to begin HTTP request");
        http.end();
        return;
    }
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.print("[!] HTTP Error: ");
        Serial.println(httpCode);
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    Serial.println("[OK] Version info received");
    Serial.println("Response: " + payload);
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("[!] JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Extract version and download URL
    String latestVersion = doc["version"] | "0.0.0";
    String downloadUrl = doc["download_url"] | "";
    String expectedSha256 = doc["sha256"] | "";
    
    Serial.print("[*] Latest version: ");
    Serial.println(latestVersion);
    Serial.print("[*] Current version: ");
    Serial.println(FIRMWARE_VERSION);
    
    // Compare versions (simple string comparison)
    if (latestVersion != FIRMWARE_VERSION && !downloadUrl.isEmpty()) {
        Serial.println("[!] New firmware available!");
        Serial.print("[*] Downloading from: ");
        Serial.println(downloadUrl);
        
        performOTAUpdate(downloadUrl, expectedSha256);
    } else {
        Serial.println("[OK] Firmware is up to date");
    }
}

void performOTAUpdate(String firmwareUrl, String expectedSha256) {
    Serial.println("[*] Starting OTA update process...");
    
    HTTPClient http;
    http.setConnectTimeout(30000);  // 30 second timeout
    http.setTimeout(30000);
    
    // IMPORTANT: Enable following HTTP redirects (GitHub returns 302)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    if (!http.begin(firmwareUrl)) {
        Serial.println("[!] Failed to connect to firmware URL");
        http.end();
        return;
    }
    
    int httpCode = http.GET();
    
    Serial.print("[*] HTTP Response Code: ");
    Serial.println(httpCode);
    
    // Accept 200 (success) or 206 (partial content)
    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print("[!] Failed to download firmware. HTTP Code: ");
        Serial.println(httpCode);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    
    if (contentLength <= 0) {
        Serial.println("[!] Invalid content length");
        http.end();
        return;
    }
    
    Serial.print("[*] Firmware size: ");
    Serial.print(contentLength);
    Serial.println(" bytes");
    
    // Begin OTA update
    if (!Update.begin(contentLength)) {
        Serial.println("[!] Not enough space for OTA update");
        Serial.print("[!] Available: ");
        Serial.print(ESP.getFreeSketchSpace());
        Serial.println(" bytes");
        http.end();
        return;
    }
    
    // Get the stream and write firmware in chunks
    WiFiClient *stream = http.getStreamPtr();
    if (!stream) {
        Serial.println("[!] Failed to get stream");
        http.end();
        return;
    }
    
    size_t written = 0;
    uint8_t buff[512] = { 0 };
    unsigned long lastProgress = millis();
    
    while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        
        if (available) {
            int bytesRead = stream->readBytes(buff, ((available > sizeof(buff)) ? sizeof(buff) : available));
            
            if (bytesRead > 0) {
                Update.write(buff, bytesRead);
                written += bytesRead;
            }
        }
        
        // Progress indicator every 2 seconds
        if (millis() - lastProgress > 2000) {
            lastProgress = millis();
            Serial.print("[*] Downloaded: ");
            Serial.print((written * 100) / contentLength);
            Serial.print("% (");
            Serial.print(written);
            Serial.print("/");
            Serial.print(contentLength);
            Serial.println(")");
        }
        
        delay(10);
    }
    
    http.end();
    
    // Verify download was complete
    if (written != contentLength) {
        Serial.print("[!] Download incomplete: ");
        Serial.print(written);
        Serial.print(" / ");
        Serial.println(contentLength);
        Update.abort();
        return;
    }
    
    // Finish OTA update
    if (Update.end()) {
        if (Update.isFinished()) {
            Serial.println("[OK] OTA Update writing finished successfully!");
            Serial.println("[*] Restarting device in 3 seconds...");
            blinkLED(5, 100);  // 5 quick blinks before restart
            delay(3000);
            ESP.restart();
        } else {
            Serial.print("[!] OTA Update failed. Status: ");
            Serial.println(Update.getError());
            Update.abort();
        }
    } else {
        Serial.print("[!] OTA Update error: ");
        Serial.println(Update.getError());
        Update.abort();
    }
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================
void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(23, HIGH);
        delay(delayMs);
        digitalWrite(23, LOW);
        delay(delayMs);
    }
}

void printStartupInfo() {
    Serial.println("\n[INFO] Device Information:");
    Serial.print("  - Chip Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("  - Flash Size: ");
    Serial.print(ESP.getFlashChipSize() / 1024);
    Serial.println(" KB");
    Serial.print("  - Free Heap: ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB");
    Serial.println();
}
