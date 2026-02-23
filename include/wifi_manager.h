#ifndef WIFI_MANAGER_HEADER
#define WIFI_MANAGER_HEADER

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// ============================================================
// WiFi CONNECTION STATE
// ============================================================

enum WiFiConnectState {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_PROVISIONING = 1,
    WIFI_STATE_CONNECTING = 2,
    WIFI_STATE_CONNECTED = 3,
    WIFI_STATE_PROVISION_COMPLETE = 4,
    WIFI_STATE_ERROR = 5
};

// ============================================================
// WiFi PROVISIONING MANAGER
// ============================================================

class WiFiProvisioningManager {
public:
    WiFiProvisioningManager();
    
    // Initialize WiFi provisioning
    bool begin();
    
    // Start provisioning portal (hidden AP)
    bool startProvisioningPortal();
    
    // Stop provisioning portal
    void stopProvisioningPortal();
    
    // Connect to saved WiFi
    bool connectToSavedWiFi();
    
    // Manual WiFi connection
    bool connectToWiFi(const String& ssid, const String& password);
    
    // Get current WiFi state
    WiFiConnectState getState();
    
    // Get signal strength
    int getSignalStrength();
    
    // Get connected SSID
    String getConnectedSSID();
    
    // Get connected IP
    String getConnectedIP();
    
    // Trigger re-provisioning
    void triggerReset();
    
    // Handle WiFi events
    void handleWiFiEvent();
    
    // Get last error
    String getLastError();
    
    // Is provisioning needed?
    bool isProvisioningNeeded();
    
private:
    WiFiConnectState _state;
    String _lastError;
    String _connectedSSID;
    String _connectedIP;
    unsigned long _lastConnectionAttempt;
    int _connectionAttempts;
    
    // Internal helpers
    bool _loadCredentialsFromNVS();
    bool _saveCredentialsToNVS(const String& ssid, const String& password);
    bool _clearCredentialsFromNVS();
    void _handleConnectionTimeout();
};

// ============================================================
// GLOBAL WiFi MANAGER INSTANCE
// ============================================================

extern WiFiProvisioningManager wifiManager;

#endif // WIFI_MANAGER_HEADER
