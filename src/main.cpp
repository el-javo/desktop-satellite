#include <Arduino.h>
#include "DHT.h"
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// ===== CONFIGURACIÓN DHT11 =====
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== CONFIGURACIÓN LCD I2C =====
hd44780_I2Cexp lcd; // Objeto LCD I2C
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Inicializar pantalla
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("DHT11 iniciado!");
  Serial.println("DHT11 iniciado. Leyendo datos...");
  delay(2000); // Espera para leer mensaje inicial
}

void loop() {
  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();

  lcd.clear(); // Limpiar pantalla antes de mostrar valores

  if (isnan(humedad) || isnan(temperatura)) {
    lcd.setCursor(0, 0);
    lcd.print("Error lectura");
    Serial.println("Error leyendo DHT11!");
  } else {
    // Mostrar temperatura en la primera fila
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.print(temperatura, 1);
    lcd.print((char)223); // Símbolo °
    lcd.print("C");

    // Mostrar humedad en la segunda fila
    lcd.setCursor(0, 1);
    lcd.print("H: ");
    lcd.print(humedad, 1);
    lcd.print("%");

    // También imprimir en monitor serie
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.print(" °C  ");
    Serial.print("Humedad: ");
    Serial.print(humedad, 1);
    Serial.println(" %");
  }

  delay(2000); // Espera 2 segundos entre lecturas
}
