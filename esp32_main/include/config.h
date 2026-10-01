// VisionFlow — Configurações centralizadas do firmware do ESP32 Principal
// Altere este arquivo para ajustar pinos, UUIDs e durações sem mexer no main.cpp.

#ifndef CONFIG_H
#define CONFIG_H

// Informações do firmware
#define FIRMWARE_VERSAO  "1.0.0"
#define FIRMWARE_PROJETO "VisionFlow"
#define FIRMWARE_PLACA   "ESP32 WROOM 32"

// Pinos GPIO (ESP32 WROOM 32)
#define PIN_BUZZER       12  // Mini Buzzer Ativo de 5V (Alertas sonoros de emergência)

// Sensor Ultrassônico HC-SR04 (Detecção de relevo/chão)
#define PIN_TRIG_ULTRA   27  // Sinal de disparo do ultrassônico
#define PIN_ECHO_ULTRA   14  // Retorno do pulso do ultrassônico

// Sensor Laser VL53L0X (I2C - Distância frontal)
#define PIN_I2C_SDA      21  // Dados I2C
#define PIN_I2C_SCL      22  // Clock I2C

// Amplificador DAC I2S MAX98357A (Saída para Transdutor de Condução Óssea)
#define PIN_I2S_BCLK     26  // Bit Clock (BCLK)
#define PIN_I2S_LRC      25  // Left/Right Clock (LRC / WS)
#define PIN_I2S_DOUT     33  // Data Out (DIN)

// BLE (Bluetooth Low Energy)
#define BLE_DEVICE_NAME            "VisionFlow-Glasses"
#define SERVICE_UUID_STR           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX_STR "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Parâmetros de Medição e Alertas Sonoros (Curto Alcance)
#define DIST_CRITICA_FRONTAL_M       0.6f   // Menos de 60 cm -> Bipes rápidos de colisão
#define DIST_MEDIANA_FRONTAL_M       1.5f   // Entre 60 cm e 1.5 m -> Bipes lentos
#define DELTA_BURACO_CHAO_CM         15.0f  // Chão afastou mais de 15 cm da baseline -> Buraco/degrau

// Durações e ritmos dos alertas (ms)
#define DURACAO_BIP_INICIAL_MS       100    // Bip curto de sistema ligado
#define INTERVALO_BIP_DESESPERADO_MS 50     // Ritmo urgente para buracos/degraus
#define INTERVALO_BIP_RAPIDO_MS      80     // Ritmo muito rápido para perigo iminente
#define INTERVALO_BIP_LENTO_MS       300    // Ritmo pausado para distância mediana

// Configuração Serial
#define SERIAL_BAUD_RATE 115200

#endif // CONFIG_H
