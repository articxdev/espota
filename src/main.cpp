// ============================================================
// ESP32 TEMPERATURE CONTROL SYSTEM
// WiFi Provisioning + OTA + Sensors + Firebase Realtime DB
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
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
#include <time.h>                 // NTP time sync
#include "config.h"

// ============================================================
// WIRELESS SERIAL MONITOR (TELNET)
// - Mirrors Serial logs to a TCP client on port 23
// - Single client at a time
// ============================================================
static const uint16_t TELNET_LOG_PORT = 23;
WiFiServer telnetServer(TELNET_LOG_PORT);
WiFiClient telnetClient;
bool telnetServerStarted = false;

class WiFiSerialLogger : public Print {
public:
    WiFiSerialLogger() = default;

    void attachSerial(HardwareSerial *serial) { serial_ = serial; }
    void setClient(WiFiClient *client) { client_ = client; }

    size_t write(uint8_t b) override {
        if (serial_) {
            serial_->write(b);
        }
        if (client_ && client_->connected()) {
            client_->write(b);
        }
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        if (serial_) {
            serial_->write(buffer, size);
        }
        if (client_ && client_->connected()) {
            client_->write(buffer, size);
        }
        return size;
    }

private:
    HardwareSerial *serial_ = nullptr;
    WiFiClient *client_ = nullptr;
};

WiFiSerialLogger Log;

// NTP Configuration
#define NTP_SERVER      "pool.ntp.org"
#define GMT_OFFSET_SEC  19800       // IST = GMT+5:30 = 5.5*3600
#define DAYLIGHT_OFFSET 0

// ============================================================
// HARDWARE OBJECTS
// ============================================================
OneWire         oneWire1(PIN_TEMP_SENSOR1);
OneWire         oneWire2(PIN_TEMP_SENSOR2);
DallasTemperature tempSensor1(&oneWire1);
DallasTemperature tempSensor2(&oneWire2);
Adafruit_SHT31  sht30;  // Library works for both SHT30/SHT31

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
unsigned long bootStartMillis   = 0;
unsigned long lastRelayControlRead = 0;

// ============================================================
// SENSOR READINGS
// ============================================================
float temp1           = -127.0f;   // DS18B20 sensor 1
float temp2           = -127.0f;   // DS18B20 sensor 2
float ambientTemp     = -127.0f;   // SHT30 temperature
float ambientHumidity = 0.0f;      // SHT30 humidity

// ============================================================
// RELAY / LED STATES
// ============================================================
bool heaterOn = false;
bool refrigOn = false;
bool fanOn    = false;
bool autoRelayControl = RELAY_CONTROL_DEFAULT_AUTO_MODE;

// Fan cycle state (2 min ON / 1 min OFF)
unsigned long fanCycleStartMillis = 0;
bool fanCycleOnPhase = true;

// Dynamic Setpoints
float heaterOnTemp   = DEFAULT_HEATER_ON_TEMP_C;
float heaterOffTemp  = DEFAULT_HEATER_OFF_TEMP_C;
float refrigOnTemp   = DEFAULT_REFRIG_ON_TEMP2_C;
float refrigOffTemp  = DEFAULT_REFRIG_OFF_TEMP2_C;

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================
void initializePins();
void initializeSensors();
void initializeFirebase();
void initializeWiFi();
void setupWiFiCallbacks();
bool hasValidAPPassword();
bool startProvisioningPortal();
void handleTelnetLogger();
bool shouldHoldRelaysOff();
void enforceRelaysOff();
void readSensors();
void updateAutomaticControl();
void updateFanCycle();
void applyRelayStates();
void setAllLedsImmediate(bool on);
void writeRelay(uint8_t pin, bool on, bool activeLow);
bool isValidDs18b20(float value);
bool isValidAmbientTemp(float value);
void pushToFirebase();
void updateRelayControlModeFromFirebase();
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
    bootStartMillis = millis();

    Log.attachSerial(&Serial);
    Log.setClient(&telnetClient);

    Log.println("\n\n================================");
    Log.println("ESP32 TEMP CONTROL v" FIRMWARE_VERSION);
    Log.println("Author: " FIRMWARE_AUTHOR);
    Log.println("================================\n");

    initializePins();
    initializeSensors();

    Log.println("[*] Initializing WiFi provisioning...");
    initializeWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        // Sync time via NTP (needed for timestamps)
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
        Log.println("[*] Syncing time with NTP...");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            Log.println("[OK] Time synced: " + String(asctime(&timeinfo)));
        } else {
            Log.println("[!] NTP sync failed - timestamps may be inaccurate");
        }
        initializeFirebase();
    }

    lastOTACheck      = millis();
    lastFirebaseUpdate = millis();
    lastSensorRead    = millis();
    lastRelayControlRead = millis();

    // Start fan cycle in ON phase
    fanCycleStartMillis = millis();
    fanCycleOnPhase = true;
    enforceRelaysOff();

    Log.println("[OK] Setup complete - entering main loop");
    Log.println("================================\n");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    wifiManager.process();
    handleTelnetLogger();

    // ----- WiFi watchdog: reconnect every 5 s if lost ------
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 5000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED && !provisioningMode) {
            Log.println("[!] WiFi lost - attempting reconnect...");

            bool reconnected = false;
            if (hasValidAPPassword()) {
                reconnected = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
            } else {
                reconnected = wifiManager.autoConnect(AP_SSID);
            }

            if (reconnected && WiFi.status() == WL_CONNECTED) {
                Log.println("[OK] WiFi reconnected: " + WiFi.localIP().toString());
                if (!firebaseReady) initializeFirebase();
            } else {
                Log.println("[!] Saved WiFi unavailable - starting visible provisioning hotspot");
                if (startProvisioningPortal() && !firebaseReady) {
                    initializeFirebase();
                }
            }
        }
    }

    // ----- Read sensors every 2 s --------------------------
    if (millis() - lastSensorRead > 2000) {
        lastSensorRead = millis();
        readSensors();

        if (shouldHoldRelaysOff()) {
            enforceRelaysOff();
        } else {
            const bool canReadRemoteControl = (WiFi.status() == WL_CONNECTED && firebaseReady);
            if (canReadRemoteControl && (millis() - lastRelayControlRead >= RELAY_CONTROL_PULL_INTERVAL_MS)) {
                lastRelayControlRead = millis();
                updateRelayControlModeFromFirebase();
                if (!autoRelayControl) {
                    readRelayCommandsFromFirebase();
                }
            }

            if (autoRelayControl) {
                updateAutomaticControl();
            }
            applyRelayStates();
        }
    }

    // ----- Firebase: push sensor data -----------------------
    if (WiFi.status() == WL_CONNECTED && firebaseReady) {
        if (millis() - lastFirebaseUpdate > FIREBASE_UPDATE_INTERVAL_MS) {
            lastFirebaseUpdate = millis();
            pushToFirebase();
        }
    }

    // ----- OTA check ----------------------------------------
    if (millis() - lastOTACheck > (OTA_CHECK_INTERVAL_SECONDS * 1000UL)) {
        lastOTACheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            Log.println("\n[*] Checking for firmware updates...");
            checkForUpdates();
        }
    }
}

// ============================================================
// TELNET LOGGER
// ============================================================
void handleTelnetLogger() {
    const bool staConnected = (WiFi.status() == WL_CONNECTED);
    const bool apActive = ((WiFi.getMode() & WIFI_AP) != 0);
    const bool anyNetworkUp = staConnected || apActive;

    if (!anyNetworkUp) {
        if (telnetClient && telnetClient.connected()) {
            telnetClient.stop();
        }
        telnetServerStarted = false;
        return;
    }

    if (!telnetServerStarted) {
        telnetServer.begin();
        telnetServer.setNoDelay(true);
        telnetServerStarted = true;
        Log.println("[TELNET] Log server started on port " + String(TELNET_LOG_PORT));
    }

#if defined(ARDUINO_ARCH_ESP32)
    if (telnetServer.hasClient()) {
        WiFiClient newClient = telnetServer.available();
        if (newClient) {
            if (telnetClient && telnetClient.connected()) {
                telnetClient.stop();
            }
            telnetClient = newClient;
            telnetClient.setNoDelay(true);
            telnetClient.println("[TELNET] Connected to ESP32 log stream");
            if (staConnected) {
                telnetClient.println("[TELNET] STA IP: " + WiFi.localIP().toString());
            }
            if (apActive) {
                telnetClient.println("[TELNET]  AP IP: " + WiFi.softAPIP().toString());
            }
        }
    }
#else
    if (!telnetClient || !telnetClient.connected()) {
        WiFiClient newClient = telnetServer.available();
        if (newClient) {
            telnetClient = newClient;
        }
    }
#endif

    if (telnetClient && telnetClient.connected()) {
        // Drain any incoming bytes (we don't accept commands here)
        while (telnetClient.available() > 0) {
            (void)telnetClient.read();
        }
    }
}

// Redirect all Serial.print/println in the remainder of this file to Log (USB + telnet).
// (Placed here to keep Serial.begin() and logger attachment clean in setup())
#define Serial Log

// ============================================================
// PIN INITIALIZATION
// ============================================================
void initializePins() {
    // Relays
    pinMode(PIN_RELAY_HEATER, OUTPUT); writeRelay(PIN_RELAY_HEATER, false, RELAY_HEATER_ACTIVE_LOW);
    pinMode(PIN_RELAY_REFRIG, OUTPUT); writeRelay(PIN_RELAY_REFRIG, false, RELAY_REFRIG_ACTIVE_LOW);
    pinMode(PIN_RELAY_FAN,    OUTPUT); writeRelay(PIN_RELAY_FAN,    false, RELAY_FAN_ACTIVE_LOW);

    // Status LEDs
    pinMode(PIN_LED_HEATER, OUTPUT);
    pinMode(PIN_LED_REFRIG, OUTPUT);
    pinMode(PIN_LED_FAN,    OUTPUT);

    setAllLedsImmediate(false);

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

    // I2C (SHT30) sensor
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (sht30.begin(I2C_SENSOR_ADDR)) {
        Serial.println("[OK] SHT30 I2C sensor ready at address 0x" +
                       String(I2C_SENSOR_ADDR, HEX));
    } else {
        Serial.println("[!] SHT30 not found - check wiring / sensor address");
    }
}

// ============================================================
// FIREBASE INITIALIZATION
// ============================================================
// Helper: Get current epoch timestamp
unsigned long getEpochTime() {
    time_t now;
    time(&now);
    return (unsigned long)now;
}

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

        String base = String(FIREBASE_BASE_PATH);

        // Set online status with timestamp
        Firebase.RTDB.setString(&fbdo, base + "/status/state", "online");
        Firebase.RTDB.setString(&fbdo, base + "/status/firmware", FIRMWARE_VERSION);
        Firebase.RTDB.setInt(&fbdo, base + "/status/last_seen", getEpochTime());
        Firebase.RTDB.setInt(&fbdo, base + "/status/heartbeat_interval_s", FIREBASE_UPDATE_INTERVAL_MS / 1000);

        bool remoteAutoMode = RELAY_CONTROL_DEFAULT_AUTO_MODE;
        const String modePath = base + "/relays/auto_mode";
        if (Firebase.RTDB.getBool(&fbdo, modePath, &remoteAutoMode)) {
            autoRelayControl = remoteAutoMode;
        } else {
            autoRelayControl = RELAY_CONTROL_DEFAULT_AUTO_MODE;
            Firebase.RTDB.setBool(&fbdo, modePath, autoRelayControl);
        }
        
        // NOTE: For automatic offline detection, your app should check:
        // if (current_time - last_seen > heartbeat_interval_s * 2) then device is OFFLINE
        // Firebase ESP Client doesn't support onDisconnect like JS SDK
        
        // Push fallback defaults to Firebase on first boot if they don't exist
        float dummyTemp;
        if (!Firebase.RTDB.getFloat(&fbdo, base + "/settings/heater_onTemp", &dummyTemp)) {
            Firebase.RTDB.setFloat(&fbdo, base + "/settings/heater_onTemp", DEFAULT_HEATER_ON_TEMP_C);
            Firebase.RTDB.setFloat(&fbdo, base + "/settings/heater_offTemp", DEFAULT_HEATER_OFF_TEMP_C);
            Firebase.RTDB.setFloat(&fbdo, base + "/settings/refrig_onTemp", DEFAULT_REFRIG_ON_TEMP2_C);
            Firebase.RTDB.setFloat(&fbdo, base + "/settings/refrig_offTemp", DEFAULT_REFRIG_OFF_TEMP2_C);
        }

        Serial.println("[OK] Presence timestamps enabled - app should check last_seen");
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

    // SHT30 I2C Sensor (pins 21, 22)
    float t = sht30.readTemperature();
    float h = sht30.readHumidity();
    if (!isnan(t)) ambientTemp     = t;
    if (!isnan(h)) ambientHumidity = h;

    Serial.println("[SENSOR] Temp1: " + String(temp1,1) + "°C  |  " +
                   "Temp2: " + String(temp2,1) + "°C  |  " +
                   "Ambient: " + String(ambientTemp,1) + "°C  " +
                   String(ambientHumidity,1) + "%RH");
}

// ============================================================
// AUTOMATIC CONTROL LOGIC
// ============================================================
bool isValidDs18b20(float value) {
    return (value > -126.0f && value < 85.0f);
}

bool isValidAmbientTemp(float value) {
    return (value > -40.0f && value < 125.0f);
}

bool shouldHoldRelaysOff() {
    const bool startupHoldActive = (millis() - bootStartMillis) < RELAY_STARTUP_HOLDOFF_MS;
    const bool provisioningHoldActive = RELAYS_OFF_DURING_PROVISIONING && provisioningMode;
    return startupHoldActive || provisioningHoldActive;
}

void enforceRelaysOff() {
    heaterOn = false;
    refrigOn = false;
    fanOn = false;
    applyRelayStates();
}

void updateFanCycle() {
    const unsigned long now = millis();
    const unsigned long phaseDuration = fanCycleOnPhase ? FAN_ON_DURATION_MS : FAN_OFF_DURATION_MS;

    if (now - fanCycleStartMillis >= phaseDuration) {
        fanCycleOnPhase = !fanCycleOnPhase;
        fanCycleStartMillis = now;

        Serial.println("[CTRL] Fan cycle phase -> " + String(fanCycleOnPhase ? "ON (2 min)" : "OFF (1 min)"));
    }

    fanOn = fanCycleOnPhase;
}

void updateAutomaticControl() {
    updateFanCycle();

    const bool temp1Valid = isValidDs18b20(temp1);
    const bool ambientValid = isValidAmbientTemp(ambientTemp);
    if (temp1Valid && ambientValid) {
        const float avgTemp = (temp1 + ambientTemp) / 2.0f;

        if (avgTemp > heaterOffTemp) {
            heaterOn = false;
        } else if (avgTemp < heaterOnTemp) {
            heaterOn = true;
        }

        Serial.println("[CTRL] Heater avg(temp1+i2c)/2 = " + String(avgTemp, 2) +
                       "°C (ON<" + String(heaterOnTemp, 1) +
                       ", OFF>" + String(heaterOffTemp, 1) + ")");
    } else {
        Serial.println("[CTRL] Heater control skipped (invalid temp1 or i2c temp)");
    }

    const bool temp2Valid = isValidDs18b20(temp2);
    if (temp2Valid) {
        if (temp2 < refrigOffTemp) {
            refrigOn = false;
        } else if (temp2 > refrigOnTemp) {
            refrigOn = true;
        }

        Serial.println("[CTRL] Refrig control temp2 = " + String(temp2, 2) +
                       "°C (OFF<" + String(refrigOffTemp, 1) +
                       ", ON>" + String(refrigOnTemp, 1) + ")");
    } else {
        Serial.println("[CTRL] Refrig control skipped (invalid temp2)");
    }
}

// ============================================================
// RELAY & LED CONTROL
// ============================================================
void applyRelayStates() {
    // Write relay outputs (supports active-low relay modules)
    writeRelay(PIN_RELAY_HEATER, HEATER_OUTPUT_INVERTED ? !heaterOn : heaterOn, RELAY_HEATER_ACTIVE_LOW);
    writeRelay(PIN_RELAY_REFRIG, REFRIG_OUTPUT_INVERTED ? !refrigOn : refrigOn, RELAY_REFRIG_ACTIVE_LOW);
    writeRelay(PIN_RELAY_FAN,    FAN_OUTPUT_INVERTED ? !fanOn : fanOn, RELAY_FAN_ACTIVE_LOW);

    // Set LEDs directly
    digitalWrite(PIN_LED_HEATER, heaterOn ? HIGH : LOW);
    digitalWrite(PIN_LED_REFRIG, refrigOn ? HIGH : LOW);
    digitalWrite(PIN_LED_FAN,    fanOn    ? HIGH : LOW);

    Serial.println("[RELAY] Heater:" + String(heaterOn ? "ON ":"OFF") +
                   "  Refrig:" + String(refrigOn ? "ON ":"OFF") +
                   "  Fan:"    + String(fanOn    ? "ON ":"OFF"));
}

void writeRelay(uint8_t pin, bool on, bool activeLow) {
    const bool level = activeLow ? !on : on;
    
    if (activeLow) {
        // OPEN-DRAIN SIMULATION: 3.3V ESP32 trying to turn off a 5V Active-Low relay.
        // A standard 3.3V HIGH is not enough to stop the 5V current loop in the relay's optocoupler.
        if (level == HIGH) {
            // Turn relay OFF -> Disconnect pin (float) so no current flows to ground
            pinMode(pin, INPUT);
        } else {
            // Turn relay ON -> Pull to ground
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
    } else {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, level ? HIGH : LOW);
    }
}

void setAllLedsImmediate(bool on) {
    digitalWrite(PIN_LED_HEATER, on ? HIGH : LOW);
    digitalWrite(PIN_LED_REFRIG, on ? HIGH : LOW);
    digitalWrite(PIN_LED_FAN,    on ? HIGH : LOW);
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
    unsigned long now = getEpochTime();

    // Check sensor validity (DS18B20 returns -127 when disconnected)
    bool temp1Valid = (temp1 > -126.0f && temp1 < 85.0f);
    bool temp2Valid = (temp2 > -126.0f && temp2 < 85.0f);
    bool ambientValid = (ambientTemp > -40.0f && ambientTemp < 125.0f);
    bool anySensorValid = temp1Valid || temp2Valid || ambientValid;

    // Explicit unit marker for all temperature readings
    ok &= Firebase.RTDB.setString(&fbdo, base + "/sensors/temperature_unit", TEMPERATURE_UNIT_LABEL);

    // Sensor data (only push valid readings)
    if (temp1Valid) {
        ok &= Firebase.RTDB.setFloat(&fbdo, base + "/sensors/temp1", temp1);
    } else {
        ok &= Firebase.RTDB.setString(&fbdo, base + "/sensors/temp1", "disconnected");
    }

    if (temp2Valid) {
        ok &= Firebase.RTDB.setFloat(&fbdo, base + "/sensors/temp2", temp2);
    } else {
        ok &= Firebase.RTDB.setString(&fbdo, base + "/sensors/temp2", "disconnected");
    }

    if (ambientValid) {
        ok &= Firebase.RTDB.setFloat(&fbdo, base + "/sensors/ambient_temp", ambientTemp);
        ok &= Firebase.RTDB.setFloat(&fbdo, base + "/sensors/ambient_humidity", ambientHumidity);
    } else {
        ok &= Firebase.RTDB.setString(&fbdo, base + "/sensors/ambient_temp", "disconnected");
        ok &= Firebase.RTDB.setString(&fbdo, base + "/sensors/ambient_humidity", "disconnected");
    }

    // Sensor validity flag and timestamp
    ok &= Firebase.RTDB.setBool(&fbdo, base + "/sensors/valid", anySensorValid);
    ok &= Firebase.RTDB.setInt(&fbdo,  base + "/sensors/last_update", now);

    // Relay states (actual hardware state)
    ok &= Firebase.RTDB.setBool(&fbdo, base + "/relays/heater_state", heaterOn);
    ok &= Firebase.RTDB.setBool(&fbdo, base + "/relays/refrig_state", refrigOn);
    ok &= Firebase.RTDB.setBool(&fbdo, base + "/relays/fan_state",    fanOn);
    ok &= Firebase.RTDB.setBool(&fbdo, base + "/relays/auto_mode_state", autoRelayControl);

    // Device diagnostics with timestamp
    ok &= Firebase.RTDB.setString(&fbdo, base + "/status/state", "online");
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/last_seen", now);
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/rssi",     WiFi.RSSI());
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/free_heap", ESP.getFreeHeap());
    ok &= Firebase.RTDB.setInt(&fbdo,    base + "/status/uptime_s",  millis() / 1000);

    if (ok) {
        Serial.println("[FB] Data pushed (sensors valid: " + String(anySensorValid ? "yes" : "no") + ")");
    } else {
        Serial.println("[FB] Push error: " + fbdo.errorReason());
    }
}

// ============================================================
// READ RELAY CONTROL MODE FROM FIREBASE
// ============================================================
void updateRelayControlModeFromFirebase() {
    if (!Firebase.ready()) return;

    const String basePath = String(FIREBASE_BASE_PATH);
    const String modePath = basePath + "/relays/auto_mode";
    
    // Auto Mode Check
    bool remoteAutoMode = autoRelayControl;
    if (Firebase.RTDB.getBool(&fbdo, modePath, &remoteAutoMode)) {
        if (remoteAutoMode != autoRelayControl) {
            autoRelayControl = remoteAutoMode;
            if (autoRelayControl) {
                fanCycleStartMillis = millis();
                fanCycleOnPhase = true;
            }
            Serial.println("[FB] Relay mode -> " + String(autoRelayControl ? "AUTO" : "MANUAL"));
        }
    }
    
    // Setpoints Check
    float t;
    if (Firebase.RTDB.getFloat(&fbdo, basePath + "/settings/heater_onTemp", &t))  heaterOnTemp = t;
    if (Firebase.RTDB.getFloat(&fbdo, basePath + "/settings/heater_offTemp", &t)) heaterOffTemp = t;
    if (Firebase.RTDB.getFloat(&fbdo, basePath + "/settings/refrig_onTemp", &t))  refrigOnTemp = t;
    if (Firebase.RTDB.getFloat(&fbdo, basePath + "/settings/refrig_offTemp", &t)) refrigOffTemp = t;
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
bool hasValidAPPassword() {
    const size_t apPasswordLength = strlen(AP_PASSWORD);
    return (apPasswordLength >= 8 && apPasswordLength <= 63);
}

bool startProvisioningPortal() {
    provisioningMode = true;

    Serial.println("[*] Starting provisioning portal...");
    Serial.println("[*] Connect to hotspot: " AP_SSID);
    Serial.println("[*] Open: http://192.168.4.1");

    bool connected = false;
    if (hasValidAPPassword()) {
        connected = wifiManager.startConfigPortal(AP_SSID, AP_PASSWORD);
    } else {
        Serial.println("[!] AP_PASSWORD length invalid for WPA2 AP - starting open hotspot");
        connected = wifiManager.startConfigPortal(AP_SSID);
    }

    provisioningMode = false;
    return connected && WiFi.status() == WL_CONNECTED;
}

void initializeWiFi() {
    wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
    wifiManager.setConnectTimeout(WIFI_TIMEOUT);
    wifiManager.setConnectRetries(1);
    wifiManager.setWiFiAutoReconnect(true);
    wifiManager.setWiFiAPHidden(AP_HIDDEN);
    wifiManager.setEnableConfigPortal(false); // Manual fallback to portal for predictable failover timing
    setupWiFiCallbacks();

    Serial.println("[*] Attempting to reconnect to last known WiFi...");

    bool connected = false;
    if (hasValidAPPassword()) {
        connected = wifiManager.autoConnect(AP_SSID, AP_PASSWORD);
    } else {
        connected = wifiManager.autoConnect(AP_SSID);
    }

    if (connected) {
        provisioningMode = false;
        Serial.println("[OK] WiFi connected!");
        Serial.println("[OK] SSID: "   + String(WiFi.SSID()));
        Serial.println("[OK] IP:   "   + WiFi.localIP().toString());
        Serial.println("[OK] RSSI: "   + String(WiFi.RSSI()) + " dBm");
    } else {
        Serial.println("[!] Saved WiFi unavailable - switching to provisioning hotspot");
        if (!startProvisioningPortal()) {
            Serial.println("[!] Provisioning portal closed without WiFi connection");
        }
    }
}

void setupWiFiCallbacks() {
    wifiManager.setSaveConfigCallback([]() {
        Serial.println("[OK] WiFi credentials saved!");
        provisioningMode = false;
    });
    wifiManager.setAPCallback([](WiFiManager *wm) {
        provisioningMode = true;
        Serial.println("[*] Provisioning portal active: " + wm->getConfigPortalSSID());
        // Fast-blink the fan LED to indicate provisioning mode
        for (int i = 0; i < 6; i++) {
            digitalWrite(PIN_LED_FAN, HIGH); delay(150);
            digitalWrite(PIN_LED_FAN, LOW);  delay(150);
        }
        digitalWrite(PIN_LED_FAN, fanOn ? HIGH : LOW);
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
            setAllLedsImmediate(true);
            delay(100);
            setAllLedsImmediate(false);
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
