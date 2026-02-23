// ============================================================
// MINIMAL TEST FIRMWARE - Debug boot loop
// ============================================================

#include <Arduino.h>

void setup() {
    delay(1000);  // Let serial stabilize
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n================================");
    Serial.println("ESP32 TEST FIRMWARE STARTED");
    Serial.println("================================");
    
    // Configure GPIO 23
    pinMode(23, OUTPUT);
    digitalWrite(23, LOW);
    Serial.println("[OK] GPIO 23 initialized");
    
    Serial.println("[OK] Setup complete - entering main loop");
}

void loop() {
    // Blink LED
    digitalWrite(23, HIGH);
    Serial.println(">>> LED ON");
    delay(1000);
    
    digitalWrite(23, LOW);
    Serial.println("<<< LED OFF");
    delay(1000);
}
