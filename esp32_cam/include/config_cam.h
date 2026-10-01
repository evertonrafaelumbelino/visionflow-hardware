// VisionFlow — Configurações do firmware do ESP32-CAM (Câmera Inteligente)
// Módulo: ESP32-CAM (AI-Thinker OV2640)

#ifndef CONFIG_CAM_H
#define CONFIG_CAM_H

// --- Definições de Pinos do Módulo ESP32-CAM AI-THINKER ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED interno (opcional)
#define LED_FLASH_GPIO     4

// --- Configurações da Rede Wi-Fi Access Point (AP Autônomo) ---
#define WIFI_AP_SSID       "VisionFlow-CAM-AP"
#define WIFI_AP_PASS       ""   // Rede aberta para conexão automática do aplicativo sem senha
#define WIFI_AP_CHANNEL    1
#define WIFI_AP_MAX_CONN   4

// IP Fixo da Câmera no Modo Access Point
// Endereço de acesso ao stream: http://192.168.4.1:81/stream
#define STREAM_SERVER_PORT 81

// --- Configuração Serial ---
#define SERIAL_BAUD_RATE   115200

#endif // CONFIG_CAM_H
