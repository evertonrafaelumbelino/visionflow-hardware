// VisionFlow — Firmware do ESP32-CAM (Câmera Inteligente)
// Placa: ESP32-CAM (AI-Thinker OV2640) | Rede: Wi-Fi Access Point (AP Autônomo) | Streaming: MJPEG HTTP

#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "config_cam.h"

// Boundary único para delimitador do stream multipart MJPEG
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

// --- Handler HTTP para Transmissão de Vídeo MJPEG ---
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  // Desativa cache no cliente HTTP para evitar delays na transmissão em tempo real
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println(F("[ERRO] Falha ao capturar frame da câmera!"));
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (res != ESP_OK) {
      break;
    }

    if (res != ESP_OK) {
      break;
    }
  }

  return res;
}

// --- Inicialização do Servidor HTTP na Porta 81 ---
void startStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = STREAM_SERVER_PORT;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.printf("[ESP32-CAM] Iniciando servidor de streaming HTTP na porta %d...\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println(F("[ESP32-CAM] Servidor HTTP registrado em /stream com sucesso!"));
  } else {
    Serial.println(F("[ERRO] Falha ao iniciar servidor HTTP no ESP32-CAM!"));
  }
}

// --- Inicialização da Câmera OV2640 ---
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Verificação da PSRAM para ajuste dinâmico de taxa e resolução de quadros
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;  // 640x480 para maior clareza visual no celular
    config.jpeg_quality = 10;           // Qualidade alta com compressão rápida
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240 para placas sem PSRAM externa
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicializa o driver da câmera OV2640
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERRO GRAVE] Falha ao inicializar a câmera OV2640 (Erro: 0x%x)\n", err);
    return false;
  }

  // Configurações de ajuste do sensor para ambientes externos/internos
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 1);     // Ligeiro ganho de brilho para acessibilidade
    s->set_contrast(s, 1);       // Contraste elevado para reconhecimento de objetos
    s->set_saturation(s, 0);
  }

  return true;
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println(F("\n=========================================="));
  Serial.println(F("  VisionFlow — Firmware Câmera ESP32-CAM  "));
  Serial.println(F("=========================================="));

  // 1. Inicializa o hardware da câmera OV2640
  if (!initCamera()) {
    Serial.println(F("[FATAL] Reiniciando ESP32-CAM em 5 segundos devido à falha de hardware..."));
    delay(5000);
    ESP.restart();
  }
  Serial.println(F("[ESP32-CAM] Câmera OV2640 pronta."));

  // 2. Configura e ativa a rede Wi-Fi em modo Access Point (AP Autônomo)
  WiFi.mode(WIFI_MODE_AP);
  bool apCreated = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);

  if (apCreated) {
    IPAddress apIP = WiFi.softAPIP();
    Serial.println(F("------------------------------------------"));
    Serial.printf("[Wi-Fi AP] Rede Criada com Sucesso: %s\n", WIFI_AP_SSID);
    Serial.printf("[Wi-Fi AP] Endereço IP Fixo: %s\n", apIP.toString().c_str());
    Serial.printf("[STREAMING] URL de Vídeo para o App: http://%s:%d/stream\n", apIP.toString().c_str(), STREAM_SERVER_PORT);
    Serial.println(F("------------------------------------------"));
  } else {
    Serial.println(F("[ERRO GRAVE] Falha ao criar a rede Wi-Fi Access Point!"));
  }

  // 3. Inicia o Servidor de Stream MJPEG
  startStreamServer();
}

void loop() {
  // O servidor HTTP do ESP32 opera de forma assíncrona/multithread no FreeRTOS
  delay(1000);
}
