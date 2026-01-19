#include "Adafruit_BME680.h"
#include "Adafruit_SGP30.h"
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Wire.h>

// ======================================================
// 1. SELEÇÃO DA PLACA 
// ======================================================
// #define BOARD_S2  // ESP32-S2 (WiFi)
#define BOARD_V3 // Heltec V3 (LoRa SX1262)

// ======================================================
// 2. CONFIGURAÇÃO DE HARDWARE E PINOS
// ======================================================
#ifdef BOARD_S2
#define BOARD_NAME "ESP32-S2"
#include <WiFi.h>
#include <WiFiUdp.h>
#define PIN_SDA 8
#define PIN_SCL 9
#define LED_RED 4
#define LED_WHT 6
#define LED_YLW 7
#define LED_BLU 10
#define SENSOR_WIRE Wire
WiFiUDP udp;

// --- CONFIGURAÇÕES GLOBAIS (Visíveis em todo o código) ---
const char *ssid = "RPi_Gateway_AP";
const char *password = "12345678";
const char *serverIP = "192.168.4.1";
const int serverPort = 5005;

// IPs Estáticos com vírgulas
IPAddress local_IP(192, 168, 4, 100);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

#else
#define BOARD_NAME "HELTEC-V3"
#include <RadioLib.h> // Biblioteca correta para SX1262
#include <U8x8lib.h>
#define PIN_SDA 41
#define PIN_SCL 42
#define LED_RED 1
#define LED_WHT 2
#define LED_YLW 3
#define LED_BLU 4
#define OLED_RST 21

// Definição do Rádio SX1262 (NSS: 8, DIO1: 14, NRST: 12, BUSY: 13)
SX1262 radio = new Module(8, 14, 12, 13);

TwoWire I2C_Bus2 = TwoWire(1);
#define SENSOR_WIRE I2C_Bus2
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(OLED_RST, 18, 17);
#endif

// ======================================================
// 3. PARÂMETROS GERAIS
// ======================================================
const int NODE_ID = 2;                   // 1 for s2, 2 for s3
const unsigned long WARMUP_TIME = 20000; // 3 minutos
const int REQUIRED_CONFIRMATIONS = 3;

Adafruit_BME680 bme;
Adafruit_SGP30 sgp;
Preferences preferences;
int sensorMode = 0;
int fireConfirmCounter = 0;
unsigned long startTime = 0;
bool radioInitialized = false;

void showMsg(String l1, String l2 = "") {
  Serial.println("[" + String(BOARD_NAME) + "] " + l1 + " " + l2);
#ifdef BOARD_V3
  u8x8.clearLine(2);
  u8x8.clearLine(4);
  u8x8.drawString(0, 2, l1.c_str());
  u8x8.drawString(0, 4, l2.c_str());
#endif
}

// ======================================================
// 4. SETUP
// ======================================================
void setup() {
  pinMode(0, INPUT_PULLUP);
  Serial.begin(115200);
  delay(1500);

  startTime = millis();
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_WHT, OUTPUT);
  pinMode(LED_YLW, OUTPUT);
  pinMode(LED_BLU, OUTPUT);

  preferences.begin("saiot", false);
  sensorMode = preferences.getInt("mode", 0);

#ifdef BOARD_V3
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, "SAIOT HUB V3");

  // Inicializar SX1262
  Serial.print(F("[SX1262] Iniciando ... "));
  int state = radio.begin(868.0); // Frequência 868MHz
  if (state == RADIOLIB_ERR_NONE) {
    radio.setSyncWord(0xF3);
    radio.setSpreadingFactor(7);
    radioInitialized = true;
    Serial.println(F("Sucesso!"));
    u8x8.drawString(0, 6, "LORA: OK (868)");
  } else {
    Serial.print(F("Falha, código "));
    Serial.println(state);
    u8x8.drawString(0, 6, "LORA: ERRO!");
  }
#else

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Erro Config Estática");
  }
  WiFi.begin(ssid, password);
#endif

#ifdef BOARD_V3
  SENSOR_WIRE.begin(PIN_SDA, PIN_SCL, 100000);
#else
  SENSOR_WIRE.begin(PIN_SDA, PIN_SCL);
#endif

  if (sensorMode == 0) {
    if (bme.begin(0x77, &SENSOR_WIRE))
      bme.setGasHeater(320, 150);
  } else {
    sgp.begin(&SENSOR_WIRE);
  }
  showMsg("MODO:", sensorMode == 0 ? "BME680" : "SGP30");
}

// ======================================================
// 5. LOOP
// ======================================================
void loop() {
  digitalWrite(LED_BLU, HIGH);

  if (digitalRead(0) == LOW) {
    preferences.putInt("mode", !sensorMode);
    delay(500);
    ESP.restart();
  }

  float val = 0;
  bool success = false;
  if (sensorMode == 0 && bme.performReading()) {
    val = bme.gas_resistance;
    success = true;
  } else if (sensorMode == 1 && sgp.IAQmeasure()) {
    val = sgp.TVOC;
    success = true;
  }

  if (!success) {
    showMsg("ERRO LEITURA");
    delay(5000);
    return;
  }

  // Lógica de Risco
  String risk =
      (sensorMode == 0)
          ? (val < 15000 ? "FIRE-DETECTED"
                         : (val < 30000 ? "POSSIBLE-FIRE" : "NO-FIRE"))
          : (val > 500 ? "FIRE-DETECTED"
                       : (val > 100 ? "POSSIBLE-FIRE" : "NO-FIRE"));

  String finalStatus = "WARMING-UP";
  unsigned long uptime = millis() - startTime;

  if (uptime < WARMUP_TIME) {
    showMsg("AQUECENDO", String((WARMUP_TIME - uptime) / 1000) + "s");
  } else {
    if (risk == "FIRE-DETECTED")
      fireConfirmCounter++;
    else
      fireConfirmCounter = 0;
    if (fireConfirmCounter >= REQUIRED_CONFIRMATIONS)
      finalStatus = "FIRE-DETECTED";
    else if (fireConfirmCounter > 0)
      finalStatus = "POSSIBLE-FIRE";
    else
      finalStatus = "NO-FIRE";
  }

  digitalWrite(LED_WHT, (finalStatus == "NO-FIRE"));
  digitalWrite(LED_YLW, (finalStatus == "POSSIBLE-FIRE"));
  digitalWrite(LED_RED, (finalStatus == "FIRE-DETECTED"));

  String payload =
      "ID:" + String(NODE_ID) + ",LABEL:" + finalStatus + ",VAL:" + String(val);

#ifdef BOARD_V3
  if (radioInitialized) {
    u8x8.drawString(0, 7, "TX..."); // Indica no ecrã que está a enviar
    int state = radio.transmit(payload);
    if (state == RADIOLIB_ERR_NONE) {
      u8x8.drawString(0, 7, "LORA SENT ");
    } else {
      u8x8.drawString(0, 7, "TX ERROR  ");
    }
  }
#else
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BLU, HIGH);
    udp.beginPacket("192.168.4.1", 5005);
    udp.print(payload);
    udp.endPacket();
    Serial.println("[WiFi] Pacote UDP enviado!");
  } else {
    Serial.print("[WiFi] Status: ");
    Serial.println(WiFi.status());

    // FORÇAR RECONEXÃO: Se passar 10 segundos sem ligar, tenta de novo
    static unsigned long lastConnectRetry = 0;
    if (millis() - lastConnectRetry > 10000) {
      Serial.println("A reiniciar tentativa de ligação...");
      WiFi.begin(ssid, password);
      lastConnectRetry = millis();
    }
  }
#endif

  Serial.println("[VERBOSE] " + payload);
  digitalWrite(LED_BLU, LOW);
  delay(1000);
}
