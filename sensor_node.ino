#include "Adafruit_BME680.h"
#include "Adafruit_SGP30.h"
#include <Adafruit_Sensor.h>
#include <Wire.h>


// --- PINOS ---
#define PIN_SDA 8
#define PIN_SCL 9

// --- LEDS ---
#define LED_VERMELHO 4 // PERIGO IMEDIATO
#define LED_BRANCO 6   // AR LIMPO (SAFE)
#define LED_AMARELO 7  // AVISO / PRE-ALARME
#define LED_AZUL 10    // ATIVIDADE (HEARTBEAT) & LEITURA

// --- OBJETOS ---
Adafruit_BME680 bme;
Adafruit_SGP30 sgp;

// --- LIMITES (Calibra isto conforme os teus testes) ---
// Nota: Resistência do gás desce quando há poluição
const uint32_t GAS_LIMPO = 100000; // Ar muito limpo
const uint32_t GAS_AVISO = 40000;  // Começa a cheirar a algo (Amarelo)
const uint32_t GAS_PERIGO = 15000; // Fumo detetado (Vermelho)

const uint16_t TVOC_PERIGO = 200; // Limite TVOC

void setup() {
  Serial.begin(115200);

  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_BRANCO, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  // Sequência de Boot Rápida
  digitalWrite(LED_AZUL, HIGH);
  delay(100);
  digitalWrite(LED_AZUL, LOW);
  digitalWrite(LED_AMARELO, HIGH);
  delay(100);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_BRANCO, HIGH);
  delay(100);
  digitalWrite(LED_BRANCO, LOW);
  digitalWrite(LED_VERMELHO, HIGH);
  delay(100);
  digitalWrite(LED_VERMELHO, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);

  // Inicialização (Simplificada para não bloquear se falhar um sensor não
  // crítico)
  if (!bme.begin())
    Serial.println("Aviso: BME680 não detetado.");
  else {
    bme.setGasHeater(320, 150); // 320°C por 150ms
  }

  if (!sgp.begin()) {
    Serial.println("ERRO CRÍTICO: SGP30 não detetado.");
    while (1) {
      digitalWrite(LED_VERMELHO, HIGH);
      delay(100);
      digitalWrite(LED_VERMELHO, LOW);
      delay(100);
    } // Pisca rápido erro
  }

  Serial.println(">>> SISTEMA PRONTO <<<");
}

void loop() {
  // 1. INDICADOR DE LEITURA (Heartbeat)
  // Acende o azul APENAS enquanto está a processar a leitura
  digitalWrite(LED_AZUL, HIGH);

  // Leitura dos sensores
  unsigned long inicioLeitura =
      millis(); // Para cronometrar (opcional, ver no Serial)
  bool bmeSuccess = bme.performReading();
  bool sgpSuccess = sgp.IAQmeasure();

  digitalWrite(LED_AZUL, LOW); // Apaga assim que termina a leitura
  // Se vires o led azul piscar, o sistema não está bloqueado!

  // Se falhar a leitura, sai do loop e tenta de novo
  if (!sgpSuccess)
    return;

  // 2. OBTER DADOS
  uint32_t gasRes =
      bmeSuccess ? bme.gas_resistance : 0; // Se BME falhar, assume 0
  uint16_t tvoc = sgp.TVOC;

  Serial.print("Gás (Ohms): ");
  Serial.print(gasRes);
  Serial.print(" | TVOC: ");
  Serial.println(tvoc);

  // 3. LOGICA DE DECISÃO (Semáforo Inteligente)
  // Apaga todos os LEDs de estado antes de decidir
  digitalWrite(LED_BRANCO, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);

  if (gasRes < GAS_PERIGO || tvoc > TVOC_PERIGO) {
    // --- ESTADO: PERIGO ---
    digitalWrite(LED_VERMELHO, HIGH);
    Serial.println("STATUS: EVACUAR!");

  } else if (gasRes < GAS_AVISO) {
    // --- ESTADO: ATENÇÃO (Pré-Alarme) ---
    // O valor baixou do normal, mas ainda não é crítico
    digitalWrite(LED_AMARELO, HIGH);
    Serial.println("STATUS: Cuidado...");

  } else {
    // --- ESTADO: SEGURO ---
    digitalWrite(LED_BRANCO, HIGH);
  }

  // Pequena pausa entre ciclos
  delay(500);
}