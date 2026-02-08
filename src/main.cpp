#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(0);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.println("TFT_eSPI");
}

void loop() {
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_BLACK);
    delay(500);
}
