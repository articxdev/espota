#define CONFIG_H

// ============================================================
// FIRMWARE CONFIGURATION
// ============================================================

// Firmware Version
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_AUTHOR "ESP32 Project"

// ============================================================
// WiFi PROVISIONING SETTINGS
// ============================================================

// AP Settings for first-time setup (hidden SSID)
#define AP_SSID "ESP32"          // Hidden WiFi hotspot SSID
#define AP_PASSWORD "esp12345"   // AP password (min 8 chars)
#define AP_HIDDEN true                   // Hide the setup hotspot

// WiFiManager settings
#define WIFI_TIMEOUT 180                 // Timeout in seconds for WiFi setup
#define WIFI_AUTOCONNECT true           // Auto-connect to stored WiFi
#define WIFI_PORTAL_TIMEOUT 300         // Portal timeout in seconds (5 mins)

// Portal web server settings
#define PORTAL_PORT 80                   // Web portal HTTP port
#define PORTAL_UPDATE_INTERVAL 1000      // Portal update interval (ms)

// ============================================================
// OTA UPDATE SETTINGS
// ============================================================

// GitHub OTA Configuration
#define GITHUB_OWNER "articxdev"    // GitHub username
#define GITHUB_REPO "espota"            // GitHub repository name
#define VERSION_JSON_URL "https://raw.githubusercontent.com/" \
                         GITHUB_OWNER "/" GITHUB_REPO "/main/version.json"
#define FIRMWARE_URL_BASE "https://github.com/" \
                          GITHUB_OWNER "/" GITHUB_REPO "/releases/download"

// OTA Check Interval
#define OTA_CHECK_INTERVAL_SECONDS 3600  // Check for updates every hour

// HTTPS verification (keep for security, disable for testing)
#define REQUIRE_HTTPS_CERT true

// HTTPS Certificate for GitHub (public CA cert)
#define GITHUB_CERT_CA \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIFazCCBFOgAwIBAgIRAIIQz7DSQONZRGPgu2FrpDAwDQYJKoZIhvcNAQELBQAw\n" \
  "RzELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGElu\n" \
  "dGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMzAxMDEwMDAwMDBaFw0yNDAxMDEw\n" \
  "MDAwMDBaMFYxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYD\n" \
  "VQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQxGDAWBgNVBAMMD2dpdGh1Yi5j\n" \
  "b20wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC7Q0Q7gKuEqNI3VlCp\n" \
  "vMy5nYd3tz6vfYHWqVIlA1QvM1wnmLe7B8XL7Y0xJIw7yC+yEm8qW3x+A5xI7qxm\n" \
  "2Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0\n" \
  "Z5Z0Z5Z0Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5\n" \
  "Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5Z0Z5\n" \
  "-----END CERTIFICATE-----"

// ============================================================
// GPIO HARDWARE CONTROL SETTINGS
// ============================================================

#define GPIO_OUTPUT_PIN 23      // GPIO 23 for LED/output control
#define LED_ON_DURATION_MS 2000  // Turn ON for 2 seconds
#define LED_OFF_DURATION_MS 2000 // Turn OFF for 2 seconds

// ============================================================
// SYSTEM SETTINGS
// ============================================================

// Watchdog timer
#define ENABLE_WATCHDOG true
#define WATCHDOG_TIMEOUT_SECONDS 30

// Serial debug
#define SERIAL_DEBUG true
#define SERIAL_BAUD_RATE 115200

// LED status indicators
#define LED_PROVISIONING_BLINK 500   // Fast blink while provisioning
#define LED_OTA_PROGRESS_BLINK 200   // Faster blink during OTA
#define LED_ERROR_BLINK 100           // Very fast blink on error

// ============================================================
// ROLLBACK SETTINGS
// ============================================================

// Rollback behavior: if new firmware crashes in first BOOT_TEST_DURATION,
// automatically rollback to previous version
#define BOOT_TEST_DURATION_SECONDS 30  // Give new firmware 30 seconds to prove itself
#define ROLLBACK_ENABLED true
#define ROLLBACK_CHECK_DELAY 2000      // Delay before checking if rollback occurred

// ============================================================
// MEMORY & STORAGE
// ============================================================

// NVS (Non-Volatile Storage) namespaces
#define NVS_PARTITION "nvs"
#define NVS_NAMESPACE_WIFI "wifi_config"
#define NVS_NAMESPACE_OTA "ota_status"
#define NVS_NAMESPACE_VERSION "fw_version"

// ============================================================
// LOGGING LEVELS
// ============================================================

#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4
#define LOG_TRACE 5

#define CURRENT_LOG_LEVEL LOG_INFO

#endif // CONFIG_H
