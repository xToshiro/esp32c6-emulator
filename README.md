# ESP32-C6 Modular Hardware & Sensor Emulator

Ambiente de emulação e testes de alta fidelidade para a arquitetura **ESP32-C6 (RISC-V 32-bit @ 160MHz)** e periféricos de sensoriamento do ecossistema de monitoramento ambiental e qualidade do ar.

O emulador reproduz fielmente barramentos de comunicação físicos (**I2C com TCA9548A**, **SPI com Cartão SD**, **UART com GPS NMEA**, **ADC com divisor resistivo de bateria** e **power gating com MCP23008**), permitindo executar e validar a lógica de firmware sem a necessidade de bancada física.

---

## ⚡ Principais Características

- **Execução Estrita em Tempo Real (1.0x)**: Sem aceleração temporal artificial, reproduzindo com fidelidade a cadência real de amostragem, warmup de sensores e ciclo de envio para a nuvem.
- **Arquitetura Modular de Satélites**: Sensores e periféricos são desacoplados em módulos plugáveis que podem ser desconectados ou sofrer falhas a qualquer momento para testes de resiliência.
- **Auditoria de Fundo & Caixa-Preta**: Registro síncrono e contínuo de todas as transições de estado, chamadas de barramento e telemetria na pasta local `logs/`.
- **Dashboard Web Interativo**: Interface visual moderna com controle de falhas de hardware, monitor serial em tempo real, telemetria em nuvem e visualização de atuadores (LED RGB WS2812B).
- **Compilação Híbrida**:
  - **Host C++17 Nativo**: Execução ultrarrápida dos cenários de teste via camada de abstração de hardware (HAL Mock).
  - **Firmware Nativo ESP32-C6**: Compilação de imagens binárias reais (`.bin`, `.elf`, `.factory.bin`) via PlatformIO com ESP-IDF e Arduino Core.

---

## 📁 Estrutura do Projeto

```
.
├── src/                          # Código-fonte principal do firmware (ponto de entrada PlatformIO)
├── hal/                          # Camada de Abstração de Hardware (Mocks C++ para o Host)
│   ├── Arduino.h / .cpp          # Tipos base, GPIO, ADC, Serial, millis(), delay()
│   ├── Wire.h / .cpp             # Barramento I2C simulado com roteamento de endereços
│   ├── SPI.h / SD.h / .cpp       # Barramento SPI e sistema de arquivos SD Card
│   ├── WiFi.h / .cpp             # Pilha Wi-Fi simulada (STA e SoftAP)
│   ├── HTTPClient.h / .cpp       # Interceptador e cliente de telemetria HTTP
│   ├── Preferences.h / .cpp      # Memória não-volátil NVS do ESP32
│   └── freertos/                 # Primitivas FreeRTOS nativas (Queue, Mutex, Tasks)
├── modules/                      # Modelos matemáticos e físicos dos periféricos
│   ├── i2c_bus.h                 # Barramento I2C central com multiplexação
│   ├── tca9548a.h                # Multiplexador I2C (8 canais independentes)
│   ├── mcp23008.h                # Expansor GPIO (gerenciamento de energia de sensores)
│   ├── scd41.h                   # Sensor Sensirion SCD41 (CO2, Temp, Umidade)
│   ├── bme680.h                  # Sensor Bosch BME680 (Pressão, Temp, Umidade, VOC)
│   ├── dfrobot_pm.h              # Sensor de Material Particulado DFRobot (PM1.0, PM2.5, PM10)
│   ├── dfrobot_ozone.h           # Sensor eletroquímico de Ozônio (O3)
│   ├── seeed_multigas.h          # Sensor Multigás GMXXX (CO, NO2, VOC)
│   ├── sound_sensor.h            # Sensor de Ruído Acústico (dB SPL)
│   ├── battery_model.h           # Modelo resistivo de bateria com histerese de corte
│   └── gps_model.h               # Gerador de sentenças NMEA ($GPRMC, $GPGGA) para UART
├── emulator_runner/              # Suítes de testes de integração C++
│   └── test_suites.cpp           # Cenários de validação (I2C, bateria, FreeRTOS, satélites)
├── web_dashboard/                # Dashboard de controle e monitoramento visual
│   ├── server.js                 # Servidor Node.js com streaming SSE e REST API
│   └── public/                   # Interface web com monitor serial e injeção de falhas
├── build.bat                     # Script unificado de compilação e execução
├── platformio.ini                # Configuração do compilador PlatformIO para ESP32-C6
└── min_spiffs.csv                # Tabela de particionamento da memória Flash de 4MB
```

---

## 🔒 Configuração de Segredos & Credenciais

Por motivos de segurança e privacidade, **todas as credenciais reais (Wi-Fi, chaves de API, senhas e identificadores de dispositivos) são isoladas em arquivos privados ignorados pelo Git**.

Para configurar seu ambiente local:

1. **Credenciais para o Firmware C++**:
   Copie o template de exemplo para o arquivo privado:
   ```bash
   cp config_secrets.example.h config_secrets.h
   ```
   Edite `config_secrets.h` preenchendo o seu SSID Wi-Fi, senha e chave de API.

2. **Credenciais para o Web Dashboard (Node.js)**:
   Copie o template JSON:
   ```bash
   cp config_secrets.example.json config_secrets.json
   ```
   Ajuste os parâmetros conforme seu ambiente de teste.

3. **Lista de Dispositivos e Tokens**:
   Copie o modelo de CSV:
   ```bash
   cp MOQAs.example.csv MOQAs.csv
   ```
   Adicione os IDs dos dispositivos e tokens da API que deseja testar.

> [!NOTE]
> Os arquivos `config_secrets.h`, `config_secrets.json` e `MOQAs.csv` estão configurados no `.gitignore` e **nunca** serão incluídos em commits do repositório.

---

## 🚀 Como Executar

O projeto conta com o script de automação unificado `build.bat`:

### 1. Executar os Testes de Integração (C++ Nativo)
Compila os modelos de hardware e executa a suíte de testes de estresse:
```cmd
build.bat test
```
*Valida detecção I2C, comutação de energia MCP23008, histerese de bateria, transbordamento de fila FreeRTOS, fallback de API e injeção de falhas em módulos satélite.*

### 2. Compilar o Firmware ESP32-C6 Nativo (.bin / .elf)
Gera os binários executáveis reais para gravação no chip físico ESP32-C6 via PlatformIO:
```cmd
build.bat compile-bin
```
*Artefatos gerados:*
- `.pio\build\esp32-c6-devkitc-1\firmware.bin`
- `.pio\build\esp32-c6-devkitc-1\firmware.elf`
- `.pio\build\esp32-c6-devkitc-1\firmware.factory.bin`

### 3. Iniciar o Web Dashboard
Inicia o painel visual e a engine de emulação em tempo real na porta `3000`:
```cmd
build.bat dashboard
```
Abra o navegador no endereço: `http://localhost:3000`.

---

## 🛰️ Teste de Falhas em Módulos Satélite

No painel de controle ou via testes automatizados, cada satélite pode ser ligado, desligado ou colocado em estado de falha em tempo de execução:

| Módulo Satélite | Barramento / Interface | Comportamento em Falha |
| :--- | :--- | :--- |
| **GPS** | UART Serial (9600 baud) | Interrompe o fluxo NMEA; firmware preserva última posição válida. |
| **Cartão MicroSD** | SPI Bus | Interrompe operações de leitura/escrita; logs são retidos na memória volátil. |
| **Modem / Wi-Fi** | RF 2.4GHz | Pacotes de telemetria são acumulados na fila FreeRTOS até restauração da rede. |
| **SCD41 (CO2)** | I2C (Endereço 0x62) | Barramento retorna NACK; status de erro de sensor é sinalizado no LED RGB. |
| **BME680 (Clima)** | I2C (Endereço 0x77) | Leitura de pressão e gás suspensa com degradação graciosa. |

---

## 🛠️ Requisitos de Ambiente

- **Windows 10/11** ou **Linux/macOS**
- **Compilador C++17** (GCC / MinGW ou Clang)
- **Node.js** (v18 ou superior)
- **PlatformIO Core (CLI)** *(opcional, necessário apenas para compilar imagens `.bin` para o chip físico)*

---

## 📄 Licença

Consulte os termos de licença definidos no arquivo do projeto.
