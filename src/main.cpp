#include <Arduino.h>
#include <WiFi.h>

// Analog input pins
const int LDR_PIN_A = 15; 
const int LDR_PIN_B = 4;  
const int LED_PIN_A = 18;  
const int LED_PIN_B = 19;  
const float DIFF_THRESHOLD_POS = 0.5f;
const float DIFF_THRESHOLD_NEG = -0.5f;

// Read interval
const unsigned long READ_INTERVAL_MS = 100;
const unsigned long ACTION_INTERVAL_MS = 500;
unsigned long last_read_time = 0;
float diff_sum = 0.0f;
unsigned int diff_count = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    pinMode(LED_PIN_A, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);

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

        diff_sum += difference_percent;
        diff_count++;

        const unsigned int samples_per_action =
            (READ_INTERVAL_MS > 0) ? max(1UL, ACTION_INTERVAL_MS / READ_INTERVAL_MS) : 1U;

        if (diff_count >= samples_per_action) {
            float diff_avg = diff_sum / (float)diff_count;
            diff_sum = 0.0f;
            diff_count = 0;

            if (diff_avg > DIFF_THRESHOLD_POS) {
                digitalWrite(LED_PIN_A, HIGH);
                digitalWrite(LED_PIN_B, LOW);
            } else if (diff_avg < DIFF_THRESHOLD_NEG) {
                digitalWrite(LED_PIN_A, LOW);
                digitalWrite(LED_PIN_B, HIGH);
            } else {
                digitalWrite(LED_PIN_A, LOW);
                digitalWrite(LED_PIN_B, LOW);
            }

            // Serial log
            Serial.print("LDR_A=");
            Serial.print(value_a);
            Serial.print(" | LDR_B=");
            Serial.print(value_b);
            Serial.print(" | DIFF_AVG=");
            Serial.print(diff_avg, 2);
            Serial.println(" %");
        }
    }
}
