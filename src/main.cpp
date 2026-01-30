#include <Arduino.h>
#include "DHT.h"

// Pin del DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

// Crear objeto DHT
DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200); // Monitor serie a 115200 baudios
    dht.begin();          // Inicializar sensor
    Serial.println("DHT11 iniciado. Leyendo datos...");
}

void loop() {
    // Leer humedad
    float humedad = dht.readHumidity();
    // Leer temperatura en Celsius
    float temperatura = dht.readTemperature();

    // Comprobar si la lectura falló
    if (isnan(humedad) || isnan(temperatura)) {
        Serial.println("Error leyendo el DHT11!");
    } else {
        Serial.print("Humedad: ");
        Serial.print(humedad);
        Serial.print("%  ");
        Serial.print("Temperatura: ");
        Serial.print(temperatura);
        Serial.println("°C");
    }

    delay(2000); // Esperar 2 segundos entre lecturas
}
