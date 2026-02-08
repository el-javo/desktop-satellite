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

    void setTrackingInfo(float diff_percent) {
        diff_percent_ = diff_percent;
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
        tft_.setTextColor(TFT_WHITE, TFT_BLUE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 6);
        tft_.print(title);
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
        drawEnvBlock(10, 40);
        drawDiffGauge(10, 120, 220, 40);
        force_full_redraw_ = false;
    }

    void drawEnvBlock(int x, int y) {
        if (!force_full_redraw_ &&
            temp_c_ == last_temp_c_ &&
            humidity_pct_ == last_humidity_pct_) {
            return;
        }

        tft_.fillRect(x, y, 220, 52, TFT_BLACK);
        tft_.setTextColor(TFT_WHITE, TFT_BLACK);
        tft_.setTextSize(2);
        tft_.setCursor(x, y);
        tft_.print("T: ");
        tft_.print(temp_c_, 1);
        tft_.print(" C");
        tft_.setCursor(x, y + 26);
        tft_.print("H: ");
        tft_.print(humidity_pct_, 1);
        tft_.print(" %");
        last_temp_c_ = temp_c_;
        last_humidity_pct_ = humidity_pct_;
    }

    void drawDiffGauge(int x, int y, int w, int h) {
        if (!force_full_redraw_ && diff_percent_ == last_diff_percent_) {
            return;
        }

        tft_.drawRect(x, y, w, h, TFT_WHITE);
        const int center_x = x + w / 2;
        tft_.drawLine(center_x, y, center_x, y + h, TFT_WHITE);

        float diff = diff_percent_;
        diff = constrain(diff, -100.0f, 100.0f);
        const int marker_x = center_x + (int)((diff / 100.0f) * (w / 2 - 2));
        tft_.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_BLACK);
        tft_.drawLine(marker_x, y + 2, marker_x, y + h - 2, TFT_GREEN);

        tft_.fillRect(x, y + h + 6, 220, 12, TFT_BLACK);
        tft_.setTextColor(TFT_WHITE, TFT_BLACK);
        tft_.setTextSize(1);
        tft_.setCursor(x, y + h + 6);
        tft_.print("Diff: ");
        tft_.print(diff_percent_, 1);
        tft_.print(" %");
        last_diff_percent_ = diff_percent_;
    }

    Config cfg_;
    TFT_eSPI tft_;
    Mode mode_ = Mode::Off;
    bool dirty_ = true;
    bool force_full_redraw_ = true;
    unsigned long last_draw_ms_ = 0;
    float temp_c_ = 0.0f;
    float humidity_pct_ = 0.0f;
    float diff_percent_ = 0.0f;
    float last_temp_c_ = 9999.0f;
    float last_humidity_pct_ = 9999.0f;
    float last_diff_percent_ = 9999.0f;
};
