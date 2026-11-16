#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <DHT.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6

#define RELAY_PIN 26
#define SOIL_PIN 34
#define DHT_PIN 15
#define DHT_TYPE DHT22
#define TIMEOUT_MS 90000UL

WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);

bool pumpOn = false;
unsigned long pumpStartedAt = 0;
unsigned long lastClimateRead = 0;
float lastTemp = NAN;
float lastHum = NAN;

void setPump(bool on) {
  pumpOn = on;
  if (on)
    pumpStartedAt = millis();
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

void readClimate() {
  if (millis() - lastClimateRead < 2000 && !isnan(lastTemp))
    return;

  lastTemp = dht.readTemperature();
  lastHum = dht.readHumidity();
  lastClimateRead = millis();
}

bool climateValid() {
  readClimate();
  return !isnan(lastTemp) && !isnan(lastHum);
}

void applyTimeout() {
  if (pumpOn && millis() - pumpStartedAt >= TIMEOUT_MS) {
    setPump(false);
    Serial.println("timeout: rele desligado");
  }
}

String statusJson() {
  applyTimeout();
  int umidade = constrain(map(analogRead(SOIL_PIN), 0, 4095, 0, 100), 0, 100);
  bool valid = climateValid();
  if (!valid && pumpOn) {
    setPump(false);
    Serial.println("fail-safe: rele desligado");
  }

  String json = "{\"umidadeSolo\":";
  json += umidade;
  json += ",\"temperatura\":";
  json += valid ? String(lastTemp, 1) : "0";
  json += ",\"umidadeAr\":";
  json += valid ? String(lastHum, 1) : "0";
  json += ",\"valido\":";
  json += valid ? "true" : "false";
  json += ",\"bombaLigada\":";
  json += pumpOn ? "true" : "false";
  json += "}";
  return json;
}

void sendStatus() {
  String body = statusJson();
  Serial.print("HTTP enviado: ");
  Serial.println(body);
  server.send(200, "application/json", body);
}

void handleCommand() {
  String comando = server.pathArg(0);
  Serial.print("HTTP recebido: GET /comando/");
  Serial.println(comando);

  if (comando == "ligar") {
    if (climateValid()) {
      setPump(true);
    } else {
      setPump(false);
      Serial.println("fail-safe: comando ligar recusado");
    }
  } else if (comando == "desligar") {
    setPump(false);
  } else if (comando == "automatico") {
    Serial.println("modo automatico: a decisao fica no servidor");
  }

  sendStatus();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  setPump(false);
  analogReadResolution(12);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", sendStatus);
  server.on("/status", sendStatus);
  server.on(UriBraces("/comando/{}"), handleCommand);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  applyTimeout();
  delay(2);
}
