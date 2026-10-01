/*// VisionFlow — Firmware do ESP32 Principal (Cérebro do Óculos)
// Placa: ESP32-32U | Sensores: VL53L0X + HC-SR04 | Áudio: MAX98357A I2S | BLE

#include <Arduino.h>         // Biblioteca base da estrutura Arduino para ESP32
#include <Wire.h>            // Comunicação I2C (usada para ler o sensor laser VL53L0X)
#include <Adafruit_VL53L0X.h> // Biblioteca do sensor de distância laser ToF
#include <BLEDevice.h>       // Biblioteca principal de gerenciamento do Bluetooth Low Energy
#include <BLEServer.h>       // Permite que o ESP32 atue como Servidor BLE
#include <BLEUtils.h>        // Utilitários de protocolo do BLE
#include <driver/i2s.h>      // Driver nativo do ESP32 para transmissão de áudio digital I2S
#include <config.h>          // Arquivo contendo constantes (pinos, deltas, UUIDs)
#include <string>            // Suporte ao tipo de texto std::string do C++
#include <math.h>            // Funções matemáticas (usada para calcular a onda senoidal do áudio)

// INSTÂNCIAS E PONTEIROS GLOBAIS
BLEServer *pServer = NULL;                   // Ponteiro para controlar o servidor Bluetooth
BLECharacteristic *pCharacteristicRX = NULL; // Ponteiro para a característica BLE que recebe dados
bool deviceConnected = false;                // Indica se o celular está conectado via BLE
bool pendingBLEBeep = false;                 // Avisa o loop principal para tocar o bip de conexão
bool isLaserWorking = false;                 // Registra se o sensor laser inicializou corretamente

Adafruit_VL53L0X lox = Adafruit_VL53L0X();   // Objeto de controle do sensor laser VL53L0X

// VARIÁVEIS DE ESTADO DO SISTEMA
float groundBaselineDistance = 0.0f; // Armazena a distância padrão do óculos até o chão (cm)
String pendingObjectFromApp = "";     // Guarda o nome do objeto detectado pela IA do aplicativo

// CONFIGURAÇÃO DO BARRAMENTO DE ÁUDIO I2S (MAX98357A)
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRC,
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// EMISSÃO DE TOM DE CONFIRMAÇÃO (SINTETIZADOR DE ONDA SENOIDAL POR CÓDIGO)
void playBoneConductionBeep(uint16_t freqHz, uint16_t durationMs) {
  int sampleRate = 16000;
  int numSamples = (sampleRate * durationMs) / 1000;
  int16_t buffer[128];
  
  int samplesWritten = 0;
  float phase = 0.0f;
  float phaseInc = (2.0f * M_PI * freqHz) / sampleRate;

  while (samplesWritten < numSamples) {
    int chunk = min(128, numSamples - samplesWritten);
    for (int i = 0; i < chunk; i++) {
      buffer[i] = (int16_t)(sin(phase) * 12000.0f);
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, buffer, chunk * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    samplesWritten += chunk;
  }
}

// EVENTOS DO BLUETOOTH BLE (CALLBACKS DE CONEXÃO E DESCONEXÃO)
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    pendingBLEBeep = true;
    Serial.println(F("[BLE] Dispositivo conectado!"));
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println(F("[BLE] Dispositivo desconectado. Reiniciando anúncio..."));
    pServer->startAdvertising();
  }
};

// EVENTO DE RECEBIMENTO DE DADOS DO APLICATIVO (TEXTO ENVIADO PELA IA)
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String receivedText = String(value.c_str());
      receivedText.trim();
      pendingObjectFromApp = receivedText;
      Serial.print(F("[BLE IA Recebido]: "));
      Serial.println(pendingObjectFromApp);
    }
  }
};

// FUNÇÃO DE LEITURA DO SENSOR ULTRASSÔNICO HC-SR04 (DETECÇÃO DE CHÃO / BURACOS)
float readUltrasonic() {
  digitalWrite(PIN_TRIG_ULTRA, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG_ULTRA, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG_ULTRA, LOW);

  long duration = pulseIn(PIN_ECHO_ULTRA, HIGH, 30000); 
  if (duration == 0) return 999.0f;

  float distanceCm = (duration * 0.0343f) / 2.0f;

  // Filtra ruídos de leituras muito coladas na estrutura ou falhas
  if (distanceCm < 2.0f || distanceCm > 400.0f) {
    return 999.0f;
  }
  
  return distanceCm;
}

// GERENCIADOR DE ALERTA SONORO DO BUZZER (NÃO-BLOQUEANTE USANDO MILLIS)
void triggerBuzzerAlert(int intervalMs) {
  static unsigned long lastBuzzerTime = 0;
  static bool buzzerState = false;

  if (millis() - lastBuzzerTime >= (unsigned long)intervalMs) {
    lastBuzzerTime = millis();
    buzzerState = !buzzerState;
    digitalWrite(PIN_BUZZER, buzzerState ? HIGH : LOW);
  }
}

// CONFIGURAÇÃO INICIAL DO SISTEMA (EXECUTADO UMA ÚNICA VEZ AO LIGAR)
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000); // 1. Tempo de segurança para estabilização da alimentação do ESP32 e periféricos

  Serial.println(F("\n=========================================="));
  Serial.println(F("   VisionFlow — Firmware ESP32 Principal  "));
  Serial.println(F("=========================================="));

  // Define os pinos de hardware como entradas ou saídas
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_TRIG_ULTRA, OUTPUT);
  pinMode(PIN_ECHO_ULTRA, INPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Inicializa o módulo de áudio I2S
  setupI2S();

  // FEEDBACK DE SISTEMA LIGADO
  digitalWrite(PIN_BUZZER, HIGH);
  playBoneConductionBeep(880, DURACAO_BIP_INICIAL_MS);
  digitalWrite(PIN_BUZZER, LOW);

  // 2. INICIALIZAÇÃO DO BARRAMENTO I2C E SENSOR LASER
  // Inicia o barramento especificando os pinos definidos no config.h
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  delay(100); // Pequena pausa para os capacitores da linha I2C estabilizarem

  // Passa o endereço explícito 0x29 descoberto no teste
  if (!lox.begin(0x29)) {
    Serial.println(F("[ERRO GRAVE] Falha ao inicializar sensor Laser VL53L0X!"));
    isLaserWorking = false;
  } else {
    isLaserWorking = true;
    // Configura o sensor para o modo de maior alcance e precisão (Long Range)
    lox.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE);
    Serial.println(F("[VisionFlow] Sensor Laser VL53L0X inicializado com sucesso!"));
  }

  // 3. CALIBRAÇÃO INICIAL DO PISO (HC-SR04)
  float sum = 0.0f;
  int validSamples = 0;
  for (int i = 0; i < 5; i++) {
    float readVal = readUltrasonic();
    if (readVal < 400.0f) {
      sum += readVal;
      validSamples++;
    }
    delay(100);
  }
  
  groundBaselineDistance = (validSamples > 0) ? (sum / validSamples) : 100.0f; 
  Serial.print(F("[VisionFlow] Baseline do chão calibrada em: "));
  Serial.print(groundBaselineDistance);
  Serial.println(F(" cm."));

  // 4. INICIALIZAÇÃO DO BLUETOOTH BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID_STR);
  pCharacteristicRX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX_STR,
      BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID_STR);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println(F("[VisionFlow] BLE ativo. Aguardando conexão do VisionFlow App..."));
}

// LAÇO PRINCIPAL (EXECUTADO CONTINUAMENTE)
void loop() {
  // Processa o som de conexão do Bluetooth
  if (pendingBLEBeep) {
    digitalWrite(PIN_BUZZER, HIGH); delay(80);
    digitalWrite(PIN_BUZZER, LOW);  delay(80);
    digitalWrite(PIN_BUZZER, HIGH); delay(80);
    digitalWrite(PIN_BUZZER, LOW);
    pendingBLEBeep = false;
  }

  // MONITORAMENTO DE CURTO ALCANCE (SENSORIAMENTO EM TEMPO REAL)
  // 1. Leitura da distância de objetos frontais usando o Sensor Laser (em metros)
  float frontalDistanceMeters = 999.0f;
  if (isLaserWorking) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    
    // SOMENTE aceita a leitura se o RangeStatus for 0 (Medição Válida sem Erros)
    // E se a distância medida for maior que 20mm (2cm)
    if (measure.RangeStatus == 0 && measure.RangeMilliMeter > 20) { 
      frontalDistanceMeters = measure.RangeMilliMeter / 1000.0f;
    } else {
      // Qualquer falha de leitura (nada no alcance, ruído, erro de fase) é tratada como sem obstáculo
      frontalDistanceMeters = 999.0f; 
    }
  }

  // 2. Leitura da distância atual do piso usando o Sensor Ultrassônico (em centímetros)
  float currentGroundDistance = readUltrasonic();

  // 3. Algoritmo de detecção de degraus descendentes / buracos
  bool holeDetected = (currentGroundDistance < 400.0f) && 
                      (currentGroundDistance > (groundBaselineDistance + DELTA_BURACO_CHAO_CM));

  // 4. Lógica de decisão dos alertas sonoros emitidos pelo Buzzer
  if (holeDetected) {
    // Caso 1: Risco Iminente de Queda
    triggerBuzzerAlert(INTERVALO_BIP_DESESPERADO_MS);
    Serial.println(F("[ALERTA CRÍTICO] Degrau/Buraco detectado no chão!"));
  } else if (frontalDistanceMeters < DIST_CRITICA_FRONTAL_M) {
    // Caso 2: Colisão Frontal Iminente (< 60cm)
    triggerBuzzerAlert(INTERVALO_BIP_RAPIDO_MS);
    Serial.print(F("[ALERTA CRÍTICO] Colisão iminente a "));
    Serial.print(frontalDistanceMeters, 2);
    Serial.println(F(" m"));
  } else if (frontalDistanceMeters >= DIST_CRITICA_FRONTAL_M && frontalDistanceMeters < DIST_MEDIANA_FRONTAL_M) {
    // Caso 3: Objeto no campo visual mediano (60cm - 1.5m)
    triggerBuzzerAlert(INTERVALO_BIP_LENTO_MS);
  } else {
    // Caso 4: Caminho livre
    digitalWrite(PIN_BUZZER, LOW);
  }

  // PROCESSAMENTO DE MENSAGENS DA INTELIGÊNCIA ARTIFICIAL DO CELULAR
  if (pendingObjectFromApp != "") {
    if (frontalDistanceMeters < 3.0f) {
      String formattedMessage = pendingObjectFromApp + " a " + String(frontalDistanceMeters, 1) + " metros";

      Serial.print(F("[TOMADA DE DECISÃO] Enviando para Condução Óssea (MAX98357A): "));
      Serial.println(formattedMessage);

      playBoneConductionBeep(600, 150);
      delay(50);
      playBoneConductionBeep(1200, 200);
    }

    pendingObjectFromApp = "";
  }

  delay(20); // Ciclo de atualização (~50Hz)
}
*/

// VisionFlow — Firmware de Testes e Diagnóstico de Hardware (ESP32 Principal)
// Foco: Validação de Comunicação, Sensores, Áudio I2S, Buzzer e Bluetooth

#include <Arduino.h>         // Biblioteca base da estrutura Arduino para ESP32
#include <Wire.h>            // Comunicação I2C (VL53L0X)
#include <Adafruit_VL53L0X.h> // Biblioteca do sensor laser ToF
#include <BLEDevice.h>       // Bluetooth Low Energy
#include <BLEServer.h>
#include <BLEUtils.h>
#include <driver/i2s.h>      // Driver de áudio digital I2S
#include <config.h>          // Arquivo contendo constantes e pinos
#include <math.h>

// INSTÂNCIAS GLOBAIS
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
bool isLaserWorking = false;
bool isI2SWorking = true; // Assumido funcional via driver nativo

// CONFIGURAÇÃO DO BARRAMENTO DE ÁUDIO I2S (MAX98357A)
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRC,
    .data_out_num = PIN_I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// EMISSÃO DE TOM DE TESTE NOS TRANSDUTORES DE CONDUÇÃO ÓSSEA
void playTestTone(uint16_t freqHz, uint16_t durationMs) {
  int sampleRate = 16000;
  int numSamples = (sampleRate * durationMs) / 1000;
  int16_t buffer[128];
  
  int samplesWritten = 0;
  float phase = 0.0f;
  float phaseInc = (2.0f * M_PI * freqHz) / sampleRate;

  while (samplesWritten < numSamples) {
    int chunk = min(128, numSamples - samplesWritten);
    for (int i = 0; i < chunk; i++) {
      buffer[i] = (int16_t)(sin(phase) * 12000.0f);
      phase += phaseInc;
      if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, buffer, chunk * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    samplesWritten += chunk;
  }
}

// LEITURA DO SENSOR ULTRASSÔNICO HC-SR04
float readUltrasonic() {
  digitalWrite(PIN_TRIG_ULTRA, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG_ULTRA, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG_ULTRA, LOW);

  long duration = pulseIn(PIN_ECHO_ULTRA, HIGH, 30000); 
  if (duration == 0) return 999.0f;

  float distanceCm = (duration * 0.0343f) / 2.0f;
  if (distanceCm < 2.0f || distanceCm > 400.0f) return 999.0f;
  
  return distanceCm;
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1500);

  Serial.println(F("\n=================================================="));
  Serial.println(F("   VisionFlow — MODO DE DIAGNÓSTICO E TESTES       "));
  Serial.println(F("=================================================="));

  // 1. CONFIGURAÇÃO DE PINOS
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_TRIG_ULTRA, OUTPUT);
  pinMode(PIN_ECHO_ULTRA, INPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println(F("[OK] Pinos digitais básicos configurados."));

  // 2. TESTE DO BUZZER ATIVO
  Serial.println(F("[TESTE] Acionando Buzzer Ativo..."));
  digitalWrite(PIN_BUZZER, HIGH);
  delay(300);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println(F("[OK] Buzzer testado."));

  // 3. TESTE DO ÁUDIO I2S E CONDUÇÃO ÓSSEA
  Serial.println(F("[TESTE] Inicializando driver I2S e testando som (MAX98357A)..."));
  setupI2S();
  playTestTone(440, 250); // Tom de 440Hz por 250ms
  delay(100);
  playTestTone(880, 250); // Tom agudo de 880Hz por 250ms
  Serial.println(F("[OK] Ciclo de áudio I2S concluído. Verifique se ouviu os bips nos transdutores."));

  // 4. TESTE DO BARRAMENTO I2C E SENSOR LASER TOF (VL53L0X)
  Serial.println(F("[TESTE] Inicializando barramento I2C e escaneando sensor VL53L0X..."));
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  delay(100);

  if (!lox.begin(0x29)) {
    Serial.println(F("[FALHA] Sensor Laser VL53L0X NÃO encontrado no endereço I2C 0x29!"));
    isLaserWorking = false;
  } else {
    isLaserWorking = true;
    //lox.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE);
    Serial.println(F("[SUCESSO] Sensor Laser VL53L0X conectado e configurado com sucesso!"));
  }

  // 5. TESTE DO BLUETOOTH BLE
  Serial.println(F("[TESTE] Inicializando pilha BLE..."));
  BLEDevice::init("VisionFlow_TestMode");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService("12345678-1234-1234-1234-1234567890ab");
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
  Serial.println(F("[SUCESSO] BLE inicializado. Anunciando como 'VisionFlow_TestMode'."));

  Serial.println(F("\n=================================================="));
  Serial.println(F(" INICIANDO LOOP DE MONITORAMENTO CONTÍNUO (DEBUG) "));
  Serial.println(F("==================================================\n"));
}

void loop() {
  // 1. Leitura do Sensor Ultrassônico (Chão / Distância Geral)
  float distUltrasonic = readUltrasonic();

  // 2. Leitura do Sensor Laser ToF (Frontal)
  float distLaserMeters = 999.0f;
  int laserStatus = -1;
  if (isLaserWorking) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    laserStatus = measure.RangeStatus;
    if (measure.RangeStatus == 0) {
      distLaserMeters = measure.RangeMilliMeter / 1000.0f;
    }
  }

  // 3. Feedback detalhado no Monitor Serial em tempo real
  Serial.print(F("[DIAGNÓSTICO] "));
  Serial.print(F("Ultrassônico (HC-SR04): "));
  if (distUltrasonic >= 999.0f) {
    Serial.print(F("Fora de alcance / Sem eco"));
  } else {
    Serial.print(distUltrasonic, 1);
    Serial.print(F(" cm"));
  }

  Serial.print(F(" | Laser (VL53L0X): "));
  if (!isLaserWorking) {
    Serial.print(F("SENSOR DESCONECTADO"));
  } else if (distLaserMeters >= 999.0f) {
    Serial.print(F("Sem alvo válido (Status: "));
    Serial.print(laserStatus);
    Serial.print(F(")"));
  } else {
    Serial.print(distLaserMeters, 2);
    Serial.print(F(" m"));
  }

  Serial.println();

  // Pequeno feedback tátil/sonoro opcional a cada ciclo lento de teste
  // (Opcional: pisca o buzzer brevemente se um objeto aproximar a menos de 30cm do laser)
  if (isLaserWorking && distLaserMeters < 0.30f) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(50);
    digitalWrite(PIN_BUZZER, LOW);
  }

  delay(500); // Pausa de meio segundo entre as linhas de log para facilitar a leitura visual
}