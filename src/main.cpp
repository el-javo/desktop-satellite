#include <Arduino.h>
#include <WiFi.h>

// Analog input pins
const int LDR_PIN_A = 15; 
const int LDR_PIN_B = 4;  
const int LED_PIN_A = 18;  
const int LED_PIN_B = 19;  
const float DIFF_THRESHOLD_POS = 0.1f;
const float DIFF_THRESHOLD_NEG = -0.1f;

// Read interval
const unsigned long READ_INTERVAL_MS = 30;
const unsigned long ACTION_INTERVAL_MS = 300;
unsigned long last_read_time = 0;
uint32_t sum_a = 0;
uint32_t sum_b = 0;
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

        sum_a += (uint32_t)value_a;
        sum_b += (uint32_t)value_b;
        diff_count++;

        const unsigned int samples_per_action =
            (READ_INTERVAL_MS > 0) ? max(1UL, ACTION_INTERVAL_MS / READ_INTERVAL_MS) : 1U;

        if (diff_count >= samples_per_action) {
            const uint32_t total_sum = sum_a + sum_b;
            float diff_avg = 0.0f;
            if (total_sum > 0) {
                diff_avg =
                    ((float)((int32_t)sum_a - (int32_t)sum_b) /
                     (float)total_sum) * 100.0f;
            }

            // Safety clamp
            diff_avg = constrain(diff_avg, -100.0f, 100.0f);

            const uint32_t avg_a = sum_a / diff_count;
            const uint32_t avg_b = sum_b / diff_count;

            sum_a = 0;
            sum_b = 0;
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
            Serial.print(avg_a);
            Serial.print(" | LDR_B=");
            Serial.print(avg_b);
            Serial.print(" | DIFF_AVG=");
            Serial.print(diff_avg, 2);
            Serial.println(" %");
        }
    }
}
