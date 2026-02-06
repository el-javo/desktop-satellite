#include <Arduino.h>
#include <WiFi.h>

//PINS
const int LDR_PIN_A = 15; 
const int LDR_PIN_B = 4;  
const int LED_PIN_A = 18;  
const int LED_PIN_B = 19;  
const int MOTOR_IN1_PIN = 16;
const int MOTOR_IN2_PIN = 17;
//Tracking config
const float DIFF_THRESHOLD_POS = 1.5f; //0..100 //? Param
const float DIFF_THRESHOLD_NEG = -1.5f; //0..-100 //? Param
//Motor config
const int MOTOR_PWM_FREQ = 20000;
const int MOTOR_PWM_RES_BITS = 8;
const int MOTOR_PWM_CH_IN1 = 0;
const int MOTOR_PWM_CH_IN2 = 1;
const float MOTOR_PWM_MIN_NORM = 0.3f; // 0..1 //? Param
const float MOTOR_PWM_MAX_NORM = 0.9f; // 0..1 //? Param
const float MOTOR_PWM_SMOOTH = 0.01f;   // 0..1 (0 = inmediato, 1 = muy suave) //? Param
const float DIFF_PWM_THRESHOLD = 30.0f; // Diff (%) para pasar a PWM max //? Param
const uint32_t MOTOR_PWM_RANGE = (1U << MOTOR_PWM_RES_BITS) - 1U;

// Read interval
const unsigned long READ_INTERVAL_MS = 30;
const unsigned long ACTION_INTERVAL_MS = 300;
unsigned long last_read_time = 0;
uint32_t sum_a = 0;
uint32_t sum_b = 0;
unsigned int diff_count = 0;
float motor_pwm_target = 0.0f;
float motor_pwm_filtered = 0.0f;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF);

    pinMode(LED_PIN_A, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);

    ledcSetup(MOTOR_PWM_CH_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
    ledcSetup(MOTOR_PWM_CH_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
    ledcAttachPin(MOTOR_IN1_PIN, MOTOR_PWM_CH_IN1);
    ledcAttachPin(MOTOR_IN2_PIN, MOTOR_PWM_CH_IN2);
    ledcWrite(MOTOR_PWM_CH_IN1, 0);
    ledcWrite(MOTOR_PWM_CH_IN2, 0);

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

        bool log_now = false;
        uint32_t log_avg_a = 0;
        uint32_t log_avg_b = 0;
        float log_diff = 0.0f;

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

            const float diff_abs = fabsf(diff_avg);
            const float pwm_min_norm = constrain(MOTOR_PWM_MIN_NORM, 0.0f, 1.0f);
            const float pwm_max_norm = constrain(MOTOR_PWM_MAX_NORM, 0.0f, 1.0f);
            const uint32_t pwm_min_raw =
                (uint32_t)lroundf(pwm_min_norm * (float)MOTOR_PWM_RANGE);
            const uint32_t pwm_max_raw =
                (uint32_t)lroundf(pwm_max_norm * (float)MOTOR_PWM_RANGE);
            const uint32_t pwm_low = min(pwm_min_raw, pwm_max_raw);
            const uint32_t pwm_high = max(pwm_min_raw, pwm_max_raw);

            if (diff_avg > DIFF_THRESHOLD_POS) {
                motor_pwm_target =
                    (diff_abs >= DIFF_PWM_THRESHOLD) ? (float)pwm_high : (float)pwm_low;
            } else if (diff_avg < DIFF_THRESHOLD_NEG) {
                motor_pwm_target =
                    -((diff_abs >= DIFF_PWM_THRESHOLD) ? (float)pwm_high : (float)pwm_low);
            } else {
                motor_pwm_target = 0.0f;
            }

            log_now = true;
            log_avg_a = avg_a;
            log_avg_b = avg_b;
            log_diff = diff_avg;
        }

        const float smooth = constrain(MOTOR_PWM_SMOOTH, 0.0f, 1.0f);
        const float alpha = 1.0f - smooth;
        motor_pwm_filtered += (motor_pwm_target - motor_pwm_filtered) * alpha;
        const float pwm_filtered_abs = fabsf(motor_pwm_filtered);
        const uint32_t pwm = (uint32_t)constrain(
            (int)lroundf(pwm_filtered_abs), 0, (int)MOTOR_PWM_RANGE);

        if (motor_pwm_filtered > 0.0f) {
            ledcWrite(MOTOR_PWM_CH_IN1, pwm);
            ledcWrite(MOTOR_PWM_CH_IN2, 0);
        } else if (motor_pwm_filtered < 0.0f) {
            ledcWrite(MOTOR_PWM_CH_IN1, 0);
            ledcWrite(MOTOR_PWM_CH_IN2, pwm);
        } else {
            ledcWrite(MOTOR_PWM_CH_IN1, 0);
            ledcWrite(MOTOR_PWM_CH_IN2, 0);
        }

        if (log_now) {
            // Serial log
            Serial.print("LDR_A=");
            Serial.print(log_avg_a);
            Serial.print(" | LDR_B=");
            Serial.print(log_avg_b);
            Serial.print(" | DIFF_AVG=");
            Serial.print(log_diff, 2);
            Serial.print(" %");
            Serial.print(" | PWM=");
            Serial.println(pwm);
        }
    }
}
