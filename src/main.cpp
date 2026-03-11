// ============================================================
// ESP32 TEMPERATURE CONTROL SYSTEM
// WiFi Provisioning + OTA + Sensors + Firebase Realtime DB
// ============================================================

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_SHT31.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"   // Firebase token callback
#include "addons/RTDBHelper.h"    // Firebase RTDB utility
#include "config.h"

// ============================================================
// HARDWARE OBJECTS
// ============================================================
OneWire         oneWire1(PIN_TEMP_SENSOR1);
OneWire         oneWire2(PIN_TEMP_SENSOR2);
DallasTemperature tempSensor1(&oneWire1);
DallasTemperature tempSensor2(&oneWire2);
Adafruit_SHT31  sht31;

// ============================================================
// FIREBASE OBJECTS
// ============================================================
FirebaseData    fbdo;
FirebaseAuth    fbAuth;
FirebaseConfig  fbConfig;
bool            firebaseReady = false;

// ============================================================
// WIFI
// ============================================================
WiFiManager wifiManager;
bool        provisioningMode = false;

// ============================================================
// TIMERS
// ============================================================
unsigned long lastOTACheck      = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastSensorRead    = 0;

// ============================================================
// SENSOR READINGS
// ============================================================
float temp1           = -127.0f;   // DS18B20 sensor 1
float temp2           = -127.0f;   // DS18B20 sensor 2
float ambientTemp     = -127.0f;   // SHT31 temperature
float ambientHumidity = 0.0f;      // SHT31 humidity

// ============================================================
// RELAY / LED STATES
// ============================================================
bool heaterOn = false;
bool refrigOn = false;
bool fanOn    = false;

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================
void initializePins();
void initializeSensors();
void initializeFirebase();
void initializeWiFi();
void setupWiFiCallbacks();
void readSensors();
void applyRelayStates();
void pushToFirebase();
void readRelayCommandsFromFirebase();
void checkForUpdates();
void performOTAUpdate(String firmwareUrl, String expectedSha256);

// ============================================================
// SETUP
// ============================================================
void setup() {
    delay(1000);
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println("\n\n================================");
    Serial.println("ESP32 TEMP CONTROL v" FIRMWARE_VERSION);
    Serial.println("Author: " FIRMWARE_AUTHOR);
    Serial.println("================================\n");

    initializePins();
    initializeSensors();

    Serial.println("[*] Initializing WiFi provisioning...");
    initializeWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        initializeFirebase();
    }

    lastOTACheck      = millis();
    lastFirebaseUpdate = millis();
    lastSensorRead    = millis();

    Serial.println("[OK] Setup complete - entering main loop");
    Serial.println("================================\n");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    wifiManager.process();

    // ----- WiFi watchdog: reconnect every 5 s if lost ------
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 5000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED && !provisioningMode) {
            Serial.println("[!] WiFi lost - reconnecting...");
            wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[OK] WiFi reconnected: " + WiFi.localIP().toString());
                if (!firebaseReady) initializeFirebase();
            }
        }
    }

    // ----- Read sensors every 2 s --------------------------
    if (millis() - lastSensorRead > 2000) {
        lastSensorRead = millis();
        readSensors();
        applyRelayStates();
    }

    // ----- Firebase: push sensor data & read commands ------
    if (WiFi.status() == WL_CONNECTED && firebaseReady) {
        if (millis() - lastFirebaseUpdate > FIREBASE_UPDATE_INTERVAL_MS) {
            lastFirebaseUpdate = millis();
            readRelayCommandsFromFirebase();
            pushToFirebase();
        }
    }

    // ----- OTA check ----------------------------------------
    if (millis() - lastOTACheck > (OTA_CHECK_INTERVAL_SECONDS * 1000UL)) {
        lastOTACheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[*] Checking for firmware updates...");
            checkForUpdates();
        }
    }
}

// ============================================================
// PIN INITIALIZATION
// ============================================================
void initializePins() {
    // Relays
    pinMode(PIN_RELAY_HEATER, OUTPUT); digitalWrite(PIN_RELAY_HEATER, LOW);
    pinMode(PIN_RELAY_REFRIG, OUTPUT); digitalWrite(PIN_RELAY_REFRIG, LOW);
    pinMode(PIN_RELAY_FAN,    OUTPUT); digitalWrite(PIN_RELAY_FAN,    LOW);

    // Status LEDs
    pinMode(PIN_LED_HEATER, OUTPUT); digitalWrite(PIN_LED_HEATER, LOW);
    pinMode(PIN_LED_REFRIG, OUTPUT); digitalWrite(PIN_LED_REFRIG, LOW);
    pinMode(PIN_LED_FAN,    OUTPUT); digitalWrite(PIN_LED_FAN,    LOW);

    Serial.println("[OK] GPIO initialized:");
    Serial.println("     Relay  Heater  -> PIN " + String(PIN_RELAY_HEATER));
    Serial.println("     Relay  Refrig  -> PIN " + String(PIN_RELAY_REFRIG));
    Serial.println("     Relay  Fan     -> PIN " + String(PIN_RELAY_FAN));
    Serial.println("     LED    Heater  -> PIN " + String(PIN_LED_HEATER));
    Serial.println("     LED    Refrig  -> PIN " + String(PIN_LED_REFRIG));
    Serial.println("     LED    Fan     -> PIN " + String(PIN_LED_FAN));
    Serial.println("     Temp1  DS18B20 -> PIN " + String(PIN_TEMP_SENSOR1));
    Serial.println("     Temp2  DS18B20 -> PIN " + String(PIN_TEMP_SENSOR2));
    Serial.println("     I2C    SDA     -> PIN " + String(PIN_I2C_SDA));
    Serial.println("     I2C    SCL     -> PIN " + String(PIN_I2C_SCL));
}

// ============================================================
// SENSOR INITIALIZATION
// ============================================================
void initializeSensors() {
    // DS18B20 sensors
    tempSensor1.begin();
    tempSensor2.begin();
    Serial.print("[OK] DS18B20 Sensor1 devices found: ");
    Serial.println(tempSensor1.getDeviceCount());
    Serial.print("[OK] DS18B20 Sensor2 devices found: ");
    Serial.println(tempSensor2.getDeviceCount());

    // I2C (SHT31) sensor
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (sht31.begin(I2C_SENSOR_ADDR)) {
        Serial.println("[OK] SHT31 I2C sensor ready at address 0x" +
                       String(I2C_SENSOR_ADDR, HEX));
    } else {
        Serial.println("[!] SHT31 not found - check wiring / sensor address");
    }
}

// ============================================================
// FIREBASE INITIALIZATION
// ============================================================
void initializeFirebase() {
    Serial.println("[*] Connecting to Firebase...");

    fbConfig.database_url  = FIREBASE_URL;   // regional DB needs full URL
    fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    fbConfig.token_status_callback = tokenStatusCallback;  // token refresh helper

    Firebase.begin(&fbConfig, &fbAuth);
    Firebase.reconnectWiFi(true);

    if (Firebase.ready()) {
        firebaseReady = true;
        Serial.println("[OK] Firebase connected!");
        // Write initial online status
        Firebase.RTDB.setString(&fbdo,
            String(FIREBASE_BASE_PATH) + "/status/state", "online");
        Firebase.RTDB.setString(&fbdo,
            String(FIREBASE_BASE_PATH) + "/status/firmware", FIRMWARE_VERSION);
    } else {
        Serial.println("[!] Firebase connection failed - will retry");
    }
}

// ============================================================
// READ SENSORS
// ============================================================
void readSensors() {
    // DS18B20 Sensor 1 (pin 27)
    tempSensor1.requestTemperatures();
    temp1 = tempSensor1.getTempCByIndex(0);

    // DS18B20 Sensor 2 (pin 12)
    tempSensor2.requestTemperatures();
    temp2 = tempSensor2.getTempCByIndex(0);

    // SHT31 I2C Sensor (pins 21, 22)
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t)) ambientTemp     = t;
    if (!isnan(h)) ambientHumidity = h;

    Serial.println("[SENSOR] Temp1: " + String(temp1,1) + "°C  |  " +
                   "Temp2: " + String(temp2,1) + "°C  |  " +
                   "Ambient: " + String(ambientTemp,1) + "°C  " +
                   String(ambientHumidity,1) + "%RH");
}

// ============================================================
// RELAY & LED CONTROL
// ============================================================
void applyRelayStates() {
    // Write relay outputs
    digitalWrite(PIN_RELAY_HEATER, heaterOn ? HIGH : LOW);
    digitalWrite(PIN_RELAY_REFRIG, refrigOn ? HIGH : LOW);
    digitalWrite(PIN_RELAY_FAN,    fanOn    ? HIGH : LOW);

    // Mirror relay state to status LEDs
    digitalWrite(PIN_LED_HEATER, heaterOn ? HIGH : LOW);
    digitalWrite(PIN_LED_REFRIG, refrigOn ? HIGH : LOW);
    digitalWrite(PIN_LED_FAN,    fanOn    ? HIGH : LOW);

    Serial.println("[RELAY] Heater:" + String(heaterOn ? "ON ":"OFF") +
                   "  Refrig:" + String(refrigOn ? "ON ":"OFF") +
                   "  Fan:"    + String(fanOn    ? "ON ":"OFF"));
}

// ============================================================
// PUSH SENSOR DATA TO FIREBASE
// ============================================================
void pushToFirebase() {
    if (!Firebase.ready()) {
        firebaseReady = Firebase.ready();
        return;
    }

    String base = String(FIREBASE_BASE_PATH);
    bool ok = true;

    // Sensor data
    ok &= Firebase.RTDB.setFloat(&fbdo,  base + "/sensors/temp1",            temp1);
    ok &= Firebase.RTDB.setFloat(&fbdo,  base + "/sensors/temp2",            temp2);
    ok &= Firebase.RTDB.setFloat(&fbdo,  base + "/sensors/ambient_temp",     ambientTemp);
    ok &= Firebase.RTDB.setFloat(&fbdo,  base + "/sensors/ambient_humidity", ambientHumidity);

    // Relay states (actual hardware state)
    ok &= Firebase.RTDB.setBool(&fbdo,   base + "/relays/heater_state", heaterOn);
    ok &= Firebase.RTDB.setBool(&fbdo,   base + "/relays/refrig_state", refrigOn);
    ok &= Firebase.RTDB.setBool(&fbdo,   base + "/relays/fan_state",    fanOn);

    // Device diagnostics
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/rssi",     WiFi.RSSI());
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/free_heap", ESP.getFreeHeap());
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/uptime_s",  millis() / 1000);

    if (ok) {
        Serial.println("[FB] Data pushed to Firebase successfully");
    } else {
        Serial.println("[FB] Push error: " + fbdo.errorReason());
    }
}

// ============================================================
// READ RELAY COMMANDS FROM FIREBASE (remote control)
// ============================================================
void readRelayCommandsFromFirebase() {
    if (!Firebase.ready()) return;

    String base = String(FIREBASE_BASE_PATH) + "/relays/control";

    bool cmd;

    if (Firebase.RTDB.getBool(&fbdo, base + "/heater", &cmd)) {
        heaterOn = cmd;
    }
    if (Firebase.RTDB.getBool(&fbdo, base + "/refrig", &cmd)) {
        refrigOn = cmd;
    }
    if (Firebase.RTDB.getBool(&fbdo, base + "/fan",    &cmd)) {
        fanOn = cmd;
    }

    Serial.println("[FB] Commands read - Heater:" + String(heaterOn ? "ON":"OFF") +
                   " Refrig:" + String(refrigOn ? "ON":"OFF") +
                   " Fan:"    + String(fanOn    ? "ON":"OFF"));
}

// ============================================================
// WiFi INITIALIZATION & PROVISIONING
// ============================================================
void initializeWiFi() {
    wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
    wifiManager.setConnectTimeout(WIFI_TIMEOUT);
    wifiManager.setEnableConfigPortal(true);
    setupWiFiCallbacks();

    Serial.println("[*] Attempting to reconnect to last known WiFi...");

    bool connected = false;
    for (int i = 0; i < 3 && !connected; i++) {
        Serial.println("[*] Connection attempt " + String(i + 1) + " of 3");
        connected = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
        if (!connected && i < 2) { Serial.println("[!] Retrying in 5 s..."); delay(5000); }
    }

    if (connected) {
        provisioningMode = false;
        Serial.println("[OK] WiFi connected!");
        Serial.println("[OK] SSID: "   + String(WiFi.SSID()));
        Serial.println("[OK] IP:   "   + WiFi.localIP().toString());
        Serial.println("[OK] RSSI: "   + String(WiFi.RSSI()) + " dBm");
    } else {
        provisioningMode = true;
        Serial.println("[!] Connected failed - portal started");
        Serial.println("[*] Connect to hotspot: " AP_SSID);
        Serial.println("[*] Open: http://192.168.4.1");
    }
}

void setupWiFiCallbacks() {
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("[OK] WiFi credentials saved!");
        provisioningMode = false;
    });
    wifiManager.setAPCallback([](WiFiManager *wm) {
        Serial.println("[*] Provisioning portal active: " + wm->getConfigPortalSSID());
        // Fast-blink the fan LED to indicate provisioning mode
        for (int i = 0; i < 6; i++) {
            digitalWrite(PIN_LED_FAN, HIGH); delay(150);
            digitalWrite(PIN_LED_FAN, LOW);  delay(150);
        }
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
    http.setConnectTimeout(10000);
    Serial.println("[*] Fetching version info from: " VERSION_JSON_URL);

    if (!http.begin(VERSION_JSON_URL)) {
        Serial.println("[!] Failed to begin HTTP request");
        return;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.println("[!] HTTP Error: " + String(httpCode));
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Serial.println("[!] JSON parse error");
        return;
    }

    String latestVersion = doc["version"]      | "0.0.0";
    String downloadUrl   = doc["download_url"] | "";
    String sha256        = doc["sha256"]        | "";

    Serial.println("[*] Latest: " + latestVersion + "  Current: " FIRMWARE_VERSION);

    if (latestVersion != FIRMWARE_VERSION && !downloadUrl.isEmpty()) {
        Serial.println("[!] New firmware available - starting OTA...");
        performOTAUpdate(downloadUrl, sha256);
    } else {
        Serial.println("[OK] Firmware is up to date");
    }
}

void performOTAUpdate(String firmwareUrl, String expectedSha256) {
    Serial.println("[*] Starting OTA update process...");

    HTTPClient http;
    http.setConnectTimeout(30000);
    http.setTimeout(30000);

    for (int redirectCount = 0; redirectCount < 3; redirectCount++) {
        Serial.println("[*] Attempt " + String(redirectCount + 1) +
                       ": Connecting to " + firmwareUrl);

        if (!http.begin(firmwareUrl)) {
            Serial.println("[!] Failed to connect to firmware URL");
            http.end();
            return;
        }

        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        int httpCode = http.GET();
        Serial.println("[*] HTTP Response: " + String(httpCode));

        if (httpCode == 301 || httpCode == 302 || httpCode == 307) {
            String loc = http.header("Location");
            http.end();
            if (loc.length() > 0) { firmwareUrl = loc; continue; }
        }

        if (httpCode == HTTP_CODE_OK) break;

        http.end();
        if (redirectCount < 2) { delay(2000); } else { return; }
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[!] Invalid content length");
        http.end();
        return;
    }

    Serial.println("[*] Firmware size: " + String(contentLength) + " bytes");

    if (!Update.begin(contentLength)) {
        Serial.println("[!] Not enough space for OTA (free: " +
                       String(ESP.getFreeSketchSpace()) + " bytes)");
        http.end();
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    if (!stream) { Serial.println("[!] Stream error"); http.end(); return; }

    size_t written = 0;
    uint8_t buff[512] = {0};
    unsigned long lastProgress = millis();

    while (http.connected() && written < (size_t)contentLength) {
        size_t avail = stream->available();
        if (avail) {
            int r = stream->readBytes(buff, min(avail, sizeof(buff)));
            if (r > 0) { Update.write(buff, r); written += r; }
        }
        if (millis() - lastProgress > 2000) {
            lastProgress = millis();
            Serial.println("[*] Progress: " + String((written * 100) / contentLength) +
                           "% (" + String(written) + "/" + String(contentLength) + ")");
        }
        delay(10);
    }
    http.end();

    if (written != (size_t)contentLength) {
        Serial.println("[!] Download incomplete - aborting");
        Update.abort();
        return;
    }

    if (Update.end() && Update.isFinished()) {
        // Signal all LEDs before reboot
        for (int i = 0; i < 5; i++) {
            digitalWrite(PIN_LED_HEATER, HIGH);
            digitalWrite(PIN_LED_REFRIG, HIGH);
            digitalWrite(PIN_LED_FAN,    HIGH);
            delay(100);
            digitalWrite(PIN_LED_HEATER, LOW);
            digitalWrite(PIN_LED_REFRIG, LOW);
            digitalWrite(PIN_LED_FAN,    LOW);
            delay(100);
        }
        Serial.println("[OK] OTA complete - restarting...");
        delay(3000);
        ESP.restart();
    } else {
        Serial.println("[!] OTA failed: " + String(Update.getError()));
        Update.abort();
    }
}
