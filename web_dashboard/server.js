const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { exec } = require('node:child_process');

const PORT = 3000;
const PUBLIC_DIR = path.join(__dirname, 'public');
const ROOT_DIR = path.join(__dirname, '..');
const SECRETS_PATH = path.join(ROOT_DIR, 'config_secrets.json');
const SECRETS_EXAMPLE_PATH = path.join(ROOT_DIR, 'config_secrets.example.json');
const CSV_PATH = path.join(ROOT_DIR, 'MOQAs.csv');
const CSV_EXAMPLE_PATH = path.join(ROOT_DIR, 'MOQAs.example.csv');

// Load secrets from config_secrets.json or config_secrets.example.json
function loadConfigSecrets() {
    let raw = null;
    if (fs.existsSync(SECRETS_PATH)) {
        raw = fs.readFileSync(SECRETS_PATH, 'utf-8');
    } else if (fs.existsSync(SECRETS_EXAMPLE_PATH)) {
        raw = fs.readFileSync(SECRETS_EXAMPLE_PATH, 'utf-8');
    }
    if (raw) {
        try {
            return JSON.parse(raw);
        } catch (e) {
            console.error('[CONFIG] Erro ao analisar config_secrets.json:', e.message);
        }
    }
    return {
        wifi: { ssid: 'SUA_REDE_WIFI', password: '', ip: '192.168.1.100', rssi: -65 },
        deviceId: 'MOQA-INFRA-001',
        apiKey: 'mqk_live_placeholder',
        apiUrlPrimary: 'https://api.exemplo.gov.br/telemetry',
        apiUrlBackup: 'https://api-backup.exemplo.gov.br/telemetry'
    };
}
const configSecrets = loadConfigSecrets();

// Parse MOQAs.csv or fallback to MOQAs.example.csv
function loadDevicesFromCsv() {
    const devices = [];
    const targetCsv = fs.existsSync(CSV_PATH) ? CSV_PATH : (fs.existsSync(CSV_EXAMPLE_PATH) ? CSV_EXAMPLE_PATH : null);
    try {
        if (targetCsv) {
            const content = fs.readFileSync(targetCsv, 'utf-8');
            const lines = content.split('\n');
            for (let i = 1; i < lines.length; i++) {
                const line = lines[i].trim();
                if (!line) continue;
                const parts = line.split(';');
                if (parts.length >= 2 && parts[0].trim()) {
                    devices.push({
                        id: parts[0].trim(),
                        token: parts[1].trim()
                    });
                }
            }
        }
    } catch (e) {
        console.error('[CSV] Erro ao carregar MOQAs.csv:', e.message);
    }
    return devices;
}

const deviceList = loadDevicesFromCsv();
// Default to the LAST ID in MOQAs.csv (or MOQAs.example.csv), or fallback to configSecrets
const defaultDevice = (deviceList.length > 0) 
    ? deviceList[deviceList.length - 1] 
    : { id: configSecrets.deviceId || 'MOQA-INFRA-001', token: configSecrets.apiKey || 'mqk_live_placeholder' };

// Dedicated Logs Directory for Background Audit & Tracing
const LOGS_DIR = path.join(ROOT_DIR, 'logs');
if (!fs.existsSync(LOGS_DIR)) {
    fs.mkdirSync(LOGS_DIR, { recursive: true });
}
const BG_LOG_PATH = path.join(LOGS_DIR, 'emulator_background_trace.log');
const BG_JSONL_PATH = path.join(LOGS_DIR, 'emulator_background_trace.jsonl');

// Background Audit Recorder (synchronously persists ALL background execution to disk)
function appendBackgroundAudit(subsystem, action, details, payload = null, status = 'OK') {
    const now = new Date();
    const iso = now.toISOString();
    const ms = (emulatorState && emulatorState.uptimeSec) ? (emulatorState.uptimeSec * 1000) : 0;
    const logLine = `[${iso}] [${String(ms).padStart(8, ' ')}ms] [${subsystem.padEnd(16, ' ')}] [${action.padEnd(22, ' ')}] [${status.padEnd(5, ' ')}] ${details}${payload ? ' | Payload: ' + JSON.stringify(payload) : ''}\n`;
    try {
        fs.appendFileSync(BG_LOG_PATH, logLine, 'utf-8');
        const jsonRecord = JSON.stringify({
            timestamp: iso,
            uptimeMs: ms,
            deviceId: emulatorState ? emulatorState.deviceId : 'INIT',
            subsystem,
            action,
            status,
            details,
            payload
        }) + '\n';
        fs.appendFileSync(BG_JSONL_PATH, jsonRecord, 'utf-8');
    } catch (e) {
        console.error('[BG AUDIT] Falha ao persistir em disco:', e.message);
    }
}

// Server-Sent Events subscribers
const sseClients = new Set();
function broadcastSSE(data) {
    const payload = `data: ${JSON.stringify(data)}\n\n`;
    for (const res of sseClients) {
        res.write(payload);
    }
}

// Comprehensive Emulator State
const emulatorState = {
    // Device Config
    deviceId: defaultDevice.id,
    boardId: '3.2',
    codeId: '4.0.0',

    // Simulation Execution Engine (STRICT REAL-TIME 1.0X, ZERO ACCELERATION)
    simRunning: true,
    simSpeed: 1, // Fixed 1.0x Real Time
    uptimeSec: 0,
    mode: 'COLETA', // 'COLETA' or 'CONFIG_AP'
    inWarmup: true,
    sensorError: false,
    wifiConnected: true,
    wifiSSID: (configSecrets.wifi && configSecrets.wifi.ssid) || 'SUA_REDE_WIFI',
    wifiIP: (configSecrets.wifi && configSecrets.wifi.ip) || '192.168.1.100',
    wifiRSSI: (configSecrets.wifi && configSecrets.wifi.rssi !== undefined) ? configSecrets.wifi.rssi : -65,
    freeHeap: 184320,

    // Watchdog Timer (ESP-IDF 15s Watchdog)
    watchdog: {
        timeoutMs: 15000,
        remainingMs: 15000,
        tripsCount: 0,
        lastResetTime: Date.now()
    },

    // Actuators
    rgbLed: { r: 0, g: 0, b: 50, colorName: 'Azul Suave (Warmup)' },
    extLed: false,
    gpsPower: false,

    // Hardware Multiplexer & Expander
    tca9548a: { address: 0x70, channelMask: 0xFF },
    mcp23008: { address: 0x20, gpio: 0xFF, pwrScd: true, pwrBme: true, pwrPm: true, pwrO3: true, pwrGas: true },

    // Injected Sensor Faults (for resilience testing)
    faults: {
        scdFault: false,
        bmeFault: false,
        pmFault: false,
        o3Fault: false,
        gasFault: false,
        apiFault: false,
        taskStall: false,
        memLeak: false
    },

    // Modular Satellite Peripherals (Hot-pluggable Hardware Architecture)
    satellites: {
        gps: { id: 'gps', name: 'GPS NEO-6M', bus: 'UART1 (Pins 18/1)', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE', nmeaActive: true },
        sdcard: { id: 'sdcard', name: 'MicroSD SPI', bus: 'SPI (CS Pin 7)', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        modem: { id: 'modem', name: 'Modem / WiFi', bus: 'RF 2.4GHz / MAC-PHY', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        scd41: { id: 'scd41', name: 'Sensirion SCD41', bus: 'I2C 0x62', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        bme680: { id: 'bme680', name: 'Bosch BME680', bus: 'I2C 0x77', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        pm: { id: 'pm', name: 'Seeed HM3301 (PM)', bus: 'I2C 0x19', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        ozone: { id: 'ozone', name: 'DFRobot Ozone O3', bus: 'I2C 0x73', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' },
        multigas: { id: 'multigas', name: 'Seeed Multigas', bus: 'I2C 0x08', plugged: true, powered: true, fault: 'NONE', status: 'ONLINE' }
    },

    // Sensor Readings
    sensors: {
        co2: 420,
        temp: 25.5,
        hum: 58.0,
        pres: 1013.25,
        gasRes: 24500,
        pm1: 8.5,
        pm25: 14.2,
        pm10: 22.0,
        o3: 35.0,
        co: 1.20,
        no2: 0.05,
        voc: 0.85,
        vBat: 3.85,
        batMode: 'FULL', // FULL, PARTIAL, EMERGENCY
        soundDb: 46.5,
        gpsLat: -3.7442,
        gpsLon: -38.5748,
        gpsFix: true
    },

    // Cloud Telemetry
    telemetry: {
        queueCount: 1,
        maxQueue: 80,
        sequenceNum: 1,
        apiKey: defaultDevice.token,
        apiUrlPrimary: configSecrets.apiUrlPrimary || 'https://api.exemplo.gov.br/telemetry',
        apiUrlBackup: configSecrets.apiUrlBackup || 'https://api-backup.exemplo.gov.br/telemetry',
        lastPayload: null,
        lastHttpCode: 200,
        lastSentTime: new Date().toLocaleTimeString(),
        history: []
    },

    // SD Card
    sdCard: {
        initialized: true,
        recordCount: 1,
        lastRecord: ''
    },

    // Diagnostics & Analytics (A Posteriori Error Finding)
    metrics: {
        totalAcquisitions: 0,
        packetsSent: 0,
        packetsFailed: 0,
        queueDrops: 0,
        sensorErrors: 0,
        watchdogPanics: 0,
        batteryCutoffs: 0,
        anomaliesDetected: 0
    },

    // Event Journal (Historical log for post-mortem analysis)
    diagnosticJournal: [],

    // Time-Series for trend charts
    timeSeries: {
        labels: [],
        co2: [],
        pm25: [],
        temp: [],
        vBat: [],
        freeHeap: []
    },

    // Serial Console logs
    serialLogs: [
        `[BOOT] ESP32-C6 RISC-V 160MHz inicializado com sucesso. ID Padrao: ${defaultDevice.id}`,
        `[CONFIG] Chave de API Vitae carregada: ${defaultDevice.token.substring(0, 15)}...`,
        '[STARTUP] Watchdog configurado (15s). RGB LED e Mutex criados.',
        '[NVS] Preferencias carregadas da particao NVS.',
        '[I2C SCAN] Varredura concluida. 7 Dispositivos detectados (0x08, 0x19, 0x20, 0x62, 0x70, 0x73, 0x77)',
        '[HARDWARE] TCA9548A e MCP23008 configurados. Todos os sensores alimentados.',
        '[SD] Cartao SD montado com sucesso (/datalog.csv pronto).',
        `[WIFI] Conectado a ${(configSecrets.wifi && configSecrets.wifi.ssid) || 'SUA_REDE_WIFI'} (IP: ${(configSecrets.wifi && configSecrets.wifi.ip) || '192.168.1.100'}, RSSI: ${(configSecrets.wifi && configSecrets.wifi.rssi !== undefined) ? configSecrets.wifi.rssi : -65} dBm).`,
        '[NTP] Sincronizado com servidores NTP brasileiros (a.st1.ntp.br).',
        '[STARTUP] Warmup ativo de 5 minutos. LED Azul ativo.'
    ]
};

// Push an initial journal event
recordDiagnosticEvent('INFO', 'BOOT', 'Sistema Inicializado', `Dispositivo ${emulatorState.deviceId} iniciado em Modo Coleta.`, 'Iniciado Warmup de 5 minutos.');

function recordDiagnosticEvent(severity, subsystem, title, details, systemAction) {
    const time = new Date().toLocaleTimeString();
    const event = {
        id: emulatorState.diagnosticJournal.length + 1,
        timestamp: time,
        uptimeSec: emulatorState.uptimeSec,
        severity, // 'CRITICAL', 'ERROR', 'WARN', 'INFO'
        subsystem, // 'I2C', 'BATTERY', 'CLOUD', 'RTOS', 'WATCHDOG', 'STORAGE', 'GPS'
        title,
        details,
        systemAction
    };
    emulatorState.diagnosticJournal.unshift(event);
    if (emulatorState.diagnosticJournal.length > 500) {
        emulatorState.diagnosticJournal.pop();
    }

    if (severity === 'CRITICAL' || severity === 'ERROR') {
        emulatorState.metrics.sensorErrors++;
    } else if (severity === 'WARN') {
        emulatorState.metrics.anomaliesDetected++;
    }

    // Persist to disk audit trail
    appendBackgroundAudit(subsystem, title, `${details} | Ação: ${systemAction || '--'}`, null, severity);

    broadcastSSE({ type: 'journalEvent', event });
}

function updateTelemetryPayload() {
    const s = emulatorState.sensors;
    const payload = {
        sequence_number: emulatorState.telemetry.sequenceNum,
        moqa_id: emulatorState.deviceId,
        timestamp: Math.floor(Date.now() / 1000),
        latitude: s.gpsLat,
        longitude: s.gpsLon,
        pm1: s.pm1,
        pm25: s.pm25,
        pm10: s.pm10,
        int_temp: s.temp,
        hum: Math.round(s.hum),
        pres: s.pres,
        co: s.co,
        no2: s.no2,
        voc: s.voc,
        o3: s.o3,
        co2: s.co2,
        noise: s.soundDb,
        board_id: emulatorState.boardId,
        code_id: emulatorState.codeId,
        uptime: emulatorState.uptimeSec,
        firmware_version: emulatorState.codeId,
        battery_level_mv: Math.round(s.vBat * 1000),
        rssi: emulatorState.wifiRSSI,
        sended_packets: emulatorState.telemetry.sequenceNum
    };
    emulatorState.telemetry.lastPayload = payload;
}

function evaluateBatteryMode() {
    const v = emulatorState.sensors.vBat;
    let oldMode = emulatorState.sensors.batMode;
    let newMode = oldMode;

    if (oldMode === 'FULL') {
        if (v < 3.20) newMode = 'EMERGENCY';
        else if (v < 3.50) newMode = 'PARTIAL';
    } else if (oldMode === 'PARTIAL') {
        if (v < 3.20) newMode = 'EMERGENCY';
        else if (v >= 3.55) newMode = 'FULL';
    } else if (oldMode === 'EMERGENCY') {
        if (v >= 3.55) newMode = 'FULL';
        else if (v >= 3.25) newMode = 'PARTIAL';
    }

    emulatorState.sensors.batMode = newMode;
    if (newMode === 'FULL') {
        emulatorState.mcp23008.pwrPm = true;
        emulatorState.mcp23008.pwrGas = true;
        emulatorState.mcp23008.pwrO3 = true;
    } else if (newMode === 'PARTIAL') {
        emulatorState.mcp23008.pwrPm = false;
        emulatorState.mcp23008.pwrGas = false;
        emulatorState.mcp23008.pwrO3 = true;
    } else if (newMode === 'EMERGENCY') {
        emulatorState.mcp23008.pwrPm = false;
        emulatorState.mcp23008.pwrGas = false;
        emulatorState.mcp23008.pwrO3 = false;
        emulatorState.metrics.batteryCutoffs++;
    }

    if (oldMode !== newMode) {
        logSerial(`[POWER] Modo de Bateria alterado para: ${newMode} (VBat: ${v.toFixed(2)}V).`);
        const sev = (newMode === 'EMERGENCY') ? 'ERROR' : (newMode === 'PARTIAL' ? 'WARN' : 'INFO');
        recordDiagnosticEvent(
            sev, 
            'BATTERY', 
            `Transição para modo ${newMode}`, 
            `Tensão da bateria medida em ${v.toFixed(2)}V (limiares: 3.50V / 3.20V + histerese 0.05V).`, 
            (newMode === 'EMERGENCY') ? 'Trilhos PM, Gas e Ozonio desligados via MCP23008.' : (newMode === 'PARTIAL' ? 'Trilhos PM e Gas desligados.' : 'Todos os trilhos restabelecidos.')
        );
    }
}

function updateLedColor() {
    if (emulatorState.mode === 'CONFIG_AP') {
        emulatorState.rgbLed = { r: 50, g: 30, b: 0, colorName: 'Amarelo Suave (Modo AP Config)' };
    } else if (emulatorState.inWarmup) {
        emulatorState.rgbLed = { r: 0, g: 0, b: 50, colorName: 'Azul Suave (Warmup)' };
    } else if (emulatorState.sensorError) {
        emulatorState.rgbLed = { r: 80, g: 0, b: 0, colorName: 'Vermelho Suave (Falha de Sensor)' };
    } else if (!emulatorState.wifiConnected) {
        emulatorState.rgbLed = { r: 50, g: 12, b: 0, colorName: 'Laranja Suave (Desconectado)' };
    } else {
        emulatorState.rgbLed = { r: 0, g: 0, b: 50, colorName: 'Azul Suave (Normal / Coleta)' };
    }
}

function logSerial(msg) {
    const time = new Date().toLocaleTimeString();
    emulatorState.serialLogs.push(`[${time}] ${msg}`);
    if (emulatorState.serialLogs.length > 300) {
        emulatorState.serialLogs.shift();
    }
    appendBackgroundAudit('SERIAL_UART', 'PRINT', msg);
    broadcastSSE({ type: 'serial', message: `[${time}] ${msg}` });
}

// Reset / Reboot function
function rebootEmulator(reason = 'Reinicio solicitado') {
    logSerial(`[RESET] *** REINICIANDO ESP32-C6: ${reason} ***`);
    recordDiagnosticEvent('WARN', 'RTOS', 'Reinicio do ESP32-C6', reason, 'Recarregando NVS e reiniciando barramentos.');
    emulatorState.uptimeSec = 0;
    emulatorState.inWarmup = true;
    emulatorState.watchdog.remainingMs = emulatorState.watchdog.timeoutMs;
    emulatorState.watchdog.lastResetTime = Date.now();
    updateLedColor();
    logSerial(`[BOOT] ESP32-C6 RISC-V 160MHz inicializado com sucesso. ID Padrao: ${emulatorState.deviceId}`);
    logSerial(`[CONFIG] Chave de API Vitae carregada: ${emulatorState.telemetry.apiKey.substring(0, 15)}...`);
    logSerial('[STARTUP] Watchdog configurado (15s). RGB LED e Mutex criados.');
    logSerial('[NVS] Preferencias carregadas da particao NVS.');
    logSerial('[I2C SCAN] Varredura concluida. 7 Dispositivos detectados (0x08, 0x19, 0x20, 0x62, 0x70, 0x73, 0x77)');
    logSerial('[HARDWARE] TCA9548A e MCP23008 configurados. Todos os sensores alimentados.');
    logSerial('[SD] Cartao SD montado com sucesso (/datalog.csv pronto).');
    logSerial(`[WIFI] Conectado a ${emulatorState.wifiSSID} (IP: ${emulatorState.wifiIP}, RSSI: ${emulatorState.wifiRSSI} dBm).`);
    logSerial('[NTP] Sincronizado com servidores NTP brasileiros (a.st1.ntp.br).');
    logSerial('[STARTUP] Warmup ativo de 5 minutos. LED Azul ativo.');
    broadcastSSE({ type: 'state', state: emulatorState });
    broadcastSSE({ type: 'rgbLed', led: emulatorState.rgbLed });
}

// Main Simulation Heartbeat (Runs strictly every 1000ms in REAL-TIME 1.0X, NO ACCELERATION)
setInterval(() => {
    if (!emulatorState.simRunning) return;

    // Strict 1:1 real-world timing cadence
    emulatorState.simSpeed = 1;
    emulatorState.uptimeSec += 1;

    // Watchdog simulation (15s hardware watchdog)
    if (emulatorState.faults.taskStall) {
        emulatorState.watchdog.remainingMs -= 1000;
        if (emulatorState.watchdog.remainingMs <= 0) {
            emulatorState.watchdog.tripsCount++;
            emulatorState.metrics.watchdogPanics++;
            recordDiagnosticEvent('CRITICAL', 'WATCHDOG', 'Watchdog Panic Timeout', 'Tarefa travada por mais de 15 segundos sem alimentar o WDT (esp_task_wdt_reset).', 'Panico disparado! Forcando reinicio do processador RISC-V.');
            rebootEmulator('Watchdog Panic (>15s timeout)');
            return;
        }
    } else {
        // Normal watchdog reset in loop
        emulatorState.watchdog.remainingMs = emulatorState.watchdog.timeoutMs;
    }

    // Memory leak simulation
    if (emulatorState.faults.memLeak) {
        emulatorState.freeHeap -= 5000;
        if (emulatorState.freeHeap < 20000) {
            recordDiagnosticEvent('CRITICAL', 'RTOS', 'Esgotamento de Memoria RAM', `Memoria livre caiu para ${emulatorState.freeHeap} bytes (< 20000).`, 'Protecao critica: ESP.restart() automatico acionado.');
            emulatorState.freeHeap = 184320;
            rebootEmulator('Memoria RAM baixa (< 20KB)');
            return;
        }
    }

    // Check for sensor fault flags
    const hasFault = emulatorState.faults.scdFault || emulatorState.faults.bmeFault || 
                     emulatorState.faults.pmFault || emulatorState.faults.o3Fault || emulatorState.faults.gasFault;
    if (hasFault !== emulatorState.sensorError) {
        emulatorState.sensorError = hasFault;
        updateLedColor();
        if (hasFault) {
            recordDiagnosticEvent('ERROR', 'I2C', 'Falha de Comunicacao I2C', 'Sensor retornou NACK no barramento.', 'LED Vermelho Suave ativado; valores substituidos por 9999.0.');
        } else {
            recordDiagnosticEvent('INFO', 'I2C', 'Barramento I2C Normalizado', 'Todos os sensores responderam com ACK.', 'LED restaurado para o padrao.');
        }
    }

    // Periodic acquisition emulation (every 5 seconds in real time)
    if (emulatorState.uptimeSec % 5 === 0 && emulatorState.mode === 'COLETA') {
        emulatorState.metrics.totalAcquisitions++;
        emulatorState.extLed = true;
        broadcastSSE({ type: 'extLed', state: true });
        setTimeout(() => { 
            emulatorState.extLed = false; 
            broadcastSSE({ type: 'extLed', state: false }); 
        }, 350);

        // Add slight realistic sensor noise
        const s = emulatorState.sensors;
        s.temp = +(s.temp + (Math.random() * 0.1 - 0.05)).toFixed(2);
        s.hum = +(Math.min(100, Math.max(0, s.hum + (Math.random() * 0.2 - 0.1)))).toFixed(1);
        s.pres = +(s.pres + (Math.random() * 0.04 - 0.02)).toFixed(2);
        
        // Output live serial log for each sensor acquisition cycle
        logSerial(`[SENSOR] Coleta #${emulatorState.metrics.totalAcquisitions}: CO2=${s.co2}ppm | PM2.5=${s.pm25}µg/m³ | T=${s.temp}°C | UR=${s.hum}% | VBat=${s.vBat}V`);

        // Log to simulated SD card (if satellite is plugged & initialized)
        if (emulatorState.sdCard.initialized) {
            const timeNow = Math.floor(Date.now() / 1000);
            emulatorState.sdCard.recordCount++;
            emulatorState.sdCard.lastRecord = `${timeNow},${s.pm1},${s.pm25},${s.pm10},${s.temp},${s.hum},${s.pres},${s.o3},${s.co},${s.voc},${s.co2},${s.soundDb},${s.vBat}`;
            
            // Persist physically to sim_sdcard/datalog.csv on disk
            try {
                const sdDir = path.join(ROOT_DIR, 'sim_sdcard');
                if (!fs.existsSync(sdDir)) {
                    fs.mkdirSync(sdDir, { recursive: true });
                }
                const sdFilePath = path.join(sdDir, 'datalog.csv');
                fs.appendFileSync(sdFilePath, emulatorState.sdCard.lastRecord + '\n', 'utf-8');
            } catch (err) {
                console.error('[SD] Falha ao persistir em sim_sdcard/datalog.csv:', err.message);
            }

            appendBackgroundAudit('STORAGE_SD', 'CSV_APPEND', `Linha gravada no /datalog.csv`, { record: emulatorState.sdCard.lastRecord });
        } else {
            appendBackgroundAudit('STORAGE_SD', 'WRITE_SKIPPED', `Escrita no SD Card ignorada: módulo MicroSD desconectado ou ejetado`, null, 'WARN');
        }

        appendBackgroundAudit('SENSOR_ACQ', 'SAMPLE_ACQUIRED', `Amostra de sensores coletada com sucesso (Acq #${emulatorState.metrics.totalAcquisitions})`, {
            co2: s.co2, pm25: s.pm25, temp: s.temp, hum: s.hum, pres: s.pres, vBat: s.vBat
        });

        // Anomaly threshold checks
        if (s.pm25 > 150) {
            recordDiagnosticEvent('WARN', 'I2C', 'Anomalia: PM2.5 Critico', `Concentracao de PM2.5 atingiu ${s.pm25} ug/m3.`, 'Alerta de qualidade do ar.');
        }
        if (s.co > 10) {
            recordDiagnosticEvent('WARN', 'I2C', 'Anomalia: CO Elevado', `Monoxido de carbono medido em ${s.co} ppm.`, 'Alerta de gas perigoso.');
        }

        // Time-series recording
        const timeLabel = new Date().toLocaleTimeString();
        emulatorState.timeSeries.labels.push(timeLabel);
        emulatorState.timeSeries.co2.push(s.co2);
        emulatorState.timeSeries.pm25.push(s.pm25);
        emulatorState.timeSeries.temp.push(s.temp);
        emulatorState.timeSeries.vBat.push(s.vBat);
        emulatorState.timeSeries.freeHeap.push(emulatorState.freeHeap);
        if (emulatorState.timeSeries.labels.length > 50) {
            emulatorState.timeSeries.labels.shift();
            emulatorState.timeSeries.co2.shift();
            emulatorState.timeSeries.pm25.shift();
            emulatorState.timeSeries.temp.shift();
            emulatorState.timeSeries.vBat.shift();
            emulatorState.timeSeries.freeHeap.shift();
        }

        updateTelemetryPayload();
    }

    // Warmup auto-finish at 300s (5 minutes in real time)
    if (emulatorState.inWarmup && emulatorState.uptimeSec >= 300) {
        emulatorState.inWarmup = false;
        logSerial('[STARTUP] Warmup de 5 minutos concluido em tempo real.');
        recordDiagnosticEvent('INFO', 'RTOS', 'Fim do Warmup', 'Tempo limite de 5 minutos atingido. Iniciando gravacao no SD e consolidacao.', 'LED Azul Suave mantido para Coleta Normal.');
        updateLedColor();
        broadcastSSE({ type: 'state', state: emulatorState });
    }

    // Periodic consolidated cloud send (every 15s in real time)
    if (emulatorState.uptimeSec % 15 === 0 && emulatorState.mode === 'COLETA' && !emulatorState.inWarmup) {
        dispatchCloudPacket();
    }
}, 1000);

function dispatchCloudPacket() {
    updateTelemetryPayload();
    const payload = emulatorState.telemetry.lastPayload;

    if (!emulatorState.wifiConnected) {
        logSerial('[API] WiFi desconectado. Pacote retido na fila RTOS.');
        if (emulatorState.telemetry.queueCount >= emulatorState.telemetry.maxQueue) {
            emulatorState.metrics.queueDrops++;
            recordDiagnosticEvent('WARN', 'RTOS', 'Fila RTOS Cheia (80 pacotes)', 'Fila de telemetria lotada (>20h). Pacote mais antigo descartado para dar lugar ao novo.', 'xQueueReceive descartou item antigo e xQueueSend inseriu pacote atual.');
        } else {
            emulatorState.telemetry.queueCount++;
        }
        return;
    }

    if (emulatorState.faults.apiFault) {
        // Primary API Failure -> Trigger Fallback
        emulatorState.metrics.packetsFailed++;
        emulatorState.telemetry.lastHttpCode = 500;
        logSerial(`[API] Falha no envio para ${emulatorState.telemetry.apiUrlPrimary} (HTTP 500). Tentando API Backup...`);
        recordDiagnosticEvent('WARN', 'CLOUD', 'Falha na API Primaria (HTTP 500)', 'Servidor principal inalcancavel ou com erro interno.', `Ativando failover para URL de Backup: ${emulatorState.telemetry.apiUrlBackup}`);

        // Send via backup with 200 OK
        emulatorState.telemetry.lastHttpCode = 200;
        emulatorState.metrics.packetsSent++;
        logSerial(`[API] Telemetria enviada com sucesso na API de Backup: ${emulatorState.telemetry.apiUrlBackup} (HTTP 200)`);
    } else {
        // Normal send
        emulatorState.metrics.packetsSent++;
        emulatorState.telemetry.lastHttpCode = 200;
        logSerial(`[API] Telemetria enviada com sucesso para ${emulatorState.telemetry.apiUrlPrimary} (HTTP 200, Seq: ${payload.sequence_number})`);
    }

    appendBackgroundAudit('CLOUD_TELEMETRY', 'HTTP_POST_DISPATCH', `Pacote #${payload.sequence_number} transmitido via HTTP POST`, {
        targetUrl: emulatorState.faults.apiFault ? emulatorState.telemetry.apiUrlBackup : emulatorState.telemetry.apiUrlPrimary,
        httpCode: emulatorState.telemetry.lastHttpCode,
        sequenceNum: payload.sequence_number
    });

    // Flash green
    emulatorState.rgbLed = { r: 0, g: 255, b: 0, colorName: 'Verde Brilhante (Envio Cloud OK)' };
    broadcastSSE({ type: 'rgbLed', led: emulatorState.rgbLed });
    setTimeout(() => {
        updateLedColor();
        broadcastSSE({ type: 'rgbLed', led: emulatorState.rgbLed });
    }, 600);

    emulatorState.telemetry.sequenceNum++;
    emulatorState.telemetry.lastSentTime = new Date().toLocaleTimeString();
    emulatorState.telemetry.history.unshift({
        seq: payload.sequence_number,
        time: emulatorState.telemetry.lastSentTime,
        pm25: payload.pm25,
        co2: payload.co2,
        temp: payload.int_temp,
        vBat: (payload.battery_level_mv / 1000).toFixed(2),
        code: emulatorState.telemetry.lastHttpCode
    });
    if (emulatorState.telemetry.history.length > 25) emulatorState.telemetry.history.pop();

    broadcastSSE({ type: 'telemetry', telemetry: emulatorState.telemetry });
    broadcastSSE({ type: 'metrics', metrics: emulatorState.metrics });
}

// HTTP Server
const server = http.createServer((req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        return res.end();
    }

    const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
    const pathname = parsedUrl.pathname;

    // SSE Stream
    if (pathname === '/events') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write(`data: ${JSON.stringify({ 
            type: 'init', 
            state: emulatorState,
            devices: deviceList
        })}\n\n`);
        sseClients.add(res);
        req.on('close', () => sseClients.delete(res));
        return;
    }

    // API: Get State
    if (pathname === '/api/state' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(emulatorState));
    }

    // API: Get Device List
    if (pathname === '/api/devices' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({
            currentId: emulatorState.deviceId,
            devices: deviceList
        }));
    }

    // API: Set Active Device ID
    if (pathname === '/api/device' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { id } = JSON.parse(body);
                const match = deviceList.find(d => d.id === id);
                if (match) {
                    emulatorState.deviceId = match.id;
                    emulatorState.telemetry.apiKey = match.token;
                    logSerial(`[DEVICE] Dispositivo alterado para: ${match.id} (Token: ${match.token.substring(0, 15)}...)`);
                    recordDiagnosticEvent('INFO', 'RTOS', 'Dispositivo Alterado', `Dispositivo ativo modificado para ${match.id}.`, 'Token e cabeçalho X-API-Key atualizados.');
                    updateTelemetryPayload();
                    broadcastSSE({ type: 'state', state: emulatorState });
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    return res.end(JSON.stringify({ success: true, deviceId: match.id }));
                }
                res.writeHead(404, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Device ID nao encontrado no MOQAs.csv' }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Simulation Control (Play/Pause/Reset - Speed locked to strict 1.0x Real-Time)
    if (pathname === '/api/sim-control' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { action } = JSON.parse(body);
                if (action === 'toggle_play') {
                    emulatorState.simRunning = !emulatorState.simRunning;
                    logSerial(`[SIMULATION] Execucao ${emulatorState.simRunning ? 'Retomada' : 'Pausada'}.`);
                    appendBackgroundAudit('SIM_ENGINE', 'RUN_STATE_TOGGLE', `Simulacao ${emulatorState.simRunning ? 'Retomada' : 'Pausada'}`);
                } else if (action === 'set_speed') {
                    emulatorState.simSpeed = 1; // Strict 1.0x real time, zero artificial acceleration
                    logSerial(`[SIMULATION] Operação fixada em Tempo Real 1.0x (Cadência física estrita de sensores e RTOS).`);
                    appendBackgroundAudit('SIM_ENGINE', 'SET_SPEED_REJECT', 'Tentativa de alteracao de velocidade rejeitada: Emulador opera em estrito tempo real 1.0x');
                } else if (action === 'reset') {
                    rebootEmulator('Reinicio manual pelo usuario via Dashboard');
                }
                broadcastSSE({ type: 'state', state: emulatorState });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, state: emulatorState }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Invert / Toggle Faults
    if (pathname === '/api/fault' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { fault, state } = JSON.parse(body);
                if (emulatorState.faults[fault] !== undefined) {
                    emulatorState.faults[fault] = (state !== undefined) ? state : !emulatorState.faults[fault];
                    logSerial(`[FAULT INJECTION] Falha '${fault}' ${emulatorState.faults[fault] ? 'ATIVADA' : 'DESATIVADA'}`);
                    
                    if (fault === 'taskStall' && emulatorState.faults.taskStall) {
                        recordDiagnosticEvent('WARN', 'RTOS', 'Bloqueio de Tarefa Injetado', 'A simulacao congelou a loopTask para testar o Watchdog.', 'WDT vai estourar em 15 segundos se nao for cancelado.');
                    } else if (fault === 'apiFault' && emulatorState.faults.apiFault) {
                        recordDiagnosticEvent('WARN', 'CLOUD', 'Falha na API Injetada', 'Todas as requisicoes a URL principal retornarao HTTP 500.', 'Testando chaveamento para URL backup.');
                    }

                    broadcastSSE({ type: 'state', state: emulatorState });
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    return res.end(JSON.stringify({ success: true, faults: emulatorState.faults }));
                }
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Falha desconhecida' }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // Satellite Hardware State Machine & Dynamic Fault Handler
    function handleSatelliteStateChange(id, sat) {
        if (id === 'gps') {
            if (!sat.plugged || sat.fault === 'DISCONNECTED') {
                sat.status = 'DESCONECTADO';
                sat.nmeaActive = false;
                emulatorState.sensors.gpsFix = false;
                logSerial(`[SATELLITE] Módulo ${sat.name} DESCONECTADO (UART1 RX Pino 18 em Hi-Z, fluxo NMEA interrompido).`);
                recordDiagnosticEvent('WARN', 'GPS', 'GPS Desconectado', 'Módulo GPS desacoplado fisicamente.', 'Sinal NMEA ($GPRMC/$GPGGA) congelado, flag de Fix GPS desativada.');
                appendBackgroundAudit('SATELLITE_GPS', 'UNPLUG', 'Módulo GPS desacoplado', { id, plugged: false });
            } else if (sat.fault === 'NO_FIX') {
                sat.status = 'FALHA';
                sat.nmeaActive = true;
                emulatorState.sensors.gpsFix = false;
                logSerial(`[SATELLITE] Módulo ${sat.name} operando sem Fix ($GPGGA fix_quality=0).`);
                recordDiagnosticEvent('WARN', 'GPS', 'GPS Sem Fix', 'Módulo transmitindo sentenças NMEA sem fix de satélites.', 'Tentando obter lock.');
                appendBackgroundAudit('SATELLITE_GPS', 'FAULT_NO_FIX', 'GPS sem fix válido', { id });
            } else {
                sat.status = 'ONLINE';
                sat.nmeaActive = true;
                sat.fault = 'NONE';
                emulatorState.sensors.gpsFix = true;
                logSerial(`[SATELLITE] Módulo ${sat.name} RECONECTADO (UART1 ativa, sentenças NMEA 1Hz com 3D Fix).`);
                recordDiagnosticEvent('INFO', 'GPS', 'GPS Reconectado', 'Módulo GPS reinserido e sincronizado.', 'Sinal NMEA restaurado com Fix 3D.');
                appendBackgroundAudit('SATELLITE_GPS', 'PLUG', 'Módulo GPS reconectado', { id, plugged: true });
            }
        } else if (id === 'sdcard') {
            if (!sat.plugged || sat.fault === 'EJECTED') {
                sat.status = 'DESCONECTADO';
                emulatorState.sdCard.initialized = false;
                logSerial(`[SATELLITE] Módulo ${sat.name} EJETADO / DESCONECTADO (Barramento SPI CS Pino 7 inativo).`);
                recordDiagnosticEvent('ERROR', 'SD', 'Cartão SD Ejetado', 'Cartão de memória removido.', 'Gravação contínua em /datalog.csv interrompida.');
                appendBackgroundAudit('SATELLITE_SD', 'UNPLUG', 'Cartão SD ejetado', { id, plugged: false });
            } else if (sat.fault === 'IO_ERROR') {
                sat.status = 'FALHA';
                emulatorState.sdCard.initialized = false;
                logSerial(`[SATELLITE] Módulo ${sat.name} FALHA DE I/O (Setor corrompido ou erro de gravação).`);
                recordDiagnosticEvent('ERROR', 'SD', 'Erro I/O Cartão SD', 'Falha ao acessar partição FAT32.', 'Setor inacessível no barramento SPI.');
                appendBackgroundAudit('SATELLITE_SD', 'FAULT_IO', 'Erro de IO no SD', { id });
            } else {
                sat.status = 'ONLINE';
                sat.fault = 'NONE';
                emulatorState.sdCard.initialized = true;
                logSerial(`[SATELLITE] Módulo ${sat.name} REINSERIDO / MONTADO com sucesso (/datalog.csv ativo).`);
                recordDiagnosticEvent('INFO', 'SD', 'Cartão SD Montado', 'Mídia de armazenamento reinserida.', 'Sistema de arquivos FAT pronto para datalogging.');
                appendBackgroundAudit('SATELLITE_SD', 'PLUG', 'Cartão SD montado', { id, plugged: true });
            }
        } else if (id === 'modem') {
            if (!sat.plugged || sat.fault === 'OFFLINE') {
                sat.status = 'DESCONECTADO';
                emulatorState.wifiConnected = false;
                updateLedColor();
                logSerial(`[SATELLITE] Módulo ${sat.name} DESCONECTADO (Link de rádio RF / TCP inativo).`);
                recordDiagnosticEvent('ERROR', 'MODEM', 'Modem / Rede Desconectado', 'Sem conectividade com gateway ou nuvem.', 'Pacotes de telemetria acumulando na fila FreeRTOS.');
                appendBackgroundAudit('SATELLITE_MODEM', 'UNPLUG', 'Modem desconectado', { id, plugged: false });
            } else if (sat.fault === 'HTTP_500_FAILOVER') {
                sat.status = 'FALHA';
                emulatorState.wifiConnected = true;
                emulatorState.faults.apiFault = true;
                logSerial(`[SATELLITE] Módulo ${sat.name} FALHA NA API PRIMÁRIA (Simulando HTTP 500 Failover).`);
                recordDiagnosticEvent('WARN', 'MODEM', 'Falha API Primária', 'Servidor primário retornando HTTP 500.', 'Testando chaveamento automático para URL de backup.');
                appendBackgroundAudit('SATELLITE_MODEM', 'FAULT_HTTP_500', 'Falha API 500 failover', { id });
            } else {
                sat.status = 'ONLINE';
                sat.fault = 'NONE';
                emulatorState.wifiConnected = true;
                emulatorState.faults.apiFault = false;
                updateLedColor();
                logSerial(`[SATELLITE] Módulo ${sat.name} RECONECTADO (Link RF restabelecido).`);
                recordDiagnosticEvent('INFO', 'MODEM', 'Modem Reconectado', 'Conexão IP restabelecida com gateway.', 'Descarregando fila de telemetria.');
                appendBackgroundAudit('SATELLITE_MODEM', 'PLUG', 'Modem reconectado', { id, plugged: true });
            }
        } else {
            // I2C Sensors (scd41, bme680, pm, ozone, multigas)
            const faultMap = {
                scd41: 'scdFault',
                bme680: 'bmeFault',
                pm: 'pmFault',
                ozone: 'o3Fault',
                multigas: 'gasFault'
            };
            const faultKey = faultMap[id];
            if (!sat.plugged) {
                sat.status = 'DESCONECTADO';
                if (faultKey) emulatorState.faults[faultKey] = true;
                emulatorState.metrics.sensorErrors++;
                logSerial(`[SATELLITE] Sensor ${sat.name} DESCONECTADO (Barramento ${sat.bus} retornou I2C NACK).`);
                recordDiagnosticEvent('ERROR', 'I2C', `Sensor ${sat.name} Desconectado`, `Dispositivo ausente no barramento ${sat.bus}.`, 'Leitura substituída por código de falha / congelada.');
                appendBackgroundAudit('SATELLITE_I2C', 'UNPLUG', `Sensor ${sat.name} desacoplado`, { id, bus: sat.bus });
            } else {
                sat.status = 'ONLINE';
                sat.fault = 'NONE';
                if (faultKey) emulatorState.faults[faultKey] = false;
                logSerial(`[SATELLITE] Sensor ${sat.name} RECONECTADO (Barramento ${sat.bus} respondeu I2C ACK).`);
                recordDiagnosticEvent('INFO', 'I2C', `Sensor ${sat.name} Reconectado`, `Dispositivo no barramento ${sat.bus} detectado.`, 'Leituras em tempo real retomadas com sucesso.');
                appendBackgroundAudit('SATELLITE_I2C', 'PLUG', `Sensor ${sat.name} reconectado`, { id, bus: sat.bus });
            }
        }
    }

    // API: Toggle Satellite Module State / Fault
    if (pathname === '/api/satellite/toggle' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { id, plugged, fault } = JSON.parse(body);
                const sat = emulatorState.satellites[id];
                if (!sat) {
                    res.writeHead(404, { 'Content-Type': 'application/json' });
                    return res.end(JSON.stringify({ error: 'Módulo satélite não encontrado' }));
                }

                if (plugged !== undefined) sat.plugged = plugged;
                if (fault !== undefined) sat.fault = fault;

                handleSatelliteStateChange(id, sat);

                broadcastSSE({ type: 'state', state: emulatorState });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, satellite: sat, satellites: emulatorState.satellites }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Update Sensor Readings
    if (pathname === '/api/sensor' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const data = JSON.parse(body);
                Object.assign(emulatorState.sensors, data);
                if (data.vBat !== undefined) evaluateBatteryMode();
                updateTelemetryPayload();
                logSerial(`[SENSOR INJECTION] Sensores atualizados: ${JSON.stringify(data)}`);
                broadcastSSE({ type: 'sensors', sensors: emulatorState.sensors });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, sensors: emulatorState.sensors }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Run Scenario
    if (pathname === '/api/scenario' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { scenario } = JSON.parse(body);
                const s = emulatorState.sensors;
                switch (scenario) {
                    case 'clean_air':
                        s.pm1 = 3.2; s.pm25 = 6.4; s.pm10 = 10.0;
                        s.co2 = 412; s.o3 = 20; s.co = 0.4; s.no2 = 0.02; s.voc = 0.3;
                        recordDiagnosticEvent('INFO', 'I2C', 'Cenario: Ar Limpo', 'Condicoes de ar limpo injetadas.', 'Parametros ambientais estaveis.');
                        logSerial('[SCENARIO] Cenario aplicado: Ar Limpo / Pristine Ambient');
                        break;
                    case 'severe_pollution':
                        s.pm1 = 65.0; s.pm25 = 145.8; s.pm10 = 210.0;
                        s.co2 = 890; s.o3 = 95; s.co = 6.5; s.no2 = 0.45; s.voc = 3.2;
                        recordDiagnosticEvent('WARN', 'I2C', 'Cenario: Poluicao Severa', 'PM2.5 (145.8 ug/m3) e Ozonio (95 ppb) elevados.', 'Disparo de alerta ambiental.');
                        logSerial('[SCENARIO] Cenario aplicado: ALERTA Poluicao Severa');
                        break;
                    case 'battery_critical':
                        s.vBat = 3.05;
                        evaluateBatteryMode();
                        logSerial('[SCENARIO] Cenario aplicado: Bateria Critica 3.05V -> Modo EMERGENCY');
                        break;
                    case 'battery_full':
                        s.vBat = 4.05;
                        evaluateBatteryMode();
                        logSerial('[SCENARIO] Cenario aplicado: Bateria Cheia 4.05V -> Modo FULL');
                        break;
                    case 'cloud_offline':
                        emulatorState.wifiConnected = false;
                        updateLedColor();
                        recordDiagnosticEvent('WARN', 'CLOUD', 'Cenario: Desconexao de Rede', 'WiFi STA desconectado.', 'Retencao de telemetria na fila RTOS.');
                        logSerial('[SCENARIO] Cenario aplicado: Queda de WiFi / Conexao Cloud');
                        break;
                    case 'cloud_restore':
                        emulatorState.wifiConnected = true;
                        updateLedColor();
                        recordDiagnosticEvent('INFO', 'CLOUD', 'Cenario: Conexao Restaurada', 'WiFi reconectado.', 'Descarregando pacotes pendentes.');
                        logSerial('[SCENARIO] Cenario aplicado: Conexao WiFi / Cloud restabelecida');
                        break;
                    case 'skip_warmup':
                        emulatorState.inWarmup = false;
                        updateLedColor();
                        recordDiagnosticEvent('INFO', 'RTOS', 'Warmup Pulado', 'Clique curto no botao CFG.', 'Iniciando operacao normal imediatamente.');
                        logSerial('[BOTAO] Clique curto detectado (< 2s). Warmup pulado pelo usuario.');
                        break;
                    case 'toggle_ap_mode':
                        if (emulatorState.mode === 'COLETA') {
                            emulatorState.mode = 'CONFIG_AP';
                            recordDiagnosticEvent('INFO', 'WIFI', 'Modo AP Ativado', `Rede SoftAP MOQA_Config_${emulatorState.deviceId.slice(-6)} ativa.`, 'Acesse em http://192.168.4.1.');
                            logSerial('[BOTAO] Botao pressionado por 5s! Reiniciando em Modo AP...');
                        } else {
                            emulatorState.mode = 'COLETA';
                            logSerial('[AP] Reiniciando em Modo Coleta...');
                        }
                        updateLedColor();
                        break;
                    default:
                        break;
                }
                updateTelemetryPayload();
                broadcastSSE({ type: 'state', state: emulatorState });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, state: emulatorState }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Run Serial Command
    if (pathname === '/api/serial' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const { command } = JSON.parse(body);
                logSerial(`> ${command}`);
                const cmd = (command || '').trim().toLowerCase();
                if (cmd === 'c' || cmd === 'config' || cmd === 'ap') {
                    emulatorState.mode = 'CONFIG_AP';
                    updateLedColor();
                    logSerial('[SERIAL] Comando recebido. Alternando para Modo AP...');
                } else if (cmd === 'e' || cmd === 'exit' || cmd === 'coleta') {
                    emulatorState.mode = 'COLETA';
                    updateLedColor();
                    logSerial('[SERIAL] Comando recebido. Alternando para Modo Coleta...');
                } else if (cmd.startsWith('http ')) {
                    const target = command.substring(5).trim();
                    logSerial(`[DEBUG] Efetuando GET simulado para: ${target}`);
                    logSerial('[DEBUG] Resposta HTTP: 200 OK');
                } else {
                    logSerial(`[SERIAL] Comando desconhecido: "${command}". Comandos: config, coleta, exit, http <url>`);
                }
                broadcastSSE({ type: 'state', state: emulatorState });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
        });
        return;
    }

    // API: Run C++ Integration Test Suite (Saves full background audit trail)
    if (pathname === '/api/run-tests' && req.method === 'POST') {
        logSerial('[TEST RUNNER] Executando suite nativa C++ em Tempo Real 1.0x (test_suites.exe)...');
        appendBackgroundAudit('TEST_RUNNER', 'START_SUITE', 'Iniciando execucao da suite de testes C++ com auditoria completa de fundo');
        const testExe = path.join(__dirname, '..', 'emulator_runner', 'test_suites.exe');
        exec(`"${testExe}"`, (error, stdout, stderr) => {
            const output = stdout || stderr || (error ? error.message : '');
            logSerial('[TEST RUNNER] Suite de testes concluida. Trilha de auditoria salva em logs/latest_test_execution.log.');
            recordDiagnosticEvent(error ? 'ERROR' : 'INFO', 'RTOS', 'Suite C++ Executada', error ? 'Falha em testes de integracao.' : 'Todos os 5 testes passaram com 100% de sucesso.', 'Relatorio e auditoria de fundo gravados em logs/.');
            appendBackgroundAudit('TEST_RUNNER', 'COMPLETE_SUITE', 'Suite C++ concluida', { success: !error }, error ? 'FAIL' : 'OK');
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({
                success: !error,
                output: output,
                traceLogUrl: '/api/export/latest-test-log',
                traceJsonUrl: '/api/export/latest-test-trace'
            }));
        });
        return;
    }

    // API: Export Continuous Background Trace .log (Human-Readable)
    if (pathname === '/api/export/background-trace' && req.method === 'GET') {
        if (fs.existsSync(BG_LOG_PATH)) {
            res.writeHead(200, {
                'Content-Type': 'text/plain; charset=utf-8',
                'Content-Disposition': `attachment; filename="emulator_background_trace_${Date.now()}.log"`
            });
            return fs.createReadStream(BG_LOG_PATH).pipe(res);
        }
        res.writeHead(404, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Nenhum log de auditoria de fundo gerado ainda.' }));
    }

    // API: Export Continuous Background Trace .jsonl (Machine-Readable Stream)
    if (pathname === '/api/export/background-trace-jsonl' && req.method === 'GET') {
        if (fs.existsSync(BG_JSONL_PATH)) {
            res.writeHead(200, {
                'Content-Type': 'application/x-ndjson; charset=utf-8',
                'Content-Disposition': `attachment; filename="emulator_background_trace_${Date.now()}.jsonl"`
            });
            return fs.createReadStream(BG_JSONL_PATH).pipe(res);
        }
        res.writeHead(404, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Nenhum trace jsonl gerado ainda.' }));
    }

    // API: Export Latest Test Execution Trace .json
    if (pathname === '/api/export/latest-test-trace' && req.method === 'GET') {
        const p = path.join(LOGS_DIR, 'latest_test_execution.json');
        if (fs.existsSync(p)) {
            res.writeHead(200, {
                'Content-Type': 'application/json; charset=utf-8',
                'Content-Disposition': `attachment; filename="latest_test_execution.json"`
            });
            return fs.createReadStream(p).pipe(res);
        }
        res.writeHead(404, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Nenhum teste executado ainda.' }));
    }

    // API: Export Latest Test Execution Trace .log
    if (pathname === '/api/export/latest-test-log' && req.method === 'GET') {
        const p = path.join(LOGS_DIR, 'latest_test_execution.log');
        if (fs.existsSync(p)) {
            res.writeHead(200, {
                'Content-Type': 'text/plain; charset=utf-8',
                'Content-Disposition': `attachment; filename="latest_test_execution.log"`
            });
            return fs.createReadStream(p).pipe(res);
        }
        res.writeHead(404, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Nenhum teste executado ainda.' }));
    }

    // API: Export Diagnostics Report JSON (for a posteriori analysis)
    if (pathname === '/api/export/diagnostics' && req.method === 'GET') {
        const report = {
            exportTimestamp: new Date().toISOString(),
            deviceInfo: {
                id: emulatorState.deviceId,
                boardId: emulatorState.boardId,
                codeId: emulatorState.codeId,
                uptimeSec: emulatorState.uptimeSec
            },
            aggregatedMetrics: emulatorState.metrics,
            activeSensors: emulatorState.sensors,
            faultsStatus: emulatorState.faults,
            telemetrySummary: {
                totalDispatched: emulatorState.telemetry.sequenceNum,
                queueDepth: emulatorState.telemetry.queueCount,
                historyCount: emulatorState.telemetry.history.length
            },
            diagnosticJournal: emulatorState.diagnosticJournal,
            timeSeriesData: emulatorState.timeSeries,
            serialConsoleHistory: emulatorState.serialLogs
        };

        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Content-Disposition': `attachment; filename="diagnostics_${emulatorState.deviceId}_${Date.now()}.json"`
        });
        return res.end(JSON.stringify(report, null, 2));
    }

    // API: Export datalog.csv
    if (pathname === '/api/export/datalog' && req.method === 'GET') {
        let csvContent = "timestamp,pm1,pm25,pm10,t,h,p,o3,no2,voc,co2,snd,vBat\n";
        const sdFilePath = path.join(ROOT_DIR, 'sim_sdcard', 'datalog.csv');
        if (fs.existsSync(sdFilePath)) {
            csvContent = fs.readFileSync(sdFilePath, 'utf-8');
        } else if (emulatorState.sdCard.lastRecord) {
            csvContent += emulatorState.sdCard.lastRecord + '\n';
        }

        res.writeHead(200, {
            'Content-Type': 'text/csv',
            'Content-Disposition': `attachment; filename="datalog_${emulatorState.deviceId}.csv"`
        });
        return res.end(csvContent);
    }

    // Static files
    let filePath = path.join(PUBLIC_DIR, pathname === '/' ? 'index.html' : pathname);
    fs.readFile(filePath, (err, content) => {
        if (err) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            return res.end('404 Not Found');
        }
        let contentType = 'text/html';
        if (filePath.endsWith('.css')) contentType = 'text/css';
        if (filePath.endsWith('.js')) contentType = 'application/javascript';
        if (filePath.endsWith('.json')) contentType = 'application/json';
        if (filePath.endsWith('.svg')) contentType = 'image/svg+xml';
        res.writeHead(200, { 'Content-Type': contentType });
        res.end(content);
    });
});

server.listen(PORT, () => {
    console.log(`=======================================================`);
    console.log(`  ESP32-C6 EMULATOR WEB DASHBOARD RUNNING AT:`);
    console.log(`  >>> http://localhost:${PORT} <<<`);
    console.log(`  Dispositivo Padrao: ${emulatorState.deviceId} (Ultimo de MOQAs.csv)`);
    console.log(`=======================================================`);
});
