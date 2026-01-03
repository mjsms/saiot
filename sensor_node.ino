#include "Adafruit_BME680.h"
#include "Adafruit_SGP30.h"
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>


// --- CONFIGURAÇÃO DO NÓ ---
const int NODE_ID = 1;            // ID deste sensor (ex: 1, 2, 3...)
const int SENSOR_ANGLE = 45;      // Ângulo onde este sensor está (para o Servo)
const int SLEEP_NORMAL_SEC = 300; // 5 minutos em segundos
const int SLEEP_DANGER_SEC = 30;  // 30 segundos se detetar fogo (modo pânico)

// --- PINOS LORA (ESP32-S2 + RFM95W) ---
#define SCK 36
#define MISO 37
#define MOSI 35
#define SS 10
#define RST 5
#define DIO0 14

// --- PINOS I2C (Confirmar para o teu ESP32-S2) ---
#define PIN_SDA 8
#define PIN_SCL 9

// --- LEDS ---
#define LED_VERMELHO 4 // FOGO DETETADO
#define LED_BRANCO 6   // TUDO OK
#define LED_AMARELO 7  // AVISO
#define LED_AZUL 10    // LORA ENVIADO

// --- OBJETOS ---
Adafruit_BME680 bme;
Adafruit_SGP30 sgp;

// --- LIMITES ---
const uint32_t GAS_AVISO = 40000;
const uint32_t GAS_PERIGO = 15000;
const uint16_t TVOC_PERIGO = 200;

void setup() {
  Serial.begin(115200);
  // while (!Serial); // Comentado para não bloquear se não houver USB no boot

  // 1. Configurar Pinos LED
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_BRANCO, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  // 2. Inicializar Sensores (I2C)
  Wire.begin(PIN_SDA, PIN_SCL);

  if (!bme.begin()) {
    Serial.println("ERRO: BME680 não encontrado!");
  } else {
    bme.setGasHeater(320, 150); // Setup heater
  }

  if (!sgp.begin()) {
    Serial.println("ERRO: SGP30 não encontrado!");
  }

  // 3. Inicializar LoRa (SPI)
  Serial.println("Iniciando LoRa...");
  // SPI.begin(SCK, MISO, MOSI, SS); // Se precisares redefinir SPI default
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(868E6)) {
    Serial.println("ERRO: Falha ao iniciar LoRa!");
    // Se o rádio falhar, não podemos fazer muito, tentamos continuar
  }

  // Configuração LoRa para Floresta (Long Range)
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0xF3); // Palavra de sincronização segredo (0xF3 = isola de
                          // redes default 0x12)
  Serial.println("LoRa Iniciado com sucesso.");

  // --- LEITURA DOS SENSORES ---
  Serial.println("Lendo sensores...");
  // Pequeno delay para estabilizar
  delay(1000);

  // Leitura Real
  bool bmeSuccess = bme.performReading();
  bool sgpSuccess = sgp.IAQmeasure();

  uint32_t gasRes = bmeSuccess ? bme.gas_resistance : 0;
  uint16_t tvoc = sgpSuccess ? sgp.TVOC : 0;

  Serial.print("Gas: ");
  Serial.print(gasRes);
  Serial.print(" | TVOC: ");
  Serial.println(tvoc);

  // --- LÓGICA DE DECISÃO ---
  bool danger = false;

  if (gasRes < GAS_PERIGO || tvoc > TVOC_PERIGO) {
    danger = true;
    digitalWrite(LED_VERMELHO, HIGH);
    Serial.println("PERIGO: Fumo detetado! Enviando Alerta...");

    // Enviar PACOTE LORA
    sendLoRaAlert();

    // Pisca LED Azul para confirmar envio
    digitalWrite(LED_AZUL, HIGH);
    delay(500);
    digitalWrite(LED_AZUL, LOW);

  } else if (gasRes < GAS_AVISO) {
    digitalWrite(LED_AMARELO, HIGH);
    Serial.println("Aviso: Qualidade do ar degradada.");
  } else {
    digitalWrite(LED_BRANCO, HIGH);
    Serial.println("Status: Seguro.");
  }

  // --- PREPARAR DEEP SLEEP ---
  Serial.println("Entrando em Deep Sleep...");

  // Desliga LEDs antes de dormir para poupar bateria
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_BRANCO, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_AZUL, LOW);

  // Define tempo de sono: Pânico (30s) ou Normal (5min)
  uint64_t sleepTime = danger ? SLEEP_DANGER_SEC : SLEEP_NORMAL_SEC;

  // Configura Timer
  esp_sleep_enable_timer_wakeup(sleepTime * 1000000ULL);

  // Assegura que o Rádio dorme (opcional, o LoRa.sleep() ajuda)
  LoRa.sleep();

  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  // Loop vazio - Tudo é feito no setup() para Deep Sleep
}

void sendLoRaAlert() {
  // Formato: "ID:1,ANG:45"
  String packet = "ID:" + String(NODE_ID) + ",ANG:" + String(SENSOR_ANGLE);

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  Serial.print("Pacote enviado: ");
  Serial.println(packet);
}