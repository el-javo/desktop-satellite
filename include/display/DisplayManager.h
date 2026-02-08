#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

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
        int pin_sck;
        int pin_mosi;
        int pin_cs; // use -1 if not connected
        unsigned long refresh_interval_ms;
    };

    explicit DisplayManager(const Config& cfg)
        : cfg_(cfg),
          tft_(cfg.pin_cs, cfg.pin_dc, cfg.pin_rst) {}

    void begin() {
        if (cfg_.pin_dc >= 0) {
            pinMode(cfg_.pin_dc, OUTPUT);
        }
        if (cfg_.pin_rst >= 0) {
            pinMode(cfg_.pin_rst, OUTPUT);
            digitalWrite(cfg_.pin_rst, HIGH);
            delay(10);
            digitalWrite(cfg_.pin_rst, LOW);
            delay(50);
            digitalWrite(cfg_.pin_rst, HIGH);
            delay(150);
        }
        if (cfg_.pin_blk >= 0) {
            pinMode(cfg_.pin_blk, OUTPUT);
            digitalWrite(cfg_.pin_blk, LOW);
            delay(5);
            digitalWrite(cfg_.pin_blk, HIGH);
        }

        SPI.begin(cfg_.pin_sck, -1, cfg_.pin_mosi, cfg_.pin_cs);
        tft_.setSPISpeed(27000000);
        tft_.init(240, 240);
        tft_.setRotation(0);
        tft_.fillScreen(ST77XX_BLACK);
        mode_ = Mode::Off;
        last_draw_ms_ = 0;
    }

    void setMode(Mode mode) {
        if (mode_ != mode) {
            mode_ = mode;
            dirty_ = true;
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
        tft_.fillRect(0, 0, 240, 28, ST77XX_BLUE);
        tft_.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
        tft_.setTextSize(2);
        tft_.setCursor(8, 6);
        tft_.print(title);
    }

    void drawOff() {
        tft_.fillScreen(ST77XX_BLACK);
        drawHeader("OFF");
    }

    void drawConnecting() {
        tft_.fillScreen(ST77XX_BLACK);
        drawHeader("CONNECT");
        tft_.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft_.setTextSize(2);
        tft_.setCursor(20, 80);
        tft_.print("Connecting...");
    }

    void drawDashboard() {
        tft_.fillScreen(ST77XX_BLACK);
        drawHeader("DASH");
        drawEnvBlock(10, 40);
    }

    void drawTracking() {
        tft_.fillScreen(ST77XX_BLACK);
        drawHeader("TRACK");
        drawEnvBlock(10, 40);
        drawDiffGauge(10, 120, 220, 40);
    }

    void drawEnvBlock(int x, int y) {
        tft_.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft_.setTextSize(2);
        tft_.setCursor(x, y);
        tft_.print("T: ");
        tft_.print(temp_c_, 1);
        tft_.print(" C");
        tft_.setCursor(x, y + 26);
        tft_.print("H: ");
        tft_.print(humidity_pct_, 1);
        tft_.print(" %");
    }

    void drawDiffGauge(int x, int y, int w, int h) {
        tft_.drawRect(x, y, w, h, ST77XX_WHITE);
        const int center_x = x + w / 2;
        tft_.drawLine(center_x, y, center_x, y + h, ST77XX_WHITE);

        float diff = diff_percent_;
        diff = constrain(diff, -100.0f, 100.0f);
        const int marker_x = center_x + (int)((diff / 100.0f) * (w / 2 - 2));
        tft_.fillRect(x + 1, y + 1, w - 2, h - 2, ST77XX_BLACK);
        tft_.drawLine(marker_x, y + 2, marker_x, y + h - 2, ST77XX_GREEN);

        tft_.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft_.setTextSize(1);
        tft_.setCursor(x, y + h + 6);
        tft_.print("Diff: ");
        tft_.print(diff_percent_, 1);
        tft_.print(" %");
    }

    Config cfg_;
    Adafruit_ST7789 tft_;
    Mode mode_ = Mode::Off;
    bool dirty_ = true;
    unsigned long last_draw_ms_ = 0;
    float temp_c_ = 0.0f;
    float humidity_pct_ = 0.0f;
    float diff_percent_ = 0.0f;
};
