int motorPin = 2;
int channel = 0;
int freq = 5000;       // Frecuencia PWM
int resolution = 8;    // 8 bits → 0-255

void setup() {
  ledcSetup(channel, freq, resolution);  // Configura PWM
  ledcAttachPin(motorPin, channel);      // Asocia el pin al canal PWM
}

void loop() {
  ledcWrite(channel, 128); // Velocidad 50%
  delay(1000);
  ledcWrite(channel, 0);   // Motor apagado
  delay(1000);
}
