#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class DisplayManager {
public:
    enum class Mode {
        Off,
        Connecting,
        Tracking,
        Dashboard
    };

    struct Config {
        int pin_dc;
        int pin_rst;
        int pin_blk;
        unsigned long refresh_interval_ms;
    };

    explicit DisplayManager(const Config& cfg)
        : cfg_(cfg) {}

    void begin() {
        if (cfg_.pin_blk >= 0) {
            pinMode(cfg_.pin_blk, OUTPUT);
            digitalWrite(cfg_.pin_blk, HIGH);
        }

        tft_.init();
        tft_.setRotation(0);
        tft_.fillScreen(TFT_BLACK);

        mode_ = Mode::Off;
        last_draw_ms_ = 0;
        force_full_redraw_ = true;
    }

    void setMode(Mode mode) {
        if (mode_ != mode) {
            mode_ = mode;
            dirty_ = true;
            force_full_redraw_ = true;
        }
    }

    void setTrackingInfoH(float diff_percent) {
        diff_h_percent_ = diff_percent;
        dirty_ = true;
    }

    void setTrackingInfoHV(float diff_h, float diff_v) {
        diff_h_percent_ = diff_h;
        diff_v_percent_ = diff_v;
        dirty_ = true;
    }

    void setDeadbandPercent(float deadband_percent) {
        deadband_percent_ = deadband_percent;
        dirty_ = true;
    }

    void setPwmThresholdPercent(float pwm_threshold_percent) {
        pwm_threshold_percent_ = pwm_threshold_percent;
        dirty_ = true;
    }

    void setEnvironment(float temp_c, float humidity_pct) {
        temp_c_ = temp_c;
        humidity_pct_ = humidity_pct;
        dirty_ = true;
    }

    void tick(unsigned long now_ms) {
        if (!dirty_ && (now_ms - last_draw_ms_) < cfg_.refresh_interval_ms) {
            return;
        }
        last_draw_ms_ = now_ms;
        dirty_ = false;

        switch (mode_) {
        case Mode::Off:
            drawOff();
            break;
        case Mode::Connecting:
            drawConnecting();
            break;
        case Mode::Tracking:
            drawTracking();
            break;
        case Mode::Dashboard:
            drawDashboard();
            break;
        }
    }

private:
    void drawHeader(const char* title) {
        tft_.fillRect(0, 0, 240, 28, TFT_BLUE);
        tft_.setTextColor(TFT_YELLOW, TFT_BLUE);
        tft_.setTextDatum(TL_DATUM);
        if (tft_.drawString(title, 8, 6, 2) == 0) {
            tft_.drawString(title, 8, 8, 1);
        }
    }

    void drawOff() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("OFF");
    }

    void drawConnecting() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("CONNECT");
        tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft_.setTextSize(2);
        tft_.setCursor(20, 80);
        tft_.print("Connecting...");
        force_full_redraw_ = false;
    }

    void drawDashboard() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("DASH");
        drawEnvBlock(10, 40);
        force_full_redraw_ = false;
    }

    void drawTracking() {
        if (force_full_redraw_) {
            tft_.fillScreen(TFT_BLACK);
        }
        drawHeader("TRACK");
        drawDiffGaugeCircle(120, 120, 70);
        drawEnvBlock(10, 200);
        force_full_redraw_ = false;
    }

    void drawEnvBlock(int x, int y) {
        if (!force_full_redraw_ &&
            temp_c_ == last_temp_c_ &&
            humidity_pct_ == last_humidity_pct_) {
            return;
        }

        const uint16_t dark_env = tft_.color565(0, 25, 0);
        const int box_w = 220;
        const int box_h = 36;
        tft_.fillRect(x, y, box_w, box_h, dark_env);
        tft_.drawRect(x, y, box_w, box_h, TFT_GREEN);
        tft_.setTextColor(TFT_YELLOW, dark_env);
        tft_.setTextDatum(TL_DATUM);

        tft_.setTextSize(1);
        const int use_font = 1;
        const int pad_x = 8;
        const int pad_y = 10;

        char line[40];
        snprintf(line, sizeof(line), "T: %.1fC  H: %.1f%%", temp_c_, humidity_pct_);
        tft_.drawString(line, x + pad_x, y + pad_y, use_font);

        tft_.setTextDatum(TL_DATUM);
        last_temp_c_ = temp_c_;
        last_humidity_pct_ = humidity_pct_;
    }

    void drawDiffGaugeCircle(int cx, int cy, int r) {
        if (!force_full_redraw_ &&
            diff_h_percent_ == last_diff_h_percent_ &&
            diff_v_percent_ == last_diff_v_percent_ &&
            deadband_percent_ == last_deadband_percent_ &&
            pwm_threshold_percent_ == last_pwm_threshold_percent_) {
            return;
        }

        const uint16_t dark_green = tft_.color565(0, 40, 0);
        const uint16_t dark_blue = tft_.color565(0, 0, 60);
        const uint16_t dark_red = tft_.color565(60, 0, 0);

        const float diff_abs_full = max(fabsf(diff_h_percent_), fabsf(diff_v_percent_));
        const float deadband_for_region = fabsf(deadband_percent_);
        const float pwm_for_region = fabsf(pwm_threshold_percent_);
        const float deadband_th = min(deadband_for_region, pwm_for_region);
        const float pwm_th = max(deadband_for_region, pwm_for_region);

        uint16_t region_bg = dark_red;
        uint16_t region_fg = TFT_RED;
        if (diff_abs_full <= deadband_th) {
            region_bg = dark_green;
            region_fg = TFT_GREEN;
        } else if (diff_abs_full <= pwm_th) {
            region_bg = dark_blue;
            region_fg = TFT_BLUE;
        }

        tft_.fillCircle(cx, cy, r, region_bg);
        tft_.drawCircle(cx, cy, r, TFT_WHITE);

        const float gauge_min = -35.0f;
        const float gauge_max = 35.0f;
        const float gauge_span = gauge_max - gauge_min;

        const int deadband_r = (int)((fabsf(deadband_percent_) / gauge_span) * (2.0f * r));
        const int pwm_r = (int)((fabsf(pwm_threshold_percent_) / gauge_span) * (2.0f * r));

        if (deadband_r > 0) {
            tft_.drawCircle(cx, cy, deadband_r, TFT_GREEN);
        }
        if (pwm_r > 0) {
            tft_.drawCircle(cx, cy, pwm_r, TFT_BLUE);
        }

        // Crosshair
        const uint16_t dark_grey = tft_.color565(40, 40, 40);
        tft_.drawLine(cx - r, cy, cx + r, cy, dark_grey);
        tft_.drawLine(cx, cy - r, cx, cy + r, dark_grey);

        // Marker based on H/V diffs
        float h = constrain(diff_h_percent_, gauge_min, gauge_max);
        float v = constrain(diff_v_percent_, gauge_min, gauge_max);
        const float nx = (h - gauge_min) / gauge_span;
        const float ny = (v - gauge_min) / gauge_span;
        const int px = cx - r + (int)(nx * (2.0f * r));
        const int py = cy + r - (int)(ny * (2.0f * r));
        tft_.fillCircle(px, py, 4, TFT_WHITE);

        // Diff label above gauge
        char diff_label[28];
        snprintf(diff_label, sizeof(diff_label), "DIFF H/V: %.1f %.1f", diff_h_percent_, diff_v_percent_);
        tft_.setTextColor(region_fg, TFT_BLACK);
        tft_.setTextDatum(MC_DATUM);
        tft_.drawString(diff_label, cx, cy - r - 10, 1);
        tft_.setTextDatum(TL_DATUM);

        last_diff_h_percent_ = diff_h_percent_;
        last_diff_v_percent_ = diff_v_percent_;
        last_deadband_percent_ = deadband_percent_;
        last_pwm_threshold_percent_ = pwm_threshold_percent_;
    }

    Config cfg_;
    TFT_eSPI tft_;
    Mode mode_ = Mode::Off;
    bool dirty_ = true;
    bool force_full_redraw_ = true;
    unsigned long last_draw_ms_ = 0;
    float temp_c_ = 0.0f;
    float humidity_pct_ = 0.0f;
    float diff_h_percent_ = 0.0f;
    float diff_v_percent_ = 0.0f;
    float deadband_percent_ = 1.0f;
    float pwm_threshold_percent_ = 10.0f;
    float last_temp_c_ = 9999.0f;
    float last_humidity_pct_ = 9999.0f;
    float last_diff_h_percent_ = 9999.0f;
    float last_diff_v_percent_ = 9999.0f;
    float last_deadband_percent_ = 9999.0f;
    float last_pwm_threshold_percent_ = 9999.0f;
};
