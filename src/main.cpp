#include <Arduino.h>
#include <WiFi.h>

// Analog input pins
const int LDR_PIN_A = 15; 
const int LDR_PIN_B = 4;  
const int LED_PIN_A = 18;  
const int LED_PIN_B = 19;  

// Read interval
const unsigned long READ_INTERVAL_MS = 100;
unsigned long last_read_time = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    // ESP32 ADC configuration
    analogReadResolution(12);       // Range: 0–4095
    analogSetAttenuation(ADC_11db); // Up to ~3.3 V

    Serial.println("System initialized");
    Serial.println("Format: LDR_A | LDR_B | DIFF_%");
}

void loop() {
    unsigned long current_time = millis();

    if (current_time - last_read_time >= READ_INTERVAL_MS) {
        last_read_time = current_time;

        // Read voltage dividers
        int value_a = analogRead(LDR_PIN_A);
        int value_b = analogRead(LDR_PIN_B);

        float difference_percent = 0.0f;

        // Relative difference calculation
        if (value_a + value_b > 0) {
            difference_percent =
                ((float)(value_a - value_b) /
                 (float)(value_a + value_b)) * 100.0f;
        }

        // Safety clamp
        difference_percent = constrain(difference_percent, -100.0f, 100.0f);

        // Serial log
        Serial.print("LDR_A=");
        Serial.print(value_a);
        Serial.print(" | LDR_B=");
        Serial.print(value_b);
        Serial.print(" | DIFF=");
        Serial.print(difference_percent, 2);
        Serial.println(" %");
    }
}
