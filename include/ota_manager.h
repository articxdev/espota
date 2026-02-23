#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "config.h"

// ============================================================
// OTA STATE MACHINE
// ============================================================

enum OTAState {
    OTA_IDLE = 0,
    OTA_CHECKING = 1,
    OTA_UPDATE_AVAILABLE = 2,
    OTA_DOWNLOADING = 3,
    OTA_INSTALLING = 4,
    OTA_SUCCESS = 5,
    OTA_FAILED = 6,
    OTA_ROLLEDBACK = 7
};

// ============================================================
// VERSION CHECK STRUCTURE
// ============================================================

struct FirmwareVersion {
    String version;
    String downloadUrl;
    String sha256;
    long fileSize;
    bool isValid;
};

// ============================================================
// OTA MANAGER CLASS
// ============================================================

class OTAManager {
public:
    OTAManager();
    
    // Initialize OTA system
    bool begin();
    
    // Check state of current firmware (called at boot)
    bool validateCurrentFirmware();
    
    // Check for updates from GitHub
    bool checkForUpdates();
    
    // Get latest version info
    FirmwareVersion getLatestVersion();
    
    // Download and install firmware
    bool downloadAndInstall(const String& downloadUrl);
    
    // Get OTA state
    OTAState getState();
    
    // Get progress percentage (0-100)
    int getProgress();
    
    // Get last error message
    String getLastError();
    
    // Manual rollback to previous firmware
    bool forcedRollback();
    
    // Mark current firmware as valid (done after boot validation)
    bool markCurrentFirmwareValid();

private:
    OTAState _state;
    int _progress;
    String _lastError;
    unsigned long _lastUpdateCheck;
    
    // Internal helper functions
    bool _validateSHA256(const String& hash);
    bool _downloadFile(const String& url, uint32_t& downloadedSize);
    bool _verifyDownloadHash(const String& expectedHash);
    esp_partition_t* _getNextOtaPartition();
};

// ============================================================
// GLOBAL OTA MANAGER INSTANCE
// ============================================================

extern OTAManager otaManager;

#endif // OTA_MANAGER_H
