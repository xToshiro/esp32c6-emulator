/*
 * Copyright (c) 2026 Jairo Ivo Castro Brito. Todos os direitos reservados.
 * Termos de licença proprietária e confidencial aplicam-se a este código.
 * Consulte LICENSE.md para mais detalhes de propriedade intelectual.
 */
#include "DFRobot_AirQualitySensor.h"
#include "DFRobot_OzoneSensor.h"
#include <Adafruit_BME680.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Multichannel_Gas_GMXXX.h>
#include <NTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Seeed_HM330X.h>
#include <SensirionI2cScd4x.h>
#include <TinyGPS++.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <algorithm>
#include <esp_task_wdt.h>
#include <time.h>
#include <vector>


// --- Segredos e Credenciais Externas ---
#if __has_include("config_secrets.h")
  #include "config_secrets.h"
#elif __has_include("../config_secrets.h")
  #include "../config_secrets.h"
#elif __has_include("config_secrets.example.h")
  #include "config_secrets.example.h"
#elif __has_include("../config_secrets.example.h")
  #include "../config_secrets.example.h"
#endif

#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "SUA_REDE_WIFI"
#endif

#ifndef SECRET_WIFI_PASS
#define SECRET_WIFI_PASS "SUA_SENHA_WIFI"
#endif

#ifndef SECRET_VITAE_API_KEY
#define SECRET_VITAE_API_KEY "mqk_live_placeholder"
#endif

#ifndef SECRET_VITAE_API_URL
#define SECRET_VITAE_API_URL "https://api.exemplo.gov.br/telemetry"
#endif

#ifndef SECRET_VITAE_API_URL_BACKUP
#define SECRET_VITAE_API_URL_BACKUP "https://api-backup.exemplo.gov.br/telemetry"
#endif

#ifndef SECRET_SYSTEM_ID
#define SECRET_SYSTEM_ID "MOQA-INFRA-001"
#endif

// --- Variáveis Configuráveis via Web ---
String ssid = SECRET_WIFI_SSID;
String password = SECRET_WIFI_PASS;
String VITAE_API_KEY = SECRET_VITAE_API_KEY;
String VITAE_API_URL = SECRET_VITAE_API_URL;
String VITAE_API_URL_BACKUP = SECRET_VITAE_API_URL_BACKUP;
String SYSTEM_ID = SECRET_SYSTEM_ID;
String BOARD_ID = "3.2";
String CODE_ID = "4.0.0";
float lat = -3.7442;
float lon = -38.5748;
long gmtOffset_sec = -10800;
int dataCollectionPeriod = 900;
bool ENABLE_VITAE_API = true;

const char *ntpServer = "pool.ntp.org";
const int daylightOffset_sec = 0;

// --- Pinos ESP32-C6 ---
#define PIN_GPS_POWER 6
#define PIN_GPS_RX 18
#define PIN_GPS_TX 1
#define PIN_SD_CS 7
#define PIN_SD_MOSI 20
#define PIN_SD_CLK 19
#define PIN_SD_MISO 14
#define PIN_RGB_LED 8
#define PIN_EXT_LED 15
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define PIN_BTN_CFG 9
#define PIN_SOUND 4
#define PIN_BAT 5

float R1 = 47000.0, R2 = 100000.0, Vref = 3.3, fator_calibracao = 1.258;

Adafruit_NeoPixel rgbLed(1, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
SensirionI2cScd4x scd4x;
Adafruit_BME680 bme;
DFRobot_AirQualitySensor particle(&Wire, 0x19);
HM330X hm3301;
DFRobot_OzoneSensor Ozone;
GAS_GMXXX<TwoWire> multiGas;
WebServer server(80);
TinyGPSPlus gps;

// Instanciação global da Serial do GPS para evitar alocação na pilha da Task do
// FreeRTOS
HardwareSerial SerialGPS(1);

struct Status {
  bool scdStatus = false, bmeStatus = false, pmStatus = false;
  bool ozoneStatus = false, gasStatus = false;
} sysStatus;

struct SensorRecord {
  uint32_t timestamp;
  float pm1, pm25, pm10, t, h, p, co, no2, voc, o3, co2, snd, vBat;
};

#define RAM_BUFFER_SIZE 60 // Guarda ate 60 minutos de registros intermediarios
struct RamRingBuffer {
  SensorRecord buffer[RAM_BUFFER_SIZE];
  int head = 0;
  int count = 0;

  void push(const SensorRecord &rec) {
    buffer[head] = rec;
    head = (head + 1) % RAM_BUFFER_SIZE;
    if (count < RAM_BUFFER_SIZE)
      count++;
  }

  void clear() {
    head = 0;
    count = 0;
  }

  SensorRecord getNewest(int i) const {
    if (count == 0 || i >= count) {
      SensorRecord empty = {0};
      return empty;
    }
    int idx = (head - 1 - i + RAM_BUFFER_SIZE) % RAM_BUFFER_SIZE;
    return buffer[idx];
  }
} ramHistory;

enum BeaconEvent { BEACON_COLLECT, BEACON_SEND_OK, BEACON_SEND_FAIL };

#define QUEUE_SIZE 80 // Armazena ate 20 horas de pacotes consolidados de 15 minutos em RAM
QueueHandle_t dataQueue;
SemaphoreHandle_t netMutex;
uint32_t seqNum = 0, lastSavedDay = 0;
bool isConfigMode = false;
bool hasSD = false;
unsigned long configModeStartTime = 0;

SemaphoreHandle_t ledMutex = NULL;
bool sensorError = false;
bool inWarmup = false;

std::vector<String> triedMACs;
std::vector<String> badMACs;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", 0);

// --- Persistência de Relógio RTC em Falhas e Reinícios Offline ---
RTC_DATA_ATTR time_t rtc_last_known_epoch = 0;
RTC_DATA_ATTR uint32_t rtc_valid_magic = 0;
#define RTC_TIME_MAGIC 0xA55A5AA5

unsigned long lastSuccessfulTimeSync = 0;

int readCount = 0;
float a_pm1 = 0, a_pm25 = 0, a_pm10 = 0, a_t = 0, a_h = 0, a_p = 0, a_co = 0;
float a_no2 = 0, a_voc = 0, a_o3 = 0, a_co2 = 0, a_snd = 0, a_vBat = 0;
float currentBatVoltage = 0.0;

// --- Variáveis para Hardware Alternativo e Energia ---
bool isAlternativeHardware = false;
uint8_t tcaAddress = 0x70;
bool hasMap = false;
uint8_t bmeAddress = 0x77;

int scdChannel = -1;
int bmeChannel = -1;
int pmChannel = -1;
int ozoneChannel = -1;
int gasChannel = -1;

int mcpScdPin = 3;
int mcpBmePin = 4;
int mcpPmPin = 5;
int mcpOzonePin = 2;
int mcpGasPin = 1;

bool isGrovePM = false;

// Economia de Bateria
bool enablePowerSave = true;
float batVoltsFull = 3.50;    // Pleno acima de 3.50V (Parcial abaixo de 3.50V)
float batVoltsPartial = 3.20; // Emergência abaixo de 3.20V
float batVoltsHyst = 0.05;    // Histerese
uint8_t powerSavePartialMask =
    0x05; // PM (Bit 0) + Gas (Bit 2) desativados por padrão
uint8_t powerSaveEmergMask = 0x0D; // PM (Bit 0) + Gas (Bit 2) + Ozone (Bit 3)
                                   // desativados (BME e CO2 ativos)

enum BatteryMode { BAT_MODE_FULL = 0, BAT_MODE_PARTIAL, BAT_MODE_EMERGENCY };
BatteryMode currentBatMode = BAT_MODE_FULL;
uint8_t mcpGpioState = 0xFF;

// --- Protótipos de Funções para C++ ---
float safeVal(float v);
float getBatVoltage();
float readSoundDB();
void blinkBeacon(BeaconEvent event);
void clearSerialScreen();
void setRgbColor(uint8_t r, uint8_t g, uint8_t b);
void updateBaseLedColor();
void logToSD(SensorRecord &rec);
void loadPreferences();
void descobrirPinosMCP();
void setMcpPin(int pin, bool state);
void applyPowerSaveMode(BatteryMode mode);
void updateBatteryPowerSave(float vBat);
void handleRemap();
void handleLightRoot();
void handleGoConfig();
void startMiniWebServer();
void handleLiveBat();
void handleFullRoot();
void handleSave();
void handleExit();
void handleUpdateEnd();
void handleUpdateUpload();
void startFullWebServer();
void handleDownload();
void gpsTask(void *pvParameters);
void cloudSyncTask(void *pvParameters);
void WiFiReconnectTask(void *pvParameters);
void unifiedAcquisitionTask(void *pvParameters);
void initBLE();
bool selectTcaChannel(uint8_t address, uint8_t channel);
void selectSensorChannel(int channel);
SensorRecord averageLastRecords(int n);
String resolveDnsFallback(const String &url, String &hostHeader);
bool isSystemTimeValid();
bool syncNtpTime();

float safeVal(float v) { return isnan(v) ? 9999.0 : v; }

float safeRound2(float v) {
  if (isnan(v) || v >= 9999.0f) return 9999.0f;
  return roundf(v * 100.0f) / 100.0f;
}

int safeHum(float h) {
  if (isnan(h) || h >= 9999.0f) return 0;
  if (h < 0.0f) return 0;
  if (h > 100.0f) return 100;
  return (int)roundf(h);
}

time_t convertUtcToEpoch(int year, int month, int day, int hour, int minute, int second) {
  int y = year;
  int m = month;
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned int yoe = static_cast<unsigned int>(y - era * 400);
  unsigned int doy = (153 * (m - 3) + 2) / 5 + day - 1;
  unsigned int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int days = era * 146097 + static_cast<int>(doe) - 719468;
  return (time_t)days * 86400 + hour * 3600 + minute * 60 + second;
}

bool isSystemTimeValid() {
  time_t now;
  time(&now);
  // Valida se o timestamp e maior que 2025-01-01 (1735689600) e menor que 2036-01-01 (2082758400)
  return (now >= 1735689600 && now <= 2082758400);
}

bool syncNtpTime() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  static unsigned long lastNtpAttempt = 0;
  // Se o relogio ja estiver valido e tentou sincronizar ha menos de 60 segundos, evita flood de UDP
  if (isSystemTimeValid() && (millis() - lastNtpAttempt < 60000) && lastNtpAttempt != 0) {
    return true;
  }
  lastNtpAttempt = millis();

  Serial.println("[NTP] Sincronizando relogio com servidores NTP brasileiros (ntp.br)...");

  // Servidores NTP em ordem de prioridade (NTP.br e Fallbacks globais)
  const char *ntpServers[] = {
    "a.st1.ntp.br",
    "b.st1.ntp.br",
    "c.st1.ntp.br",
    "pool.ntp.br",
    "gps.ntp.br",
    "pool.ntp.org",
    "time.google.com"
  };
  const int numServers = sizeof(ntpServers) / sizeof(ntpServers[0]);

  // 1. Configura SNTP nativo do ESP-IDF com os 3 principais servidores brasileiros
  configTime(gmtOffset_sec, daylightOffset_sec, "a.st1.ntp.br", "b.st1.ntp.br", "pool.ntp.org");

  // 2. Consulta direta via NTPClient para garantir ajuste imediato do relogio do sistema (settimeofday)
  for (int i = 0; i < numServers; i++) {
    timeClient.setPoolServerName(ntpServers[i]);
    timeClient.begin();
    for (int attempt = 0; attempt < 3; attempt++) {
      if (timeClient.forceUpdate()) {
        unsigned long epoch = timeClient.getEpochTime();
        if (epoch >= 1735689600 && epoch <= 2082758400) {
          struct timeval tv = {.tv_sec = (time_t)epoch, .tv_usec = 0};
          settimeofday(&tv, NULL);
          timeClient.end();

          lastSuccessfulTimeSync = millis();
          rtc_last_known_epoch = (time_t)epoch;
          rtc_valid_magic = RTC_TIME_MAGIC;

          time_t now;
          time(&now);
          struct tm timeinfo;
          localtime_r(&now, &timeinfo);
          Serial.printf("[NTP] Relogio ajustado com sucesso via %s! Epoch: %lu | Data/Hora: %04d-%02d-%02d %02d:%02d:%02d\n",
                        ntpServers[i], epoch,
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
          return true;
        }
      }
      delay(150);
    }
    timeClient.end();
  }

  // 3. Fallback: Aguarda ate 3 segundos caso o SNTP nativo em background finalize
  for (int s = 0; s < 15; s++) {
    if (isSystemTimeValid()) {
      time_t now;
      time(&now);
      lastSuccessfulTimeSync = millis();
      rtc_last_known_epoch = now;
      rtc_valid_magic = RTC_TIME_MAGIC;
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      Serial.printf("[NTP] SNTP nativo sincronizou com sucesso! Epoch: %ld (%04d-%02d-%02d %02d:%02d:%02d)\n",
                    (long)now, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return true;
    }
    delay(200);
  }

  if (isSystemTimeValid()) {
    time_t now;
    time(&now);
    Serial.printf("[NTP] Servidores NTP inalcancaveis no momento (sem internet externa). Mantendo relogio interno RTC autonomo: Epoch %ld\n", (long)now);
    return true;
  }

  Serial.println("[NTP] Aviso: Falha ao sincronizar em todos os servidores NTP.");
  return false;
}

String resolveDnsFallback(const String &url, String &hostHeader) {
  String host = "";
  int startIdx = url.indexOf("://");
  if (startIdx != -1) {
    int endIdx = url.indexOf("/", startIdx + 3);
    if (endIdx != -1) {
      host = url.substring(startIdx + 3, endIdx);
    } else {
      host = url.substring(startIdx + 3);
    }
  }

  hostHeader = host;
  
  if (host.length() == 0) {
    return url;
  }

  // Forca o uso exclusivo dos servidores DNS publicos Google (8.8.8.8) e Cloudflare (1.1.1.1)
  IPAddress ip = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();
  WiFi.config(ip, gateway, subnet, IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));

  IPAddress targetIp;
  if (WiFi.hostByName(host.c_str(), targetIp) && targetIp.toString() != "0.0.0.0") {
    Serial.printf("[API] DNS resolvido via Google/Cloudflare (8.8.8.8 / 1.1.1.1): %s -> %s\n", host.c_str(), targetIp.toString().c_str());
  } else {
    Serial.printf("[API] Aviso: Falha ao resolver %s via DNS Publico. Mantendo URL para envio direto.\n", host.c_str());
  }

  return url;
}

bool selectTcaChannel(uint8_t address, uint8_t channel) {
  if (channel > 7)
    return false;
  Wire.beginTransmission(address);
  Wire.write(1 << channel);
  return (Wire.endTransmission() == 0);
}

void selectSensorChannel(int channel) {
  // Com o hardware alternativo, mantemos todos os canais habilitados (0xFF)
  // permanentemente no setup para evitar ligamentos/desligamentos dinâmicos.
}

SensorRecord averageLastRecords(int n) {
  SensorRecord avg = {0};
  if (n <= 0)
    return avg;

  float sum_pm1 = 0, sum_pm25 = 0, sum_pm10 = 0, sum_t = 0, sum_h = 0,
        sum_p = 0;
  float sum_co = 0, sum_no2 = 0, sum_voc = 0, sum_o3 = 0, sum_co2 = 0,
        sum_snd = 0, sum_vBat = 0;

  int count_pm1 = 0, count_pm25 = 0, count_pm10 = 0, count_t = 0, count_h = 0,
      count_p = 0;
  int count_co = 0, count_no2 = 0, count_voc = 0, count_o3 = 0, count_co2 = 0,
      count_snd = 0, count_vBat = 0;

  SensorRecord newest = ramHistory.getNewest(0);
  avg.timestamp = newest.timestamp;

  for (int i = 0; i < n; i++) {
    SensorRecord r = ramHistory.getNewest(i);
    if (r.timestamp > 0) {
      if (r.pm1 != 9999.0) {
        sum_pm1 += r.pm1;
        count_pm1++;
      }
      if (r.pm25 != 9999.0) {
        sum_pm25 += r.pm25;
        count_pm25++;
      }
      if (r.pm10 != 9999.0) {
        sum_pm10 += r.pm10;
        count_pm10++;
      }
      if (r.t != 9999.0) {
        sum_t += r.t;
        count_t++;
      }
      if (r.h != 9999.0) {
        sum_h += r.h;
        count_h++;
      }
      if (r.p != 9999.0) {
        sum_p += r.p;
        count_p++;
      }
      if (r.co != 9999.0) {
        sum_co += r.co;
        count_co++;
      }
      if (r.no2 != 9999.0) {
        sum_no2 += r.no2;
        count_no2++;
      }
      if (r.voc != 9999.0) {
        sum_voc += r.voc;
        count_voc++;
      }
      if (r.o3 != 9999.0) {
        sum_o3 += r.o3;
        count_o3++;
      }
      if (r.co2 != 9999.0) {
        sum_co2 += r.co2;
        count_co2++;
      }
      if (r.snd != 9999.0) {
        sum_snd += r.snd;
        count_snd++;
      }
      if (r.vBat != 9999.0) {
        sum_vBat += r.vBat;
        count_vBat++;
      }
    }
  }

  avg.pm1 = (count_pm1 > 0) ? (sum_pm1 / count_pm1) : 9999.0;
  avg.pm25 = (count_pm25 > 0) ? (sum_pm25 / count_pm25) : 9999.0;
  avg.pm10 = (count_pm10 > 0) ? (sum_pm10 / count_pm10) : 9999.0;
  avg.t = (count_t > 0) ? (sum_t / count_t) : 9999.0;
  avg.h = (count_h > 0) ? (sum_h / count_h) : 9999.0;
  avg.p = (count_p > 0) ? (sum_p / count_p) : 9999.0;
  avg.co = (count_co > 0) ? (sum_co / count_co) : 9999.0;
  avg.no2 = (count_no2 > 0) ? (sum_no2 / count_no2) : 9999.0;
  avg.voc = (count_voc > 0) ? (sum_voc / count_voc) : 9999.0;
  avg.o3 = (count_o3 > 0) ? (sum_o3 / count_o3) : 9999.0;
  avg.co2 = (count_co2 > 0) ? (sum_co2 / count_co2) : 9999.0;
  avg.snd = (count_snd > 0) ? (sum_snd / count_snd) : 9999.0;
  avg.vBat = (count_vBat > 0) ? (sum_vBat / count_vBat) : 9999.0;

  return avg;
}

float getBatVoltage() {
  return (analogRead(PIN_BAT) / 4095.0) * Vref * ((R1 + R2) / R2) *
         fator_calibracao;
}

float readSoundDB() {
  unsigned int sMax = 0, sMin = 4095;
  unsigned long start = millis();
  while (millis() - start < 50) {
    int s = analogRead(PIN_SOUND);
    if (s < 4095) {
      if (s > sMax)
        sMax = s;
      else if (s < sMin)
        sMin = s;
    }
  }
  float v = ((sMax - sMin) * 3.3) / 4095.0;
  return (v <= 0) ? 30.0 : (20.0 * log10(v / 0.001) + 15.0);
}

void blinkBeacon(BeaconEvent event) {
  int times = 0, delayMs = 0;
  switch (event) {
  case BEACON_COLLECT:
    times = 1;
    delayMs = 50;
    break;
  case BEACON_SEND_OK:
    times = 2;
    delayMs = 200;
    break;
  case BEACON_SEND_FAIL:
    times = 4;
    delayMs = 100;
    break;
  }
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_EXT_LED, HIGH);
    vTaskDelay(pdMS_TO_TICKS(delayMs));
    digitalWrite(PIN_EXT_LED, LOW);
    if (i < times - 1)
      vTaskDelay(pdMS_TO_TICKS(delayMs));
  }
}

void clearSerialScreen() {
  Serial.write(27);
  Serial.print("[2J");
  Serial.write(27);
  Serial.print("[H");
}

void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  if (ledMutex != NULL) {
    if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(100)) == pdPASS) {
      rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
      rgbLed.show();
      xSemaphoreGive(ledMutex);
    }
  } else {
    rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
    rgbLed.show();
  }
}

void updateBaseLedColor() {
  if (isConfigMode) {
    setRgbColor(50, 30, 0); // Amarelo Suave (Configuração AP)
  } else if (inWarmup) {
    setRgbColor(0, 0, 50); // Azul Suave (Warmup)
  } else if (sensorError) {
    setRgbColor(80, 0, 0); // Vermelho Suave (Falha de Sensor)
  } else if (WiFi.status() != WL_CONNECTED) {
    setRgbColor(50, 12, 0); // Laranja Suave (Conectando/Sem WiFi)
  } else {
    setRgbColor(0, 0, 50); // Azul Suave (Normal / Conectado - Verde reservado exclusivamente para envio OK)
  }
}

void logToSD(SensorRecord &rec) {
  if (!hasSD)
    return;
  File file = SD.open("/datalog.csv", FILE_APPEND);
  if (!file)
    return;

  file.printf(
      "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
      rec.timestamp, rec.pm1, rec.pm25, rec.pm10, rec.t, rec.h, rec.p, rec.o3,
      rec.no2, rec.voc, rec.co2, rec.snd, rec.vBat);
  file.close();
}

uint8_t getAlwaysOnMask() {
  uint8_t mask = 0;
  if (mcpPmPin == -1 && pmChannel >= 0 && pmChannel <= 7)
    mask |= (1 << pmChannel);
  if (mcpBmePin == -1 && bmeChannel >= 0 && bmeChannel <= 7)
    mask |= (1 << bmeChannel);
  if (mcpGasPin == -1 && gasChannel >= 0 && gasChannel <= 7)
    mask |= (1 << gasChannel);
  if (mcpOzonePin == -1 && ozoneChannel >= 0 && ozoneChannel <= 7)
    mask |= (1 << ozoneChannel);
  return mask;
}

void setMcpPin(int pin, bool state) {
  if (pin < 0 || pin > 7)
    return;
  if (state) {
    mcpGpioState |= (1 << pin);
  } else {
    mcpGpioState &= ~(1 << pin);
  }

  // Garante que pinos de sensores Always-On (onde MCP correspondente é o canal
  // TCA) permaneçam sempre em nível HIGH
  mcpGpioState |= getAlwaysOnMask();

  Wire.beginTransmission(0x20); // MCP_ADDR = 0x20
  Wire.write(0x09);             // registrador GPIO
  Wire.write(mcpGpioState);
  Wire.endTransmission();
}

void applyPowerSaveMode(BatteryMode mode) {
  uint8_t disableMask = 0;
  if (mode == BAT_MODE_PARTIAL) {
    disableMask = powerSavePartialMask;
  } else if (mode == BAT_MODE_EMERGENCY) {
    disableMask = powerSaveEmergMask;
  }

  // --- PM Sensor (Bit 0, mask 0x01) ---
  if (disableMask & 0x01) {
    if (sysStatus.pmStatus) {
      setMcpPin(mcpPmPin, false);
      sysStatus.pmStatus = false;
      Serial.println("[ENERGIA] Sensor PM desligado (Power Save).");
    }
  } else {
    if (!sysStatus.pmStatus && pmChannel != -1) {
      setMcpPin(mcpPmPin, true);
      vTaskDelay(pdMS_TO_TICKS(500));
      selectSensorChannel(pmChannel);
      if (isGrovePM) {
        if (hm3301.init() == NO_ERROR) {
          sysStatus.pmStatus = true;
          Serial.println(
              "[ENERGIA] Sensor PM Grove HM3301 religado e inicializado.");
        }
      } else {
        if (particle.begin()) {
          sysStatus.pmStatus = true;
          Serial.println(
              "[ENERGIA] Sensor PM DFRobot religado e inicializado.");
        }
      }
    }
  }

  // --- BME680 (Bit 1, mask 0x02) ---
  if (disableMask & 0x02) {
    if (sysStatus.bmeStatus) {
      setMcpPin(mcpBmePin, false);
      sysStatus.bmeStatus = false;
      Serial.println("[ENERGIA] Sensor BME680 desligado (Power Save).");
    }
  } else {
    if (!sysStatus.bmeStatus && bmeChannel != -1) {
      setMcpPin(mcpBmePin, true);
      vTaskDelay(pdMS_TO_TICKS(500));
      selectSensorChannel(bmeChannel);
      if (bme.begin(bmeAddress)) {
        sysStatus.bmeStatus = true;
        bme.setGasHeater(320, 150);
        Serial.printf(
            "[ENERGIA] Sensor BME680/688 (0x%02X) religado e inicializado.\n",
            bmeAddress);
      }
    }
  }

  // --- MultiGas (Bit 2, mask 0x04) ---
  if (disableMask & 0x04) {
    if (sysStatus.gasStatus) {
      setMcpPin(mcpGasPin, false);
      sysStatus.gasStatus = false;
      Serial.println("[ENERGIA] Sensor GMXXX desligado (Power Save).");
    }
  } else {
    if (!sysStatus.gasStatus && gasChannel != -1) {
      setMcpPin(mcpGasPin, true);
      vTaskDelay(pdMS_TO_TICKS(500));
      selectSensorChannel(gasChannel);
      multiGas.begin(Wire, 0x08);
      sysStatus.gasStatus = true;
      Serial.println("[ENERGIA] Sensor GMXXX religado e inicializado.");
    }
  }

  // --- Ozone (Bit 3, mask 0x08) ---
  if (disableMask & 0x08) {
    if (sysStatus.ozoneStatus) {
      setMcpPin(mcpOzonePin, false);
      sysStatus.ozoneStatus = false;
      Serial.println("[ENERGIA] Sensor Ozonio desligado (Power Save).");
    }
  } else {
    if (!sysStatus.ozoneStatus && ozoneChannel != -1) {
      setMcpPin(mcpOzonePin, true);
      vTaskDelay(pdMS_TO_TICKS(500));
      selectSensorChannel(ozoneChannel);
      if (Ozone.begin(OZONE_ADDRESS_3)) {
        sysStatus.ozoneStatus = true;
        Serial.println("[ENERGIA] Sensor Ozonio religado e inicializado.");
      }
    }
  }
}

void updateBatteryPowerSave(float vBat) {
  // Se a tensão da bateria for inferior a 2.5V, assume-se falha/ausência de
  // bateria com alimentação via painel solar direta. Nesse caso, ignora-se a
  // economia de energia.
  if (vBat < 2.5) {
    if (currentBatMode != BAT_MODE_FULL) {
      Serial.printf(
          "[ENERGIA] Tensao extremamente baixa (%.2fV < 2.5V) - Falha de "
          "bateria detectada (Operando via Solar). Religando tudo.\n",
          vBat);
      applyPowerSaveMode(BAT_MODE_FULL);
      currentBatMode = BAT_MODE_FULL;
    }
    return;
  }

  if (!enablePowerSave) {
    if (currentBatMode != BAT_MODE_FULL) {
      Serial.println("[ENERGIA] Controle de energia desativado. Religando "
                     "todos os sensores.");
      applyPowerSaveMode(BAT_MODE_FULL);
      currentBatMode = BAT_MODE_FULL;
    }
    return;
  }

  BatteryMode nextMode = currentBatMode;

  switch (currentBatMode) {
  case BAT_MODE_FULL:
    if (vBat < batVoltsPartial) {
      nextMode = BAT_MODE_EMERGENCY;
    } else if (vBat < batVoltsFull) {
      nextMode = BAT_MODE_PARTIAL;
    }
    break;

  case BAT_MODE_PARTIAL:
    if (vBat < batVoltsPartial) {
      nextMode = BAT_MODE_EMERGENCY;
    } else if (vBat > (batVoltsFull + batVoltsHyst)) {
      nextMode = BAT_MODE_FULL;
    }
    break;

  case BAT_MODE_EMERGENCY:
    if (vBat > (batVoltsFull + batVoltsHyst)) {
      nextMode = BAT_MODE_FULL;
    } else if (vBat > (batVoltsPartial + batVoltsHyst)) {
      nextMode = BAT_MODE_PARTIAL;
    }
    break;
  }

  if (nextMode != currentBatMode) {
    Serial.printf("[ENERGIA] Transicao de estado de bateria: %d -> %d (Tensao "
                  "atual: %.2fV)\n",
                  currentBatMode, nextMode, vBat);
    currentBatMode = nextMode;
    applyPowerSaveMode(currentBatMode);
  }
}

void descobrirPinosMCP() {
  Serial.println(
      "\n--- INICIANDO AUTO-MAPEAMENTO DE PINOS DO MCP23008 E CANAIS TCA ---");
  hasMap = false;

  pmChannel = bmeChannel = gasChannel = ozoneChannel = scdChannel = -1;
  mcpPmPin = mcpBmePin = mcpGasPin = mcpOzonePin = mcpScdPin = -1;

  // 1. Identificar o canal do SCD41 (CO2)
  Wire.beginTransmission(tcaAddress);
  Wire.write(1 << 0); // Seleciona canal 0 para falar com MCP
  Wire.endTransmission();

  Wire.beginTransmission(0x20); // MCP
  Wire.write(0x00);             // IODIR
  Wire.write(0x00);             // todas saídas
  Wire.endTransmission();

  Wire.beginTransmission(0x20);
  Wire.write(0x09); // GPIO
  Wire.write(0x00); // tudo LOW
  Wire.endTransmission();
  delay(100);

  for (uint8_t ch = 1; ch < 8; ch++) {
    Wire.beginTransmission(tcaAddress);
    Wire.write(1 << ch);
    Wire.endTransmission();

    Wire.beginTransmission(0x62);
    if (Wire.endTransmission() == 0) {
      scdChannel = ch;
      mcpScdPin = ch; // Correspondencia 1-para-1
      Serial.printf("[MAPEAMENTO] SCD41 (CO2) localizado no canal TCA: %d (MCP "
                    "Pino: %d)\n",
                    ch, mcpScdPin);
      break;
    }
  }

  // 1.5. Detectar sensores sempre alimentados (Always-On/Defeito) com MCP em
  // LOW
  bool pmAlwaysOn = false, bmeAlwaysOn = false, gasAlwaysOn = false,
       ozoneAlwaysOn = false;
  Serial.println(
      "[MAPEAMENTO] Verificando sensores sempre ativos (Always-On)...");
  for (uint8_t ch = 1; ch < 8; ch++) {
    Wire.beginTransmission(tcaAddress);
    Wire.write(1 << ch);
    Wire.endTransmission();

    for (byte addr = 1; addr < 127; addr++) {
      if (addr == 0x70 || addr == 0x20 || addr == 0x62)
        continue; // Ignora MUX, MCP e SCD41

      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        if (addr == 0x08) {
          gasChannel = ch;
          mcpGasPin = -1;
          gasAlwaysOn = true;
          Serial.printf("[MAPEAMENTO] GMXXX (Multigas) detectado como "
                        "Always-On no canal TCA: %d\n",
                        ch);
        } else if (addr == 0x73 || addr == 0x03) {
          ozoneChannel = ch;
          mcpOzonePin = -1;
          ozoneAlwaysOn = true;
          Serial.printf("[MAPEAMENTO] O3 (Ozonio) detectado como Always-On no "
                        "canal TCA: %d\n",
                        ch);
        } else if (addr == 0x77 || addr == 0x76) {
          bmeAddress = addr;
          bmeChannel = ch;
          mcpBmePin = -1;
          bmeAlwaysOn = true;
          Serial.printf("[MAPEAMENTO] BME680/688 (0x%02X) detectado como "
                        "Always-On no canal TCA: %d\n",
                        bmeAddress, ch);
        } else if (addr == 0x19 || addr == 0x40) {
          pmChannel = ch;
          mcpPmPin = -1;
          isGrovePM = (addr == 0x40);
          pmAlwaysOn = true;
          Serial.printf("[MAPEAMENTO] PM (%s) detectado como Always-On no "
                        "canal TCA: %d\n",
                        isGrovePM ? "Grove HM3301" : "DFRobot", ch);
        }
      }
    }
  }

  // 2. Varredura sequencial das portas do MCP (0 a 7)
  for (int p = 0; p < 8; p++) {
    Serial.printf("[MAPEAMENTO] Testando porta %d do MCP23008...\n", p);

    Wire.beginTransmission(tcaAddress);
    Wire.write(1 << 0);
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write(0x09); // GPIO
    Wire.write(1 << p);
    Wire.endTransmission();

    // Atraso de 5 segundos para estabilizacao alimentando o watchdog
    for (int d = 0; d < 50; d++) {
      esp_task_wdt_reset();
      delay(100);
    }

    for (uint8_t ch = 1; ch < 8; ch++) {
      Wire.beginTransmission(tcaAddress);
      Wire.write(1 << ch);
      Wire.endTransmission();

      for (byte addr = 1; addr < 127; addr++) {
        if (addr == 0x70 || addr == 0x20 || addr == 0x62)
          continue; // Ignora MUX, MCP e SCD41

        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
          if (addr == 0x08 && !gasAlwaysOn) {
            gasChannel = ch;
            mcpGasPin = p;
            Serial.printf("[MAPEAMENTO] Sensor GMXXX (Multigas) detectado! "
                          "Canal TCA: %d, Pino MCP: %d\n",
                          ch, p);
          } else if ((addr == 0x73 || addr == 0x03) && !ozoneAlwaysOn) {
            ozoneChannel = ch;
            mcpOzonePin = p;
            Serial.printf("[MAPEAMENTO] Sensor O3 (Ozonio) detectado! Canal "
                          "TCA: %d, Pino MCP: %d\n",
                          ch, p);
          } else if ((addr == 0x77 || addr == 0x76) && !bmeAlwaysOn) {
            bmeAddress = addr;
            bmeChannel = ch;
            mcpBmePin = p;
            Serial.printf("[MAPEAMENTO] Sensor BME680/688 (0x%02X) detectado! "
                          "Canal TCA: %d, Pino MCP: %d\n",
                          bmeAddress, ch, p);
          } else if ((addr == 0x19 || addr == 0x40) && !pmAlwaysOn) {
            pmChannel = ch;
            mcpPmPin = p;
            isGrovePM = (addr == 0x40);
            Serial.printf("[MAPEAMENTO] Sensor PM (%s) detectado! Canal TCA: "
                          "%d, Pino MCP: %d\n",
                          isGrovePM ? "Grove HM3301" : "DFRobot", ch, p);
          }
        }
      }
    }
  }

  if (pmChannel != -1 && bmeChannel != -1) {
    hasMap = true;
    Serial.println(
        "[MAPEAMENTO] Mapeamento concluido com sucesso! Gravando em NVS...");
    Preferences localPrefs;
    localPrefs.begin("moqa_cfg", false);
    localPrefs.putBool("has_map", true);
    localPrefs.putInt("tca_pm", pmChannel);
    localPrefs.putInt("tca_bme", bmeChannel);
    localPrefs.putInt("tca_gas", gasChannel);
    localPrefs.putInt("tca_o3", ozoneChannel);
    localPrefs.putInt("tca_scd", scdChannel);

    localPrefs.putInt("mcp_pm", mcpPmPin);
    localPrefs.putInt("mcp_bme", mcpBmePin);
    localPrefs.putInt("mcp_gas", mcpGasPin);
    localPrefs.putInt("mcp_o3", mcpOzonePin);
    localPrefs.putInt("mcp_scd", mcpScdPin);

    localPrefs.putBool("is_grove", isGrovePM);
    localPrefs.putUChar("bme_addr", bmeAddress);
    localPrefs.end();
  } else {
    Serial.println("[AVISO] Falha ao mapear todos os sensores essenciais.");
  }

  // Energiza tudo e conecta barramento para modo Coleta
  Wire.beginTransmission(tcaAddress);
  Wire.write(1 << 0);
  Wire.endTransmission();

  Wire.beginTransmission(0x20);
  Wire.write(0x09);
  Wire.write(0xFF);
  Wire.endTransmission();
  mcpGpioState = 0xFF;

  Wire.beginTransmission(tcaAddress);
  Wire.write(0xFF); // Todos canais ligados
  Wire.endTransmission();
}

void handleRemap() {
  server.send(200, "text/html",
              "<h2>Iniciando novo mapeamento de hardware... O dispositivo ira "
              "reiniciar e varrer as portas.</h2>");
  delay(1000);
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  localPrefs.putBool("has_map", false);
  localPrefs.end();
  ESP.restart();
}

void loadPreferences() {
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  seqNum = localPrefs.getUInt("seq", 0);
  lastSavedDay = localPrefs.getUInt("day", 0);

  ssid = localPrefs.getString("ssid", ssid);
  password = localPrefs.getString("pass", password);
  SYSTEM_ID = localPrefs.getString("sysid", SYSTEM_ID);
  BOARD_ID = localPrefs.getString("brdid", BOARD_ID);
  // Forca gravacao da versao v4.0.0 no NVS para sobrescrever versoes legadas (2.0.0, 3.0.0, 3.5.0) apos update OTA
  localPrefs.putString("codid", CODE_ID);
  VITAE_API_KEY = localPrefs.getString("api", VITAE_API_KEY);
  VITAE_API_URL = localPrefs.getString("api_url", VITAE_API_URL);
  VITAE_API_URL_BACKUP = localPrefs.getString("api_url_bkp", VITAE_API_URL_BACKUP);

  lat = localPrefs.getFloat("lat", lat);
  if (isnan(lat) || lat < -90.0f || lat > 90.0f) lat = -3.7442f;

  lon = localPrefs.getFloat("lon", lon);
  if (isnan(lon) || lon < -180.0f || lon > 180.0f) lon = -38.5748f;

  gmtOffset_sec = localPrefs.getLong("gmt", gmtOffset_sec);
  dataCollectionPeriod = localPrefs.getInt("colper", dataCollectionPeriod);

  R1 = localPrefs.getFloat("r1", R1);
  R2 = localPrefs.getFloat("r2", R2);
  Vref = localPrefs.getFloat("vref", Vref);
  fator_calibracao = localPrefs.getFloat("vcal", fator_calibracao);

  hasMap = localPrefs.getBool("has_map", false);
  pmChannel = localPrefs.getInt("tca_pm", pmChannel);
  bmeChannel = localPrefs.getInt("tca_bme", bmeChannel);
  gasChannel = localPrefs.getInt("tca_gas", gasChannel);
  ozoneChannel = localPrefs.getInt("tca_o3", ozoneChannel);
  scdChannel = localPrefs.getInt("tca_scd", scdChannel);

  mcpPmPin = localPrefs.getInt("mcp_pm", mcpPmPin);
  mcpBmePin = localPrefs.getInt("mcp_bme", mcpBmePin);
  mcpGasPin = localPrefs.getInt("mcp_gas", mcpGasPin);
  mcpOzonePin = localPrefs.getInt("mcp_o3", mcpOzonePin);
  mcpScdPin = localPrefs.getInt("mcp_scd", mcpScdPin);

  isGrovePM = localPrefs.getBool("is_grove", isGrovePM);
  bmeAddress = localPrefs.getUChar("bme_addr", 0x77);

  enablePowerSave = localPrefs.getBool("pwr_en", true);
  batVoltsFull = localPrefs.getFloat("v_full", 3.50);
  batVoltsPartial = localPrefs.getFloat("v_part", 3.20);
  batVoltsHyst = localPrefs.getFloat("v_hyst", 0.05);
  powerSavePartialMask = localPrefs.getUChar("mask_part", 0x05);
  powerSaveEmergMask = localPrefs.getUChar("mask_emerg", 0x0D);

  localPrefs.end();

  timeClient.setTimeOffset(0); // Mantem UTC puro para settimeofday (fuso GMT-3 e aplicado por configTime/localtime_r)
}

// ==========================================
// MODO 1: WEB SERVER LIGHT (MODO DE COLETA)
// ==========================================
void handleLightRoot() {
  String html =
      "<html><head><meta charset='UTF-8'><meta name='viewport' "
      "content='width=device-width, "
      "initial-scale=1.0'><title>MOQA</title></head><body "
      "style='font-family:sans-serif; text-align:center; padding:50px;'>";
  html += "<h2>Sistema MOQA - Coleta Ativa</h2>";
  html += "<p>Sensores operando normalmente.</p>";
  html += "<form action='/go_cfg' method='POST'><input type='submit' "
          "value='Entrar no Modo Configuração Dedicado (AP)' "
          "style='padding:15px; background:#DC3545; color:white; border:none; "
          "border-radius:5px; cursor:pointer;'></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleGoConfig() {
  server.send(200, "text/html",
              "<h2>Parando coleta e iniciando AP. Conecte na rede "
              "'MOQA_Config' em instantes...</h2>");
  delay(1000);
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  localPrefs.putBool("cfg_mode", true);
  localPrefs.end();
  ESP.restart();
}

#define BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MoqaBleCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String(pCharacteristic->getValue().c_str());
    value.trim();
    if (value.length() > 0) {
      Serial.printf("[BLE] Comando recebido: %s\n", value.c_str());
      if (value.equalsIgnoreCase("config") || value.equalsIgnoreCase("ap") ||
          value.equalsIgnoreCase("1")) {
        Serial.println("[BLE] Comando de configuracao recebido. Reiniciando em "
                       "Modo AP...");
        delay(500);
        Preferences localPrefs;
        localPrefs.begin("moqa_cfg", false);
        localPrefs.putBool("cfg_mode", true);
        localPrefs.end();
        ESP.restart();
      }
    }
  }
};

class MoqaServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("[BLE] Cliente conectado.");
  }
  void onDisconnect(BLEServer *pServer) {
    Serial.println("[BLE] Cliente desconectado. Reiniciando anuncios...");
    BLEDevice::startAdvertising();
  }
};

void initBLE() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[8];
  sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  String devSuffix = (SYSTEM_ID.length() > 0 && SYSTEM_ID != "MOQA-TEST-003") ? SYSTEM_ID : String(macStr);
  String bleName = "MOQA_" + devSuffix;
  BLEDevice::init(bleName.c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MoqaServerCallbacks());
  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      BLE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setCallbacks(new MoqaBleCallbacks());
  pCharacteristic->setValue(
      "Enviar 'config' ou 'ap' para reiniciar em modo configuracao");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.printf("[BLE] Servidor iniciado com nome '%s'.\n", bleName.c_str());
  Serial.printf("[BLE] UUID Servico: %s\n", BLE_SERVICE_UUID);
  Serial.printf("[BLE] UUID Caracteristica: %s\n", BLE_CHARACTERISTIC_UUID);
}

void startMiniWebServer() {
  server.on("/", HTTP_GET, handleLightRoot);
  server.on("/go_cfg", HTTP_GET, handleGoConfig);
  server.on("/go_cfg", HTTP_POST, handleGoConfig);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/remap", HTTP_POST, handleRemap);
  server.begin();
}

// ==========================================
// MODO 2: WEB SERVER FULL (MODO AP DEDICADO)
// ==========================================
void handleLiveBat() {
  String json = "{\"bat\":" + String(getBatVoltage(), 2) + "}";
  server.send(200, "application/json", json);
}

void handleFullRoot() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1.0'><title>MOQA "
                "Config</title>";
  html += "<script>setInterval(()=>{fetch('/"
          "live').then(r=>r.json()).then(d=>{document.getElementById('batval')."
          "innerText=d.bat.toFixed(2)+'V';});}, 2000);</script>";
  html += "</head><body style='font-family:sans-serif; padding:20px; "
          "background:#f4f4f4;'>";
  html += "<div style='background:white; padding:20px; border-radius:8px; "
          "box-shadow:0 0 10px rgba(0,0,0,0.1);'>";
  html +=
      "<h2 style='color:#333;'>Configuração DEDICADA (" + SYSTEM_ID + ")</h2>";
  html +=
      "<h3>Bateria em Tempo Real: <span id='batval' style='color:#28a745;'>" +
      String(getBatVoltage(), 2) + "V</span></h3>";

  html +=
      "<div style='margin-bottom:20px;'>"
      "<a href='/download' style='display:inline-block; padding:10px 15px; "
      "background:#17a2b8; color:white; text-decoration:none; "
      "border-radius:4px; font-weight:bold;'>Baixar Datalog do SD (.csv)</a>"
      "</div>";

  html += "<form action='/save' method='POST'>";
  html += "<h3>Rede Wi-Fi</h3>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br><br>";
  html +=
      "Senha: <input type='text' name='pass' value='" + password + "'><br><br>";

  html += "<h3>Identificação API</h3>";
  html += "System ID: <input type='text' name='sysid' value='" + SYSTEM_ID +
          "'><br><br>";
  html += "Board ID: <input type='text' name='brdid' value='" + BOARD_ID +
          "'><br><br>";
  html += "Code ID (Versão Firmware): <input type='text' name='codid' value='" + CODE_ID +
          "' readonly style='background:#e9ecef; cursor:not-allowed; font-weight:bold; color:#212529;'><br>"
          "<small style='color:#6c757d;'>Atrelado ao firmware instalado v" + CODE_ID + " (atualizável exclusivamente via OTA).</small><br><br>";
  html += "API Key: <input type='text' name='api' value='" + VITAE_API_KEY +
          "' size='50'><br><br>";
  html += "API URL Principal: <input type='text' name='api_url' value='" + VITAE_API_URL +
          "' size='60'><br><br>";
  html += "API URL Backup: <input type='text' name='api_url_bkp' value='" + VITAE_API_URL_BACKUP +
          "' size='60'><br><br>";

  html += "<h3>Geolocalização (Auto-Atualizado pelo GPS) e Tempo</h3>";
  html += "Latitude: <input type='text' name='lat' value='" + String(lat, 6) +
          "'><br><br>";
  html += "Longitude: <input type='text' name='lon' value='" + String(lon, 6) +
          "'><br><br>";
  html += "GMT Offset (s): <input type='number' name='gmt' value='" +
          String(gmtOffset_sec) + "'><br><br>";

  html += "<h3>Operação</h3>";
  html += "Período de Envio (s): <input type='number' name='colper' value='" +
          String(dataCollectionPeriod) + "'><br><br>";

  html += "<h3>Hardware (Divisor de Tensão)</h3>";
  html += "R1 (Ohms): <input type='number' name='r1' value='" + String(R1, 0) +
          "'><br><br>";
  html += "R2 (Ohms): <input type='number' name='r2' value='" + String(R2, 0) +
          "'><br><br>";
  html += "Vref (V): <input type='text' name='vref' value='" + String(Vref, 2) +
          "'><br><br>";
  html += "Fator Calibração Bateria: <input type='text' name='vcal' value='" +
          String(fator_calibracao, 3) + "'><br><br>";

  html += "<h3>Economia de Bateria</h3>";
  html += "Habilitar Controle: <select name='pwr_en'>";
  html += String("<option value='1'") + (enablePowerSave ? " selected" : "") +
          ">Ativado</option>";
  html += String("<option value='0'") + (!enablePowerSave ? " selected" : "") +
          ">Desativado</option>";
  html += "</select><br><br>";
  html +=
      "Tensão Nível Pleno (V_high): <input type='text' name='v_full' value='" +
      String(batVoltsFull, 2) + "'><br><br>";
  html +=
      "Tensão Nível Parcial (V_low): <input type='text' name='v_part' value='" +
      String(batVoltsPartial, 2) + "'><br><br>";
  html += "Histerese (V): <input type='text' name='v_hyst' value='" +
          String(batVoltsHyst, 2) + "'><br><br>";

  html += "<h4>Sensores a Desativar no Nível Parcial:</h4>";
  html += "PM (Particulado): <input type='checkbox' name='p_pm' value='1'" +
          String((powerSavePartialMask & 0x01) ? " checked" : "") + "><br>";
  html += "BME680: <input type='checkbox' name='p_bme' value='1'" +
          String((powerSavePartialMask & 0x02) ? " checked" : "") + "><br>";
  html += "MultiGas (GMXXX): <input type='checkbox' name='p_gas' value='1'" +
          String((powerSavePartialMask & 0x04) ? " checked" : "") + "><br>";
  html += "Ozônio (O3): <input type='checkbox' name='p_o3' value='1'" +
          String((powerSavePartialMask & 0x08) ? " checked" : "") + "><br><br>";

  html += "<h4>Sensores a Desativar no Nível Emergência:</h4>";
  html += "PM (Particulado): <input type='checkbox' name='e_pm' value='1'" +
          String((powerSaveEmergMask & 0x01) ? " checked" : "") + "><br>";
  html += "BME680: <input type='checkbox' name='e_bme' value='1'" +
          String((powerSaveEmergMask & 0x02) ? " checked" : "") + "><br>";
  html += "MultiGas (GMXXX): <input type='checkbox' name='e_gas' value='1'" +
          String((powerSaveEmergMask & 0x04) ? " checked" : "") + "><br>";
  html += "Ozônio (O3): <input type='checkbox' name='e_o3' value='1'" +
          String((powerSaveEmergMask & 0x08) ? " checked" : "") + "><br><br>";

  html += "<h3>Mapeamento de Hardware</h3>";
  if (hasMap) {
    html += "<p style='color: green; font-weight: bold;'>Status: Mapeamento de "
            "Hardware ATIVO</p>";
    html += "<p>PM: Canal TCA " + String(pmChannel) + ", Pino MCP " +
            (mcpPmPin == -1 ? "-1 (Always-On)" : String(mcpPmPin)) + " (" +
            (isGrovePM ? "Grove HM3301" : "DFRobot") + ")</p>";
    html += "<p>BME680: Canal TCA " + String(bmeChannel) + ", Pino MCP " +
            (mcpBmePin == -1 ? "-1 (Always-On)" : String(mcpBmePin)) + "</p>";
    html += "<p>MultiGas: Canal TCA " + String(gasChannel) + ", Pino MCP " +
            (mcpGasPin == -1 ? "-1 (Always-On)" : String(mcpGasPin)) + "</p>";
    html += "<p>Ozônio: Canal TCA " + String(ozoneChannel) + ", Pino MCP " +
            (mcpOzonePin == -1 ? "-1 (Always-On)" : String(mcpOzonePin)) +
            "</p>";
    html += "<p>CO2: Canal TCA " + String(scdChannel) +
            " (Pino MCP: " + String(mcpScdPin) + ")</p>";
  } else {
    html += "<p style='color: red; font-weight: bold;'>Status: NÃO MAPEADO (O "
            "auto-aprendizado rodará no próximo boot de Coleta)</p>";
  }

  html += "<input type='submit' value='Salvar e Voltar para Coleta' "
          "style='padding:10px; background:#007BFF; color:white; border:none; "
          "border-radius:4px; cursor:pointer;'></form>";

  html += "<br><form action='/remap' method='POST'><input type='submit' "
          "value='Forçar Re-mapeamento de Hardware' style='padding:10px; "
          "background:#FFC107; color:black; border:none; border-radius:4px; "
          "cursor:pointer;'></form><br>";

  html += "<hr><h3>Atualização de Firmware (OTA)</h3>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update' accept='.bin'><br><br>";
  html += "<input type='submit' value='Atualizar Firmware' "
          "style='padding:10px; background:#28a745; color:white; border:none; "
          "border-radius:4px; cursor:pointer;'></form>";

  html += "<br><form action='/exit' method='POST'><input type='submit' "
          "value='Sair sem Salvar (Voltar para Coleta)' style='padding:10px; "
          "background:#6c757d; color:white; border:none; border-radius:4px; "
          "cursor:pointer;'></form>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void savePrefsFromArgs() {
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);

  if (server.hasArg("ssid"))
    localPrefs.putString("ssid", server.arg("ssid"));
  if (server.hasArg("pass"))
    localPrefs.putString("pass", server.arg("pass"));
  if (server.hasArg("sysid"))
    localPrefs.putString("sysid", server.arg("sysid"));
  if (server.hasArg("brdid"))
    localPrefs.putString("brdid", server.arg("brdid"));
  // Versao de codigo e estritamente atrelada ao firmware atual (imutavel via web)
  localPrefs.putString("codid", CODE_ID);
  if (server.hasArg("api"))
    localPrefs.putString("api", server.arg("api"));
  if (server.hasArg("api_url"))
    localPrefs.putString("api_url", server.arg("api_url"));
  if (server.hasArg("api_url_bkp"))
    localPrefs.putString("api_url_bkp", server.arg("api_url_bkp"));

  if (server.hasArg("lat"))
    localPrefs.putFloat("lat", server.arg("lat").toFloat());
  if (server.hasArg("lon"))
    localPrefs.putFloat("lon", server.arg("lon").toFloat());
  if (server.hasArg("gmt"))
    localPrefs.putLong("gmt", server.arg("gmt").toInt());
  if (server.hasArg("colper"))
    localPrefs.putInt("colper", server.arg("colper").toInt());

  if (server.hasArg("r1"))
    localPrefs.putFloat("r1", server.arg("r1").toFloat());
  if (server.hasArg("r2"))
    localPrefs.putFloat("r2", server.arg("r2").toFloat());
  if (server.hasArg("vref"))
    localPrefs.putFloat("vref", server.arg("vref").toFloat());
  if (server.hasArg("vcal"))
    localPrefs.putFloat("vcal", server.arg("vcal").toFloat());

  if (server.hasArg("pwr_en"))
    localPrefs.putBool("pwr_en", server.arg("pwr_en").toInt() == 1);
  else
    localPrefs.putBool("pwr_en", false);

  if (server.hasArg("v_full"))
    localPrefs.putFloat("v_full", server.arg("v_full").toFloat());
  if (server.hasArg("v_part"))
    localPrefs.putFloat("v_part", server.arg("v_part").toFloat());
  if (server.hasArg("v_hyst"))
    localPrefs.putFloat("v_hyst", server.arg("v_hyst").toFloat());

  uint8_t partMask = 0;
  if (server.hasArg("p_pm"))
    partMask |= 0x01;
  if (server.hasArg("p_bme"))
    partMask |= 0x02;
  if (server.hasArg("p_gas"))
    partMask |= 0x04;
  if (server.hasArg("p_o3"))
    partMask |= 0x08;
  localPrefs.putUChar("mask_part", partMask);

  uint8_t emergMask = 0;
  if (server.hasArg("e_pm"))
    emergMask |= 0x01;
  if (server.hasArg("e_bme"))
    emergMask |= 0x02;
  if (server.hasArg("e_gas"))
    emergMask |= 0x04;
  if (server.hasArg("e_o3"))
    emergMask |= 0x08;
  localPrefs.putUChar("mask_emerg", emergMask);

  localPrefs.putBool("cfg_mode", false);
  localPrefs.end();
}

void handleSave() {
  if (server.args() > 0) {
    savePrefsFromArgs();
    server.send(200, "text/html",
                "<h2>Configuracoes Salvas! Reiniciando em Modo Coleta...</h2>");
    delay(1000);
    ESP.restart();
  }
}

void handleApiConfigGet() {
  JsonDocument doc;
  doc["ssid"] = ssid;
  doc["pass"] = password;
  doc["sysid"] = SYSTEM_ID;
  doc["brdid"] = BOARD_ID;
  doc["codid"] = CODE_ID;
  doc["api"] = VITAE_API_KEY;
  doc["api_url"] = VITAE_API_URL;
  doc["api_url_bkp"] = VITAE_API_URL_BACKUP;
  doc["lat"] = lat;
  doc["lon"] = lon;
  doc["gmt"] = gmtOffset_sec;
  doc["colper"] = dataCollectionPeriod;
  doc["r1"] = R1;
  doc["r2"] = R2;
  doc["vref"] = Vref;
  doc["vcal"] = fator_calibracao;
  doc["pwr_en"] = enablePowerSave ? 1 : 0;
  doc["v_full"] = batVoltsFull;
  doc["v_part"] = batVoltsPartial;
  doc["v_hyst"] = batVoltsHyst;
  doc["p_pm"] = (powerSavePartialMask & 0x01) ? 1 : 0;
  doc["p_bme"] = (powerSavePartialMask & 0x02) ? 1 : 0;
  doc["p_gas"] = (powerSavePartialMask & 0x04) ? 1 : 0;
  doc["p_o3"] = (powerSavePartialMask & 0x08) ? 1 : 0;
  doc["e_pm"] = (powerSaveEmergMask & 0x01) ? 1 : 0;
  doc["e_bme"] = (powerSaveEmergMask & 0x02) ? 1 : 0;
  doc["e_gas"] = (powerSaveEmergMask & 0x04) ? 1 : 0;
  doc["e_o3"] = (powerSaveEmergMask & 0x08) ? 1 : 0;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleApiConfigSave() {
  if (server.args() > 0) {
    savePrefsFromArgs();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Config saved, restarting...\"}");
    delay(500);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
  }
}

void handleExit() {
  server.send(200, "text/html", "<h2>Voltando para o Modo Coleta...</h2>");
  delay(1000);
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  localPrefs.putBool("cfg_mode", false);
  localPrefs.end();
  ESP.restart();
}

void handleUpdateEnd() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain",
              (Update.hasError()) ? "Falha na Atualizacao"
                                  : "OK! Reiniciando...");
  delay(1000);
  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  localPrefs.putBool("cfg_mode", false);
  localPrefs.end();
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Iniciando: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Sucesso: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleDownload() {
  if (!hasSD) {
    server.send(404, "text/plain", "Cartao SD nao inicializado ou ausente.");
    return;
  }
  if (!SD.exists("/datalog.csv")) {
    server.send(404, "text/plain",
                "Arquivo /datalog.csv nao encontrado no SD.");
    return;
  }
  File file = SD.open("/datalog.csv", FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Falha ao abrir o arquivo /datalog.csv.");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=datalog.csv");
  server.sendHeader("Connection", "close");
  server.streamFile(file, "text/csv");
  file.close();
}

void startFullWebServer() {
  server.enableCORS(true);
  server.on("/", HTTP_GET, handleFullRoot);
  server.on("/live", HTTP_GET, handleLiveBat);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/exit", HTTP_POST, handleExit);
  server.on("/update", HTTP_POST, handleUpdateEnd, handleUpdateUpload);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/remap", HTTP_POST, handleRemap);
  server.on("/api/config", HTTP_GET, handleApiConfigGet);
  server.on("/api/save", HTTP_POST, handleApiConfigSave);
  server.begin();
}

// ==========================================
// TASKS (SOMENTE NO MODO DE COLETA)
// ==========================================
void gpsTask(void *pvParameters) {
  // Utiliza a instância global SerialGPS configurada para a UART1 nos pinos
  // corretos
  SerialGPS.begin(115200, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  pinMode(PIN_GPS_POWER, OUTPUT);
  digitalWrite(PIN_GPS_POWER, LOW);

  bool gpsActive = false;
  bool isFirstFix = true;
  bool gpsClockSyncedThisCycle = false;
  unsigned long gpsStartTime = 0;
  unsigned long lastCollectTime = 0;

  double sumLat = 0, sumLon = 0;
  int validReads = 0;

  while (1) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    bool isOfflineMode = (WiFi.status() != WL_CONNECTED);
    // Dispara GPS em 4 condicoes criticas:
    // 1. Primeiro fix apos boot (isFirstFix)
    // 2. Relogio do sistema invalido (< 2025)
    // 3. Rotina periodica a cada 6 horas (no minuto 0)
    // 4. Operando offline (sem internet/NTP) por mais de 2 horas desde a ultima sincronizacao
    bool timeToRun = isFirstFix ||
                     !isSystemTimeValid() ||
                     ((timeinfo.tm_hour % 6 == 0) && (timeinfo.tm_min == 0)) ||
                     (isOfflineMode && (millis() - lastSuccessfulTimeSync > 7200000));

    if (!gpsActive && timeToRun) {
      gpsActive = true;
      gpsStartTime = millis();
      validReads = 0;
      sumLat = 0;
      sumLon = 0;
      gpsClockSyncedThisCycle = false;
      digitalWrite(PIN_GPS_POWER, HIGH);
      Serial.println("[GPS] Ligando modulo. Ciclo de 10 min iniciado.");
    }

    if (gpsActive) {
      int bytesRead = 0;
      while (SerialGPS.available() && bytesRead < 128) {
        gps.encode(SerialGPS.read());
        bytesRead++;
      }

      // Valida se a data do GPS possui calendário completo e consistente (ano, mes e dia validos)
      bool isGpsDateValid = gps.date.isValid() &&
                            gps.time.isValid() &&
                            (gps.date.year() >= 2025 && gps.date.year() <= 2035) &&
                            (gps.date.month() >= 1 && gps.date.month() <= 12) &&
                            (gps.date.day() >= 1 && gps.date.day() <= 31);

      if (!gpsClockSyncedThisCycle && isGpsDateValid) {
        time_t epoch = convertUtcToEpoch(gps.date.year(), gps.date.month(), gps.date.day(),
                                         gps.time.hour(), gps.time.minute(), gps.time.second());

        if (epoch >= 1735689600 && epoch <= 2082758400) {
          struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
          settimeofday(&tv, NULL);
          gpsClockSyncedThisCycle = true;
          lastSuccessfulTimeSync = millis();
          rtc_last_known_epoch = epoch;
          rtc_valid_magic = RTC_TIME_MAGIC;
          Serial.printf("[GPS] Relogio sincronizado com sucesso via satelites! Data: %04d-%02d-%02d %02d:%02d:%02d UTC | Epoch: %ld\n",
                        gps.date.year(), gps.date.month(), gps.date.day(),
                        gps.time.hour(), gps.time.minute(), gps.time.second(), (long)epoch);
        }
      }

      if (millis() - lastCollectTime >= 30000) {
        lastCollectTime = millis();
        if (gps.location.isValid() &&
            gps.location.lat() >= -90.0 && gps.location.lat() <= 90.0 &&
            gps.location.lng() >= -180.0 && gps.location.lng() <= 180.0 &&
            (gps.location.lat() != 0.0 || gps.location.lng() != 0.0)) {
          sumLat += gps.location.lat();
          sumLon += gps.location.lng();
          validReads++;
          Serial.printf("[GPS] Leitura OK (%d/20) -> Lat: %.6f, Lon: %.6f\n",
                        validReads, gps.location.lat(), gps.location.lng());
        }
      }

      if (millis() - gpsStartTime >= 600000) {
        gpsActive = false;
        isFirstFix = false;
        digitalWrite(PIN_GPS_POWER, LOW);
        Serial.println("[GPS] Tempo esgotado. Desligando.");

        if (validReads > 0) {
          lat = sumLat / validReads;
          lon = sumLon / validReads;
          Preferences localPrefs;
          localPrefs.begin("moqa_cfg", false);
          localPrefs.putFloat("lat", lat);
          localPrefs.putFloat("lon", lon);
          localPrefs.end();
          Serial.printf("[GPS] Media Final Salva -> Lat: %.6f, Lon: %.6f\n",
                        lat, lon);
        } else {
          Serial.println("[GPS] Sem sinal valido nesse ciclo.");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void cloudSyncTask(void *pvParameters) {
  SensorRecord record;
  uint32_t retryDelayMs = 5000;

  while (1) {
    if (xQueueReceive(dataQueue, &record, portMAX_DELAY) == pdPASS) {
      // Se estiver offline ou a API estiver desativada, mantem o registro na frente da fila e aguarda reconexao
      if (WiFi.status() != WL_CONNECTED || !ENABLE_VITAE_API) {
        xQueueSendToFront(dataQueue, &record, 0);
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }

      int httpResponseCode = -1;
      String errorStr = "";

      xSemaphoreTake(netMutex, portMAX_DELAY);

      if (WiFi.status() == WL_CONNECTED && ENABLE_VITAE_API) {
        // Valida se o timestamp do registro e valido (>= 2025). Se nao for, forca sincronizacao NTP imediata
        if (record.timestamp < 1735689600) {
          Serial.println("[API] Alerta: Timestamp do registro invalido (< 2025). Forcando sincronizacao NTP...");
          if (syncNtpTime()) {
            record.timestamp = time(NULL);
          }
        }

        uint32_t currentDay = record.timestamp / 86400;
        if (currentDay != lastSavedDay) {
          seqNum = 0;
          lastSavedDay = currentDay;
          Preferences localPrefs;
          localPrefs.begin("moqa_cfg", false);
          localPrefs.putUInt("day", lastSavedDay);
          localPrefs.end();
        }

        JsonDocument apiDoc; // Compatível com ArduinoJson v7 (alocação dinâmica
                             // flexível)

        apiDoc["sequence_number"] = seqNum;
        apiDoc["moqa_id"] = SYSTEM_ID;
        apiDoc["timestamp"] = record.timestamp;
        
        float sendLat = (lat >= -90.0f && lat <= 90.0f && (lat != 0.0f || lon != 0.0f)) ? lat : -3.7442f;
        float sendLon = (lon >= -180.0f && lon <= 180.0f && (lat != 0.0f || lon != 0.0f)) ? lon : -38.5748f;
        apiDoc["latitude"] = sendLat;
        apiDoc["longitude"] = sendLon;

        apiDoc["pm1"] = safeRound2(record.pm1);
        apiDoc["pm25"] = safeRound2(record.pm25);
        apiDoc["pm10"] = safeRound2(record.pm10);
        apiDoc["int_temp"] = safeRound2(record.t);
        apiDoc["hum"] = safeHum(record.h);
        apiDoc["pres"] = safeRound2(record.p);
        apiDoc["co"] = safeRound2(record.co);
        apiDoc["no2"] = safeRound2(record.no2);
        apiDoc["voc"] = safeRound2(record.voc);
        apiDoc["o3"] = safeRound2(record.o3);
        apiDoc["co2"] = safeRound2(record.co2);
        apiDoc["noise"] = safeRound2(record.snd);
        apiDoc["board_id"] = BOARD_ID;
        apiDoc["code_id"] = CODE_ID;
        apiDoc["uptime"] = millis() / 1000;
        apiDoc["firmware_version"] = CODE_ID;
        apiDoc["battery_level_mv"] = (int)(record.vBat * 1000.0);
        apiDoc["rssi"] = WiFi.RSSI();
        apiDoc["sended_packets"] = seqNum;

        String apiPayload;
        serializeJson(apiDoc, apiPayload);
        Serial.printf("[API] Payload enviado: %s\n", apiPayload.c_str());

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(15000);

        String hostHeader = "";
        resolveDnsFallback(VITAE_API_URL, hostHeader);

        HTTPClient http;
        Serial.printf("[API] Tentando enviar para a API Principal: %s\n", VITAE_API_URL.c_str());
        http.begin(client, VITAE_API_URL);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-Key", VITAE_API_KEY);
        http.setTimeout(10000);

        httpResponseCode = http.POST(apiPayload);
        if (httpResponseCode <= 0) {
          errorStr = http.errorToString(httpResponseCode);
        }
        http.end();

        // Se falhar (erro de conexao ou HTTP status fora de 2xx), tenta na API de Backup
        if (httpResponseCode <= 0 || httpResponseCode < 200 || httpResponseCode >= 300) {
          String backupHostHeader = "";
          resolveDnsFallback(VITAE_API_URL_BACKUP, backupHostHeader);
          
          WiFiClientSecure backupClient;
          backupClient.setInsecure();
          backupClient.setTimeout(15000);

          Serial.printf("[API] Falha no envio para a API Principal (HTTP: %d, Erro: %s). Tentando API Backup: %s\n", 
                        httpResponseCode, errorStr.c_str(), VITAE_API_URL_BACKUP.c_str());
          
          http.begin(backupClient, VITAE_API_URL_BACKUP);
          http.addHeader("Content-Type", "application/json");
          http.addHeader("X-API-Key", VITAE_API_KEY);
          http.setTimeout(10000);

          httpResponseCode = http.POST(apiPayload);
          if (httpResponseCode <= 0) {
            errorStr = http.errorToString(httpResponseCode);
          }
          http.end();
        }
      }
      xSemaphoreGive(netMutex);

      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("[API] Dados enviados com sucesso! HTTP Code: %d, Seq: "
                      "%u, Heap: %u | Fila pendente: %u itens\n",
                      httpResponseCode, seqNum, ESP.getFreeHeap(), (unsigned int)uxQueueMessagesWaiting(dataQueue));
        blinkBeacon(BEACON_SEND_OK);
        seqNum++;
        Preferences localPrefs;
        localPrefs.begin("moqa_cfg", false);
        localPrefs.putUInt("seq", seqNum);
        localPrefs.end();

        retryDelayMs = 1000; // Delay reduzido para descarregar rapidamente registros acumulados na fila

        // Flash verde por 1 segundo
        setRgbColor(0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        updateBaseLedColor();
      } else {
        Serial.printf("[API] Falha no envio! HTTP Code: %d, Erro: %s. "
                      "Retentando em %d segundos...\n",
                      httpResponseCode, errorStr.c_str(), retryDelayMs / 1000);
        blinkBeacon(BEACON_SEND_FAIL);
        xQueueSendToFront(dataQueue, &record, 0);

        // Flash vermelho por 1 segundo
        setRgbColor(255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        updateBaseLedColor();

        vTaskDelay(pdMS_TO_TICKS(retryDelayMs));

        // Aumenta o atraso exponencialmente até 5 minutos (300.000 ms)
        retryDelayMs = std::min(retryDelayMs * 2, (uint32_t)300000);
      }
    }
  }
}

void WiFiReconnectTask(void *pvParameters) {
  struct APInfo {
    int rssi;
    String macStr;
    uint8_t bssid[6];
  };

  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      updateBaseLedColor(); // Set color to Orange (or Red if sensor error)
      xSemaphoreTake(netMutex, portMAX_DELAY);
      WiFi.disconnect(true);
      vTaskDelay(pdMS_TO_TICKS(1000));

      int numberOfNetworks = WiFi.scanNetworks();
      std::vector<APInfo> networkList;
      for (int i = 0; i < numberOfNetworks; ++i) {
        if (WiFi.SSID(i) == ssid) {
          APInfo ap;
          ap.rssi = WiFi.RSSI(i);
          ap.macStr = WiFi.BSSIDstr(i);
          memcpy(ap.bssid, WiFi.BSSID(i), 6);
          networkList.push_back(ap);
        }
      }

      std::sort(
          networkList.begin(), networkList.end(),
          [](const APInfo &a, const APInfo &b) { return a.rssi > b.rssi; });
      bool connectionSuccess = false;

      for (const auto &network : networkList) {
        if (std::find(triedMACs.begin(), triedMACs.end(), network.macStr) ==
                triedMACs.end() &&
            std::find(badMACs.begin(), badMACs.end(), network.macStr) ==
                badMACs.end()) {

          WiFi.disconnect(true);
          vTaskDelay(pdMS_TO_TICKS(500));
          WiFi.begin(ssid.c_str(), password.c_str(), 0, network.bssid);

          int attempts = 0;
          while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            vTaskDelay(pdMS_TO_TICKS(500));
            attempts++;
          }

          if (WiFi.status() == WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            WiFi.setSleep(false); // Desativa o modo de economia de energia do
                                  // Wi-Fi para evitar perda de pacotes ARP/Ping
            // Configura servidores DNS publicos globais resilientes (8.8.8.8 e 1.1.1.1)
            WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
            
            // Sincroniza o relogio imediatamente via servidores NTP brasileiros (ntp.br)
            syncNtpTime();

            connectionSuccess = true;
            triedMACs.clear();
            badMACs.clear();
            Serial.print("[WIFI] STA Conectado. IP: ");
            Serial.println(WiFi.localIP());

            if (MDNS.begin("moqa")) {
              Serial.println(
                  "[MDNS] Acesse o dispositivo em: http://moqa.local");
              MDNS.addService("http", "tcp", 80);
            }

            updateBaseLedColor(); // Set color to Cyan (or Red if sensor
                                  // error)
            break;
          } else {
            triedMACs.push_back(network.macStr);
            updateBaseLedColor();
          }
        }
      }

      if (!connectionSuccess) {
        triedMACs.clear();
        badMACs.clear();
        Serial.println("[WIFI] Varredura concluida sem conexao. Coleta continua ativa normalmente. Nova tentativa em segundo plano em 30s...");
      }
      xSemaphoreGive(netMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void unifiedAcquisitionTask(void *pvParameters) {
  esp_task_wdt_add(NULL);
  static int accumulatedRecordsCount = 0;

  while (1) {
    esp_task_wdt_reset();
    readCount++;
    Wire.clearWriteError();

    if (sysStatus.scdStatus) {
      uint16_t co2;
      float temp, hum;
      selectSensorChannel(scdChannel);
      if (scd4x.readMeasurement(co2, temp, hum) == 0)
        a_co2 += co2;
    }

    if (sysStatus.bmeStatus) {
      selectSensorChannel(bmeChannel);
      if (bme.performReading()) {
        a_t += bme.temperature;
        a_h += bme.humidity;
        a_p += bme.pressure / 100.0;
      }
    }

    if (sysStatus.pmStatus) {
      selectSensorChannel(pmChannel);
      if (isGrovePM) {
        uint8_t buf[29];
        if (hm3301.read_sensor_value(buf, 29) == NO_ERROR) {
          uint16_t pm1_val = (uint16_t)buf[10] << 8 | buf[11];
          uint16_t pm25_val = (uint16_t)buf[12] << 8 | buf[13];
          uint16_t pm10_val = (uint16_t)buf[14] << 8 | buf[15];
          a_pm1 += pm1_val;
          a_pm25 += pm25_val;
          a_pm10 += pm10_val;
        } else {
          Serial.println(
              "[COLETA] Falha ao ler dados do Sensor PM Grove HM3301.");
        }
      } else {
        a_pm1 +=
            particle.gainParticleConcentration_ugm3(PARTICLE_PM1_0_ATMOSPHERE);
        a_pm25 +=
            particle.gainParticleConcentration_ugm3(PARTICLE_PM2_5_ATMOSPHERE);
        a_pm10 +=
            particle.gainParticleConcentration_ugm3(PARTICLE_PM10_ATMOSPHERE);
      }
    }

    if (sysStatus.ozoneStatus) {
      selectSensorChannel(ozoneChannel);
      a_o3 += Ozone.readOzoneData(10);
    }
    if (sysStatus.gasStatus) {
      selectSensorChannel(gasChannel);
      a_co += multiGas.measure_CO();
      a_no2 += multiGas.measure_NO2();
      a_voc += multiGas.measure_VOC();
    }

    currentBatVoltage = getBatVoltage();
    updateBatteryPowerSave(currentBatVoltage);
    a_vBat += currentBatVoltage;
    a_snd += readSoundDB();

    blinkBeacon(BEACON_COLLECT);

    int intermediatePeriod =
        (dataCollectionPeriod < 60) ? dataCollectionPeriod : 60;
    int intermediateReads = intermediatePeriod / 5;
    if (intermediateReads < 1)
      intermediateReads = 1;

    if (readCount >= intermediateReads) {
      SensorRecord rec;
      time_t now;
      time(&now);
      if (now < 1735689600 && WiFi.status() == WL_CONNECTED) {
        if (syncNtpTime()) {
          time(&now);
        }
      }
      rec.timestamp = now;
      if (now >= 1735689600) {
        rtc_last_known_epoch = now;
        rtc_valid_magic = RTC_TIME_MAGIC;
      }

      rec.pm1 = sysStatus.pmStatus ? (a_pm1 / intermediateReads) : 9999.0;
      rec.pm25 = sysStatus.pmStatus ? (a_pm25 / intermediateReads) : 9999.0;
      rec.pm10 = sysStatus.pmStatus ? (a_pm10 / intermediateReads) : 9999.0;
      rec.t = sysStatus.bmeStatus ? (a_t / intermediateReads) : 9999.0;
      rec.h = sysStatus.bmeStatus ? (a_h / intermediateReads) : 9999.0;
      rec.p = sysStatus.bmeStatus ? (a_p / intermediateReads) : 9999.0;
      rec.o3 = sysStatus.ozoneStatus ? (a_o3 / intermediateReads) : 9999.0;
      rec.co2 = sysStatus.scdStatus ? (a_co2 / intermediateReads) : 9999.0;
      rec.co = sysStatus.gasStatus ? (a_co / intermediateReads) : 9999.0;
      rec.no2 = sysStatus.gasStatus ? (a_no2 / intermediateReads) : 9999.0;
      rec.voc = sysStatus.gasStatus ? (a_voc / intermediateReads) : 9999.0;
      rec.snd = a_snd / intermediateReads;
      rec.vBat = a_vBat / intermediateReads;

      if (!inWarmup) {
        logToSD(rec);
        ramHistory.push(rec);
        Serial.printf("[COLETA] Registro intermediario de %ds gerado e salvo. "
                      "(CO: %.2f, NO2: %.2f, VOC: %.2f, CO2: %.2f)\n",
                      intermediatePeriod, rec.co, rec.no2, rec.voc, rec.co2);

        accumulatedRecordsCount++;
        int requiredRecords = dataCollectionPeriod / intermediatePeriod;
        if (requiredRecords < 1)
          requiredRecords = 1;

        if (accumulatedRecordsCount >= requiredRecords) {
          SensorRecord apiRec = averageLastRecords(requiredRecords);
          if (apiRec.timestamp >= 1735689600) {
            Preferences timePrefs;
            timePrefs.begin("moqa_cfg", false);
            timePrefs.putUInt("last_epoch", (uint32_t)apiRec.timestamp);
            timePrefs.end();
          }
          Serial.printf("[COLETA] Periodo de envio de %ds atingido. Media de "
                        "%d registros consolidada. Enviando para a fila...\n",
                        dataCollectionPeriod, requiredRecords);
          if (xQueueSend(dataQueue, &apiRec, 0) != pdPASS) {
            SensorRecord dropped;
            xQueueReceive(dataQueue, &dropped, 0); // Descarta o pacote mais antigo
            xQueueSend(dataQueue, &apiRec, 0);      // Insere o pacote mais recente
            Serial.println("[COLETA] Fila de envio cheia (> 15h). Pacote mais antigo descartado para dar lugar ao novo.");
          }
          accumulatedRecordsCount = 0;
        }
      } else {
        Serial.println(
            "[COLETA] Warmup ativo. Descartando leitura intermediaria.");
      }

      readCount = 0;
      a_pm1 = a_pm25 = a_pm10 = a_t = a_h = a_p = 0;
      a_co = a_no2 = a_voc = a_o3 = a_co2 = a_snd = a_vBat = 0;
    }

    if (ESP.getFreeHeap() < 20000) {
      Serial.println("[CRITICO] Memoria RAM baixa. Reiniciando por seguranca.");
      ESP.restart();
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// ==========================================
// SETUP PRINCIPAL DE ROTEAMENTO
// ==========================================
void setup() {
  // Garante GPS desligado o mais cedo possível no boot
  pinMode(PIN_GPS_POWER, OUTPUT);
  digitalWrite(PIN_GPS_POWER, LOW);

  Serial.begin(115200);

  // Atraso para re-enumeração da porta USB COM nativa no PC
  for (int i = 0; i < 5; i++) {
    delay(1000);
  }

  clearSerialScreen();

  // Reconfiguração correta do watchdog no ESP-IDF v5 (Arduino Core 3.x)
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 15000, .idle_core_mask = (1 << 0), .trigger_panic = true};
  esp_task_wdt_reconfigure(&wdt_config);

  // Inscreve a loopTask (tarefa principal que roda setup/loop) no watchdog
  esp_task_wdt_add(NULL);

  pinMode(PIN_EXT_LED, OUTPUT);
  pinMode(PIN_BTN_CFG, INPUT_PULLUP);
  digitalWrite(PIN_EXT_LED, LOW);
  analogReadResolution(12);

  // Inicializa RGB LED e Mutex o mais cedo possível
  ledMutex = xSemaphoreCreateMutex();
  rgbLed.begin();

  Preferences localPrefs;
  localPrefs.begin("moqa_cfg", false);
  isConfigMode = localPrefs.getBool("cfg_mode", false);
  localPrefs.end();

  loadPreferences();

  // Recupera horario previo da memoria RTC ou NVS caso o sistema reinicie sem conectividade imediata
  if (!isSystemTimeValid()) {
    if (rtc_valid_magic == RTC_TIME_MAGIC && rtc_last_known_epoch >= 1735689600) {
      struct timeval tv = {.tv_sec = rtc_last_known_epoch, .tv_usec = 0};
      settimeofday(&tv, NULL);
      Serial.printf("[RELOGIO] Horario restaurado da memoria RTC apos reinicio: Epoch %ld\n", (long)rtc_last_known_epoch);
    } else {
      Preferences timePrefs;
      timePrefs.begin("moqa_cfg", true);
      uint32_t nvsEpoch = timePrefs.getUInt("last_epoch", 0);
      timePrefs.end();
      if (nvsEpoch >= 1735689600) {
        struct timeval tv = {.tv_sec = (time_t)nvsEpoch, .tv_usec = 0};
        settimeofday(&tv, NULL);
        Serial.printf("[RELOGIO] Horario base recuperado da NVS: Epoch %lu\n", (unsigned long)nvsEpoch);
      }
    }
  }

  if (isConfigMode) {
    updateBaseLedColor(); // Define cor amarela para modo AP
    Serial.printf("=== SISTEMA MOQA: MODO CONFIGURACAO DEDICADO (v%s) ===\n", CODE_ID.c_str());
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apName[32];
    sprintf(apName, "MOQA_Config_%02X%02X%02X", mac[3], mac[4], mac[5]);
    WiFi.softAP(apName, "moqa1234");

    if (MDNS.begin("moqa")) {
      Serial.println("[MDNS AP] Acesse o dispositivo em: http://moqa.local");
      MDNS.addService("http", "tcp", 80);
    }

    startFullWebServer();
    Serial.println("[AP] Ativo. Conecte no IP: 192.168.4.1");
    configModeStartTime = millis();
  } else {
    inWarmup = true;
    updateBaseLedColor(); // Define cor azul para warmup
    Serial.printf("=== SISTEMA MOQA: MODO COLETA (v%s) ===\n", CODE_ID.c_str());

    // Inicializa o WiFi em background ANTES das tasks para nao crashar o
    // FreeRTOS
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);

    Serial.println(
        "[STARTUP] Iniciando rotinas (Warmup ativo de 5 minutos)...");

    dataQueue = xQueueCreate(QUEUE_SIZE, sizeof(SensorRecord));
    netMutex = xSemaphoreCreateMutex();

    if (dataQueue == NULL || netMutex == NULL) {
      Serial.println("[ERRO] Falha ao alocar Memoria RTOS!");
      delay(1000);
      ESP.restart();
    }

    Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.begin();
    Wire.setTimeOut(150);

    Serial.println("[I2C SCAN] Iniciando varredura completa do barramento...");
    int nDevices = 0;
    int errorCounts[6] = {0}; // Para estatisticas de erros (0 a 5)
    for (byte address = 8; address < 120; address++) {
      esp_task_wdt_reset();
      delay(1);
      Wire.beginTransmission(address);
      byte error = Wire.endTransmission();
      if (error == 0) {
        Serial.printf(
            "[I2C SCAN] Dispositivo respondendo no endereco: 0x%02X\n",
            address);
        nDevices++;
      } else {
        if (error < 6) {
          errorCounts[error]++;
        }
        if (error != 2) { // 2 = NACK normal (sem dispositivo). Qualquer outro
                          // erro e suspeito
          Serial.printf(
              "[I2C SCAN] Endereco 0x%02X retornou erro especial: %d\n",
              address, error);
        }
      }
    }
    Serial.printf("[I2C SCAN] Varredura concluida. Dispositivos: %d. Erros - "
                  "NACK: %d, Outros: (1: %d, 3: %d, 4: %d, 5: %d)\n",
                  nDevices, errorCounts[2], errorCounts[1], errorCounts[3],
                  errorCounts[4], errorCounts[5]);

    // --- Detecção da Versão Alternativa de Hardware ---
    isAlternativeHardware = false;
    tcaAddress = 0x70;

    // 1. Procurar pelo TCA9548 nos endereços 0x70 a 0x77
    for (uint8_t addr = 0x70; addr <= 0x77; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        tcaAddress = addr;
        isAlternativeHardware = true;
        Serial.printf("[HARDWARE] TCA9548 detectado no endereco: 0x%02X\n",
                      tcaAddress);
        break;
      }
    }

    if (isAlternativeHardware) {
      // 2. Buscar nos canais do TCA pelo MCP23008 (endereços 0x20 a 0x27)
      int mcpChannel = -1;
      uint8_t mcpAddress = 0x20;
      for (uint8_t ch = 0; ch < 8; ch++) {
        selectTcaChannel(tcaAddress, ch);
        for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
          Wire.beginTransmission(addr);
          if (Wire.endTransmission() == 0) {
            mcpChannel = ch;
            mcpAddress = addr;
            Serial.printf("[HARDWARE] MCP23008 detectado no canal: %d, "
                          "endereco: 0x%02X\n",
                          mcpChannel, mcpAddress);
            break;
          }
        }
        if (mcpChannel != -1)
          break;
      }

      if (mcpChannel != -1) {
        // 3. Inicializar e ligar todas as portas do MCP (HIGH) para alimentar
        // os sensores
        selectTcaChannel(tcaAddress, mcpChannel);

        // Configura IODIR como saídas (0x00)
        Wire.beginTransmission(mcpAddress);
        Wire.write(0x00); // registrador IODIR
        Wire.write(0x00); // todas as portas como saída
        if (Wire.endTransmission() != 0) {
          Serial.println("[HARDWARE] Falha ao configurar IODIR no MCP23008.");
        }

        if (!hasMap) {
          descobrirPinosMCP();
        } else {
          // Zera todas as portas do MCP (LOW) para descarregar latch/resetar
          Wire.beginTransmission(mcpAddress);
          Wire.write(0x09); // registrador GPIO
          Wire.write(0x00); // todas as saídas LOW
          Wire.endTransmission();
          Serial.println(
              "[HARDWARE] MCP23008: Todas as portas zeradas (LOW) para reset.");
          delay(100);

          // Define GPIO como HIGH (0xFF)
          Wire.beginTransmission(mcpAddress);
          Wire.write(0x09); // registrador GPIO
          Wire.write(0xFF); // todas as saídas HIGH
          if (Wire.endTransmission() == 0) {
            Serial.println("[HARDWARE] MCP23008 configurado. Portas ligadas "
                           "(HIGH). Sensores alimentados.");
            mcpGpioState = 0xFF;
          } else {
            Serial.println("[HARDWARE] Falha ao ligar portas do MCP23008.");
          }

          // Aguarda estabilização da alimentação dos sensores
          delay(150);

          // Habilitar todos os canais do multiplexador permanentemente (0xFF)
          Wire.beginTransmission(tcaAddress);
          Wire.write(0xFF);
          Wire.endTransmission();
          Serial.println("[HARDWARE] TCA9548: Todos os canais habilitados "
                         "permanentemente (0xFF).");
        }
      } else {
        Serial.println(
            "[HARDWARE] TCA detectado, mas MCP23008 nao foi localizado nos "
            "canais. Revertendo para hardware original.");
        isAlternativeHardware = false;
      }
    }

    if (!isAlternativeHardware) {
      Serial.println("[HARDWARE] Versao original de hardware detectada (sem "
                     "TCA9548/MCP23008).");
    }

    pinMode(PIN_BAT, INPUT);
    pinMode(PIN_SOUND, INPUT);

    SPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (SD.begin(PIN_SD_CS)) {
      hasSD = true;
      Serial.println("[SD] Inicializado com sucesso.");
      File file = SD.open("/datalog.csv", FILE_APPEND);
      if (file) {
        if (file.size() == 0)
          file.println("timestamp,pm1,pm25,pm10,t,h,p,o3,no2,voc,co2,snd,vBat");
        file.close();
      }
    } else {
      Serial.println("[SD] Falha ou nao encontrado (Ignorando).");
    }

    selectSensorChannel(scdChannel);
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    if (scd4x.startPeriodicMeasurement() == 0) {
      sysStatus.scdStatus = true;
    }
    selectSensorChannel(bmeChannel);
    if (bme.begin(bmeAddress)) {
      sysStatus.bmeStatus = true;
      bme.setGasHeater(320, 150);
    }
    selectSensorChannel(pmChannel);
    if (isGrovePM) {
      if (hm3301.init() == NO_ERROR) {
        sysStatus.pmStatus = true;
        Serial.println(
            "[HARDWARE] Sensor PM Grove HM3301 inicializado com sucesso.");
      } else {
        Serial.println(
            "[HARDWARE] Falha ao inicializar Sensor PM Grove HM3301.");
      }
    } else {
      if (particle.begin()) {
        sysStatus.pmStatus = true;
        Serial.println(
            "[HARDWARE] Sensor PM DFRobot inicializado com sucesso.");
      } else {
        Serial.println("[HARDWARE] Falha ao inicializar Sensor PM DFRobot.");
      }
    }
    selectSensorChannel(ozoneChannel);
    if (Ozone.begin(OZONE_ADDRESS_3)) {
      sysStatus.ozoneStatus = true;
    }
    selectSensorChannel(gasChannel);
    Wire.beginTransmission(0x08);
    if (Wire.endTransmission() == 0) {
      multiGas.begin(Wire, 0x08);
      sysStatus.gasStatus = true;
      Serial.println("[HARDWARE] Sensor GMXXX (Multigas) detectado.");
    } else {
      sysStatus.gasStatus = false;
      Serial.println("[HARDWARE] Sensor GMXXX (Multigas) NAO detectado.");
    }

    // Verifica se algum sensor essencial falhou na inicialização
    if (!sysStatus.scdStatus || !sysStatus.bmeStatus || !sysStatus.pmStatus ||
        !sysStatus.ozoneStatus || !sysStatus.gasStatus) {
      sensorError = true;
      Serial.println("[AVISO] Um ou mais sensores falharam ao iniciar!");
    } else {
      sensorError = false;
    }
    updateBaseLedColor(); // Aplica cor de erro ou estado atual (WiFi Orange por
                          // enquanto)

    startMiniWebServer();
    initBLE();

    xTaskCreate(gpsTask, "GPS", 4096, NULL, 1, NULL);
    xTaskCreate(WiFiReconnectTask, "Wifi", 4096, NULL, 1, NULL);
    xTaskCreate(unifiedAcquisitionTask, "Acq", 8192, NULL, 1, NULL);
    xTaskCreate(cloudSyncTask, "Cloud", 8192, NULL, 1, NULL);
  }
}

void loop() {
  esp_task_wdt_reset(); // Alimenta o watchdog na loopTask do Arduino
  server.handleClient();

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("config") || cmd.equalsIgnoreCase("ap") ||
        cmd.equalsIgnoreCase("c")) {
      Serial.println("[SERIAL] Comando de configuracao recebido. Reiniciando "
                     "em Modo AP...");
      delay(200);
      Preferences localPrefs;
      localPrefs.begin("moqa_cfg", false);
      localPrefs.putBool("cfg_mode", true);
      localPrefs.end();
      ESP.restart();
    } else if (cmd.equalsIgnoreCase("exit") || cmd.equalsIgnoreCase("coleta") ||
               cmd.equalsIgnoreCase("e") || cmd.equalsIgnoreCase("x")) {
      Serial.println(
          "[SERIAL] Comando de coleta recebido. Reiniciando em Modo Coleta...");
      delay(200);
      Preferences localPrefs;
      localPrefs.begin("moqa_cfg", false);
      localPrefs.putBool("cfg_mode", false);
      localPrefs.end();
      ESP.restart();
    } else if (cmd.startsWith("http ")) {
      String targetUrl = cmd.substring(5);
      targetUrl.trim();
      Serial.printf("[DEBUG] Efetuando GET para: %s\n", targetUrl.c_str());
      HTTPClient http;
      http.begin(targetUrl);
      http.setTimeout(5000);
      int code = http.GET();
      Serial.printf("[DEBUG] Resposta HTTP: %d\n", code);
      if (code < 0) {
        Serial.printf("[DEBUG] Erro: %s\n", http.errorToString(code).c_str());
      }
      http.end();
    }
  }

  if (!isConfigMode) {
    // Gerenciamento de Warmup Não-Bloqueante
    if (inWarmup) {
      static unsigned long lastBlink = 0;
      if (millis() - lastBlink >= 1000) {
        lastBlink = millis();
        digitalWrite(PIN_EXT_LED, !digitalRead(PIN_EXT_LED));
      }

      // Fim do warmup automático por tempo (5 minutos)
      if (millis() >= 300000) {
        Serial.println("[STARTUP] Warmup concluido automaticamente.");
        inWarmup = false;
        digitalWrite(PIN_EXT_LED, LOW);
        updateBaseLedColor();
      }
    }

    static unsigned long btnPressTime = 0;
    static bool btnPressed = false;
    static unsigned long lastPrintSec = 0;

    if (digitalRead(PIN_BTN_CFG) == LOW) {
      if (!btnPressed) {
        btnPressed = true;
        btnPressTime = millis();
        lastPrintSec = 0;
        Serial.println("[BOTAO] Botao pressionado. Mantenha pressionado por 5 "
                       "segundos para entrar no modo AP...");
      } else {
        unsigned long duration = millis() - btnPressTime;
        unsigned long elapsedSec = duration / 1000;
        if (elapsedSec != lastPrintSec && elapsedSec <= 5) {
          lastPrintSec = elapsedSec;
          Serial.printf("[BOTAO] Segurando... %lu/5s\n", elapsedSec);
        }
        if (duration > 5000) {
          Serial.println(
              "[BOTAO] 5 segundos atingidos! Reiniciando em Modo AP...");
          Preferences localPrefs;
          localPrefs.begin("moqa_cfg", false);
          localPrefs.putBool("cfg_mode", true);
          localPrefs.end();
          ESP.restart();
        }
      }
    } else {
      if (btnPressed) {
        unsigned long duration = millis() - btnPressTime;
        btnPressed = false;

        // Se estava em warmup e o clique foi curto (< 2000 ms), pula o warmup
        if (inWarmup && duration < 2000) {
          Serial.println("[STARTUP] Warmup pulado pelo usuario.");
          inWarmup = false;
          digitalWrite(PIN_EXT_LED, LOW);
          updateBaseLedColor();
        } else if (duration < 5000) {
          Serial.println(
              "[BOTAO] Solto antes de 5 segundos. Operacao cancelada.");
        }
      }
    }

    // Relatório periódico de status em Modo Coleta
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint >= 30000) {
      lastStatusPrint = millis();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(
            "[STATUS] WiFi Conectado ('%s'). IP: %s. Acesse: "
            "http://moqa.local/go_cfg ou http://%s/go_cfg para configurar.\n",
            ssid.c_str(), WiFi.localIP().toString().c_str(),
            WiFi.localIP().toString().c_str());
      } else {
        Serial.printf(
            "[STATUS] Conectando ao WiFi '%s'... (Acesse via Serial enviando "
            "'c' ou segure o botao por 5s para forcar modo AP)\n",
            ssid.c_str());
      }
      // Garante que o BLE continue anunciando se tiver sido interrompido por
      // oscilações de Wi-Fi ou desconexão
      BLEDevice::startAdvertising();
    }
  } else {
    // Relatório periódico de status em Modo Configuração AP
    static unsigned long lastAPPrint = 0;
    if (millis() - lastAPPrint >= 15000) {
      lastAPPrint = millis();
      Serial.println(
          "[AP] Modo Configuração Ativo. Rede WiFi: 'MOQA_Config' (Senha: "
          "moqa1234). Acesse: http://moqa.local ou http://192.168.4.1");
    }
  }

  if (isConfigMode && (millis() - configModeStartTime > 600000)) {
    Serial.println("[AP] Timeout de seguranca. Voltando para Coleta...");
    Preferences localPrefs;
    localPrefs.begin("moqa_cfg", false);
    localPrefs.putBool("cfg_mode", false);
    localPrefs.end();
    ESP.restart();
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}
