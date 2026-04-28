/***************************************************
  Sistema de Medição de Torque – ESP32-C3  
***************************************************/
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "HX711.h"

/* === HARDWARE === */
const int HX711_DOUT_PIN = 9;
const int HX711_SCK_PIN  = 10;
const int BATTERY_ADC_PIN = 4;

/* === TORQUE === */
HX711 carga;
float torque = 0.0;
float valor = 0.0;
unsigned long lastReadingMillis = 0;
const long interval = 200;

/* === CALIBRAÇÃO GLOBAL === */
float calibration_factor = 10000.0;

/* === SENHA === */
const String CALIB_PASSWORD = "1234";

/* === REDE === */
const char* ssid = "ESP32-Torque";
const char* password = "SenhaSegura";
WebServer server(80);

/* === FILTRO TORQUE === */
const int numReadings = 50;
float readings[numReadings];
int readIndex = 0;
float total = 0;
float average = 0;

/**************** TORQUE FILTER *****************/
void filter_reading(float new_reading) {
  total -= readings[readIndex];
  readings[readIndex] = new_reading;
  total += readings[readIndex];
  readIndex++;
  if (readIndex >= numReadings) readIndex = 0;
  average = total / numReadings;
}

/**************** HX711 READING *****************/
float leitura_carga(HX711& sensor) {
  if (sensor.is_ready()) {
    long reading = sensor.get_units(10);
    valor = reading / calibration_factor;
  }
  return valor;
}

/**************** RAW ADC VOLTAGE ***************/
float readAdcVoltage() {
  long sum = 0;
  for (int i=0; i<20; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delay(1);
  }
  float raw = (float)sum / 20.0;
  float voltage = (raw / 4095.0) * 3.3;   // Tensão NO PINO (0–3.3V)
  return voltage;
}

/********************** SETUP *******************/
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  pinMode(BATTERY_ADC_PIN, INPUT);

  LittleFS.begin();
  WiFi.softAP(ssid, password);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  carga.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  delay(2000);
  carga.set_scale();
  carga.tare();

  for (int i=0; i<numReadings; i++) readings[i] = 0;

  /* === ROTAS === */
  server.on("/", HTTP_GET, [](){
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  });
  server.on("/chart.js", HTTP_GET, [](){
    File f = LittleFS.open("/chart.js", "r");
    server.streamFile(f, "text/javascript");
    f.close();
  });
  server.on("/poli_logo.png", HTTP_GET, [](){
    File f = LittleFS.open("/poli_logo.png", "r");
    server.streamFile(f, "image/png");
    f.close();
  });
  server.on("/pme_logo.png", HTTP_GET, [](){
    File f = LittleFS.open("/pme_logo.png", "r");
    server.streamFile(f, "image/png");
    f.close();
  });

  /* === TORQUE === */
  server.on("/torque", HTTP_GET, [](){
    server.send(200, "text/plain", String(torque, 3));
  });

  /* === RETORNAR FATOR ATUAL === */
  server.on("/get_calibration", HTTP_GET, [](){
    server.send(200, "text/plain", String(calibration_factor, 3));
  });

  /* === DEFINIR CALIBRAÇÃO COM SENHA === */
  server.on("/set_calibration", HTTP_GET, [](){
    if (server.hasArg("value") && server.hasArg("pass")) {
      String pass = server.arg("pass");
      if (pass == CALIB_PASSWORD) {
        calibration_factor = server.arg("value").toFloat();
        Serial.print("Nova calibração: ");
        Serial.println(calibration_factor);
        server.send(200, "text/plain", "OK");
      } else {
        server.send(403, "text/plain", "Senha incorreta");
      }
    } else {
      server.send(400, "text/plain", "Parâmetros faltando");
    }
  });

  /* === BATERIA – ENVIA APENAS A TENSÃO NO PINO === */
  server.on("/battery_raw", HTTP_GET, [](){
    float vAdc = readAdcVoltage();
    server.send(200, "text/plain", String(vAdc, 3));
  });

  server.begin();
}

/*********************** LOOP *******************/
void loop() {
  server.handleClient();

  if (millis() - lastReadingMillis >= interval) {
    lastReadingMillis = millis();

    float med = leitura_carga(carga);
    filter_reading(med);
    torque = med/10.8;

    float adcV = readAdcVoltage();

    Serial.print("Torque: ");
    Serial.print(torque, 3);
    Serial.print(" | Calib: ");
    Serial.print(calibration_factor);
    Serial.print(" | ADC Pin Voltage: ");
    Serial.println(adcV, 3);
  }

  delay(20);
}
