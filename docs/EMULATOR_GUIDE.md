# Guia Completo do Emulador ESP32-C6 & Módulos de Sensores

Este projeto implementa uma arquitetura completa de emulação de hardware para o **ESP32-C6 (RISC-V 160MHz)** e seus sensores modulares associados ao **Sistema MOQA (v4.0.0)**. O emulador permite executar e testar todo o fluxo de código sem a necessidade de hardware físico.

---

## 🏗️ Arquitetura do Emulador

```
esp32 emulator/
├── hal/                          # Camada de Abstração de Hardware (Mocks HAL C++)
│   ├── Arduino.h / .cpp          # Tipos base, millis(), delay(), GPIO, ADC, Serial
│   ├── Wire.h / .cpp             # Barramento I2C simulado (TwoWire)
│   ├── SPI.h / SD.h / .cpp       # Cartão SD virtual com gravação em /datalog.csv
│   ├── WiFi.h / .cpp             # Pilha WiFi virtual (STA / SoftAP / BSSID scan)
│   ├── HTTPClient.h / .cpp       # Cliente HTTP com interceptação e injeção de respostas
│   ├── Preferences.h / .cpp      # Memória NVS do ESP32 com persistência
│   ├── freertos/                 # FreeRTOS thread-safe nativo C++ (Queue, Mutex, Tasks)
│   ├── Adafruit_NeoPixel.h       # LED RGB WS2812B com notificação de cores
│   ├── esp_task_wdt.h            # Watchdog Timer do ESP32
│   └── [Adaptadores de Sensores] # SCD41, BME680, PM DFRobot/Grove, Ozônio, Multigas, TinyGPS++
│
├── modules/                      # Modelos Físicos de Sensores e Periféricos I2C
│   ├── i2c_bus.h                 # Barramento I2C virtual central com roteamento por endereço
│   ├── tca9548a.h                # Multiplexador I2C TCA9548A (8 canais independentes)
│   ├── mcp23008.h                # Expansor GPIO MCP23008 (controle de alimentação de sensores)
│   ├── scd41.h                   # Sensirion SCD41 (CO2, Temperatura, Umidade)
│   ├── bme680.h                  # Bosch BME680 (Pressão, Temperatura, Umidade, Gás VOC)
│   ├── dfrobot_pm.h              # DFRobot Air Quality Sensor (PM1.0, PM2.5, PM10)
│   ├── hm3301.h                  # Seeed HM3301 Grove (PM1.0, PM2.5, PM10)
│   ├── dfrobot_ozone.h           # DFRobot Ozone Sensor (O3 em ppb)
│   ├── seeed_multigas.h          # Seeed GMXXX Multigas (CO, NO2, VOC)
│   ├── battery_model.h           # Divisor de tensão resistivo (R1=47k, R2=100k, Vref=3.3V)
│   ├── sound_sensor.h            # Envelope ADC e cálculo de dB SPL
│   └── gps_model.h               # Gerador de sentenças NMEA ($GPRMC, $GPGGA) para SerialGPS
│
├── emulator_runner/              # Testes Automatizados e Executável Nativo C++
│   ├── test_suites.cpp           # Suite completa de testes de integração (5 cenários)
│   └── test_hal.cpp              # Teste de fumaça inicial da camada HAL
│
├── web_dashboard/                # Dashboard Web Visual Interativo (Node.js + Vanilla CSS)
│   ├── server.js                 # Servidor de eventos SSE e API REST
│   └── public/                   # Interface com visualizador ESP32, sliders e telemetria
│
└── build.bat                     # Script unificado para compilação e execução rápida
```

---

## 🚀 Como Executar

### 1. Rodar os Testes de Integração via Linha de Comando (CLI)
Para compilar e rodar a suíte de testes nativa em C++:
```cmd
build.bat test
```
A suíte valida:
1. **Varredura e Detecção do Barramento I2C** (7 dispositivos: 0x08, 0x19, 0x20, 0x62, 0x70, 0x73, 0x77).
2. **Corte e Alimentação de Sensores (Power Gating)** via registrador GPIO do MCP23008.
3. **Thresholds e Histerese da Bateria**: Transição entre modos `FULL (>3.50V)`, `PARTIAL (3.20V - 3.50V)` e `EMERGENCY (<3.20V)`.
4. **Fila RTOS e Descarte Circular**: Comportamento de buffer cheio (descarte do registro mais antigo ao receber novos pacotes).
5. **Telemetria Cloud e Fallback de API**: Falha na URL primária e envio automático para a URL de backup com retry exponencial.
6. **Gravação e Formatação no Cartão SD**: Persistência do arquivo `/datalog.csv`.

### 2. Iniciar o Dashboard Web Visual Interativo
Para abrir a interface gráfica de controle e visualização em tempo real:
```cmd
build.bat dashboard
```
Ou diretamente:
```cmd
node web_dashboard/server.js
```
Acesse no seu navegador: **`http://localhost:3000`**

---

## 🎮 Recursos do Dashboard Web

1. **Visualizador Físico da Placa ESP32-C6**:
   - **LED RGB NeoPixel (Pino 8)**: Anel luminoso e núcleo reativo em tempo real com as cores oficiais:
     - 🔵 **Azul Suave**: Warmup (5 min) / Operação Normal
     - 🟡 **Amarelo**: Modo Configuração AP ativo
     - 🟠 **Laranja**: Sem conexão WiFi
     - 🔴 **Vermelho**: Erro em um ou mais sensores
     - 🟢 **Verde (Flash)**: Confirmação de pacote enviado com sucesso à API Vitae
   - **LED Externo (Pino 15)**: Pisca durante o warmup e gera flashes de beacon a cada coleta.
   - **Botão Físico CFG (Pino 9)**: Pressione para testar clique rápido (< 2s, pula warmup) ou segure por 5s para forçar o reinício em Modo AP.
   - **Indicadores de Trilhos MCP23008**: Mostram visualmente quais sensores estão alimentados ou cortados pela economia de bateria.

2. **Sliders Interativos para Todos os Sensores**:
   - Ajuste em tempo real: CO2, Temperatura, Umidade, Pressão, PM1, PM2.5, PM10, Ozônio, CO, NO2, VOC, Ruído dB e Tensão da Bateria.

3. **Injetor de Cenários em 1 Clique**:
   - `🌿 Ar Limpo`: Valores ambientais ideais.
   - `🏭 Poluição Severa`: Pico crítico de PM2.5 (145 µg/m³) e Ozônio.
   - `🪫 Bateria Crítica`: Tensão de 3.05V, ativando o modo `EMERGENCY` e desligando PM, Gás e Ozônio.
   - `🔋 Bateria Plena`: Tensão de 4.05V, reativando todos os sensores.
   - `🌐 Queda WiFi/Cloud`: Simula perda de conexão, retendo pacotes na fila RTOS.

4. **Monitor de Telemetria Cloud & Cartão SD**:
   - Exibição do payload JSON exato com botão para copiar.
   - Tabela ao vivo dos registros gravados em `/datalog.csv`.

5. **Terminal Serial Interativo**:
   - Envio de comandos como `config`, `coleta`, `exit` e `http <url>`.
   - Visualização de logs de boot, I2C scan e sincronização NTP.

6. **Executor de Testes Integrado na Interface & Auditoria Completa de Fundo**:
   - O botão `🧪 Executar Testes C++` dispara o executável nativo compilado e exibe o relatório de aprovação diretamente no navegador.
   - **Gravação de Fundo em Disco**: Todas as transações de barramento I2C (endereços, registradores, bytes em hexadecimal e ACK/NACK), acionamento de trilhos via MCP23008, filas FreeRTOS, requisições HTTP e operações no SD Card são salvas automaticamente em disco no diretório `logs/`.
   - **Download Imediato**: Botões dedicados no modal e no cabeçalho do Dashboard permitem baixar a trilha completa em formato `.log` ou `.json`.

---

## ⏱️ Cadência Estrita de Tempo Real (1.0x - Sem Aceleração)

Por especificação de projeto, o emulador **não utiliza aceleração artificial de tempo** (1 segundo real = 1 segundo de hardware emulado):
- **Tempos de Aquecimento Físico**: O warmup de 5 minutos (300s) respeita a estabilização real dos sensores térmicos e químicos (SCD41 e BME680).
- **Ciclos de Amostragem**: Amostragens de sensores ocorrem estritamente a cada 5 segundos físicos.
- **Temporizador Watchdog**: O Hardware Watchdog (15 segundos) monitora a execução em cadência real de relógio (`std::chrono::steady_clock`).
- **Despacho Cloud**: Telemetria para a API Vitae enviada na frequência de projeto.

---

## 🛡️ Auditoria de Fundo e Diagnóstico a Posteriori

O emulador salva **exatamente tudo o que roda em segundo plano** em arquivos persistentes no diretório `logs/`:

| Arquivo | Formato | Descrição |
|---|---|---|
| `logs/latest_test_execution.log` | Texto Formatado | Trilha legível por humanos com timestamp, uptime, subsistema, ação, status, detalhes e hex dump de todas as operações dos testes. |
| `logs/latest_test_execution.json` | JSON Estruturado | Metadados completos da sessão de testes com vetor de eventos indexados para análise automatizada. |
| `logs/emulator_background_trace.log` | Texto Contínuo | Trilha contínua de auditoria em segundo plano gerada pelo simulador durante a execução ao vivo. |
| `logs/emulator_background_trace.jsonl` | JSON Lines | Fluxo estruturado linha a linha de todos os eventos de fundo (I2C, RTOS, Cloud, SD, ADC). |

Esses arquivos podem ser inspecionados a qualquer momento ou exportados através dos botões no Dashboard Web para encontrar erros, divergências de protocolo ou falhas intermitentes a posteriori.
