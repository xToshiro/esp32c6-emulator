document.addEventListener('DOMContentLoaded', () => {
    // DOM Elements - Header
    const deviceSelect = document.getElementById('device-select');
    const tokenPreview = document.getElementById('token-preview');
    const modeText = document.getElementById('mode-text');
    const badgeMode = document.getElementById('badge-mode');
    const warmupText = document.getElementById('warmup-text');
    const badgeWarmup = document.getElementById('badge-warmup');
    const wifiText = document.getElementById('wifi-text');
    const uptimeText = document.getElementById('uptime-text');

    // DOM Elements - Runtime Bar
    const btnTogglePlay = document.getElementById('btn-toggle-play');
    const playIcon = document.getElementById('play-icon');
    const playText = document.getElementById('play-text');
    const btnReboot = document.getElementById('btn-reboot');
    const wdtBar = document.getElementById('wdt-bar');
    const wdtVal = document.getElementById('wdt-val');
    const heapVal = document.getElementById('heap-val');

    // DOM Elements - Actuators & Hardware
    const neopixelRing = document.getElementById('neopixel-ring');
    const neopixelCore = document.getElementById('neopixel-core');
    const neopixelName = document.getElementById('neopixel-name');
    const neopixelHex = document.getElementById('neopixel-hex');
    const extLedBulb = document.getElementById('ext-led-bulb');
    const extLedStatus = document.getElementById('ext-led-status');
    const hwBtnCfg = document.getElementById('hw-btn-cfg');
    const btnHoldTimer = document.getElementById('btn-hold-timer');

    const railScd = document.getElementById('rail-scd');
    const railBme = document.getElementById('rail-bme');
    const railPm = document.getElementById('rail-pm');
    const railO3 = document.getElementById('rail-o3');
    const railGas = document.getElementById('rail-gas');

    // DOM Elements - Diagnostics & Journal
    const metSuccessRate = document.getElementById('met-success-rate');
    const metTotalAcq = document.getElementById('met-total-acq');
    const metSensorErrs = document.getElementById('met-sensor-errs');
    const metWdtPanics = document.getElementById('met-wdt-panics');
    const journalTbody = document.getElementById('journal-tbody');
    const journalCount = document.getElementById('journal-count');
    const filterButtons = document.querySelectorAll('.btn-filter');
    const btnExportBgTrace = document.getElementById('btn-export-bg-trace');
    const btnExportBgJsonl = document.getElementById('btn-export-bg-jsonl');
    const btnExportDiagnostics = document.getElementById('btn-export-diagnostics');
    const btnExportDatalog = document.getElementById('btn-export-datalog');

    // DOM Elements - Cloud Telemetry
    const dispDevId = document.getElementById('disp-dev-id');
    const queueCount = document.getElementById('queue-count');
    const lastSentTime = document.getElementById('last-sent-time');
    const httpCodeTag = document.getElementById('http-code-tag');
    const seqNum = document.getElementById('seq-num');
    const payloadJson = document.getElementById('payload-json');
    const btnCopyJson = document.getElementById('btn-copy-json');

    // DOM Elements - Terminal & Tests
    const serialTerminal = document.getElementById('serial-terminal');
    const terminalForm = document.getElementById('terminal-form');
    const terminalCmd = document.getElementById('terminal-cmd');
    const btnClearSerial = document.getElementById('btn-clear-serial');
    const testModal = document.getElementById('test-modal');
    const btnRunTests = document.getElementById('btn-run-tests');
    const btnCloseModal = document.getElementById('btn-close-modal');
    const btnModalOk = document.getElementById('btn-modal-ok');
    const btnRetest = document.getElementById('btn-retest');
    const testProgress = document.getElementById('test-progress');
    const testOutputLog = document.getElementById('test-output-log');
    const testAuditBanner = document.getElementById('test-audit-banner');
    const testAuditPath = document.getElementById('test-audit-path');
    const btnDownloadTestLog = document.getElementById('btn-download-test-log');
    const btnDownloadTestJson = document.getElementById('btn-download-test-json');

    // State cache
    let currentJournal = [];
    let currentFilter = 'ALL';
    let availableDevices = [];

    // Sliders
    const sliders = {
        co2: { input: document.getElementById('slider-co2'), display: document.getElementById('val-co2'), unit: 'ppm' },
        temp: { input: document.getElementById('slider-temp'), display: document.getElementById('val-temp'), unit: '°C' },
        hum: { input: document.getElementById('slider-hum'), display: document.getElementById('val-hum'), unit: '%' },
        pres: { input: document.getElementById('slider-pres'), display: document.getElementById('val-pres'), unit: 'hPa' },
        gasRes: { input: document.getElementById('slider-gasres'), display: document.getElementById('val-gasres'), unit: 'kΩ', scale: 0.001 },
        pm1: { input: document.getElementById('slider-pm1'), display: document.getElementById('val-pm1'), unit: 'µg/m³' },
        pm25: { input: document.getElementById('slider-pm25'), display: document.getElementById('val-pm25'), unit: 'µg/m³' },
        pm10: { input: document.getElementById('slider-pm10'), display: document.getElementById('val-pm10'), unit: 'µg/m³' },
        o3: { input: document.getElementById('slider-o3'), display: document.getElementById('val-o3'), unit: 'ppb' },
        co: { input: document.getElementById('slider-co'), display: document.getElementById('val-co'), unit: 'ppm' },
        no2: { input: document.getElementById('slider-no2'), display: document.getElementById('val-no2'), unit: 'ppm' },
        voc: { input: document.getElementById('slider-voc'), display: document.getElementById('val-voc'), unit: 'ppm' },
        vBat: { input: document.getElementById('slider-vbat'), display: document.getElementById('val-vbat'), unit: 'V' },
        soundDb: { input: document.getElementById('slider-snd'), display: document.getElementById('val-snd'), unit: 'dB SPL' }
    };
    const batModeTag = document.getElementById('bat-mode-tag');

    // Attach slider inputs
    Object.keys(sliders).forEach(key => {
        const item = sliders[key];
        if (!item.input) return;
        item.input.addEventListener('input', () => {
            let val = parseFloat(item.input.value);
            let displayVal = item.scale ? (val * item.scale).toFixed(1) : val;
            item.display.textContent = `${displayVal} ${item.unit}`;
            sendSensorUpdate({ [key]: val });
        });
    });

    let updateTimeout = null;
    function sendSensorUpdate(data) {
        clearTimeout(updateTimeout);
        updateTimeout = setTimeout(() => {
            fetch('/api/sensor', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });
        }, 80);
    }

    // Connect to Server-Sent Events (SSE)
    const evtSource = new EventSource('/events');

    evtSource.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            handleServerEvent(data);
        } catch (e) {
            console.error('SSE Error:', e);
        }
    };

    function handleServerEvent(data) {
        if (data.type === 'init') {
            if (data.devices) {
                populateDeviceDropdown(data.devices, data.state.deviceId);
            }
            applyFullState(data.state);
        } else if (data.type === 'state') {
            applyFullState(data.state);
        } else if (data.type === 'rgbLed') {
            updateLedVisuals(data.led);
        } else if (data.type === 'extLed') {
            extLedBulb.classList.toggle('bulb-active', data.state);
            extLedStatus.textContent = data.state ? 'Ativo (Pulso)' : 'Inativo';
        } else if (data.type === 'sensors') {
            updateSensorSliders(data.sensors);
        } else if (data.type === 'telemetry') {
            updateTelemetryDisplay(data.telemetry);
        } else if (data.type === 'metrics') {
            updateMetrics(data.metrics);
        } else if (data.type === 'journalEvent') {
            currentJournal.unshift(data.event);
            if (currentJournal.length > 500) currentJournal.pop();
            renderJournalTable();
        } else if (data.type === 'serial') {
            appendSerialLog(data.message);
        }
    }

    function populateDeviceDropdown(devices, currentId) {
        availableDevices = devices;
        deviceSelect.innerHTML = '';
        devices.forEach(dev => {
            const opt = document.createElement('option');
            opt.value = dev.id;
            opt.textContent = `${dev.id} ${dev.id === 'MOQA-INFRA-048' ? '(Padrão)' : ''}`;
            if (dev.id === currentId) opt.selected = true;
            deviceSelect.appendChild(opt);
        });
        updateTokenPreview(currentId);
    }

    function updateTokenPreview(id) {
        const found = availableDevices.find(d => d.id === id);
        if (found) {
            tokenPreview.textContent = found.token ? `${found.token.substring(0, 15)}...` : '';
            tokenPreview.title = `Token Vitae: ${found.token}`;
        }
    }

    deviceSelect.addEventListener('change', () => {
        const newId = deviceSelect.value;
        updateTokenPreview(newId);
        fetch('/api/device', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: newId })
        });
    });

    function applyFullState(state) {
        // Device info
        dispDevId.textContent = state.deviceId;
        if (deviceSelect.value !== state.deviceId) {
            deviceSelect.value = state.deviceId;
            updateTokenPreview(state.deviceId);
        }

        // Mode & Status
        modeText.textContent = state.mode;
        badgeMode.querySelector('.badge-dot').className = `badge-dot ${state.mode === 'COLETA' ? 'dot-cyan' : 'dot-amber'}`;
        
        warmupText.textContent = state.inWarmup ? 'ATIVO (5m)' : 'CONCLUÍDO';
        badgeWarmup.querySelector('.badge-dot').className = `badge-dot ${state.inWarmup ? 'dot-blue' : 'dot-green'}`;

        wifiText.textContent = state.wifiConnected ? state.wifiIP : 'DESCONECTADO';

        // Format uptime
        const hrs = String(Math.floor(state.uptimeSec / 3600)).padStart(2, '0');
        const mins = String(Math.floor((state.uptimeSec % 3600) / 60)).padStart(2, '0');
        const secs = String(state.uptimeSec % 60).padStart(2, '0');
        uptimeText.textContent = `${hrs}:${mins}:${secs}`;

        // Play/Pause button
        playIcon.textContent = state.simRunning ? '⏸️' : '▶️';
        playText.textContent = state.simRunning ? 'Pausar Simulação' : 'Retomar Simulação';

        // Watchdog Bar
        if (state.watchdog) {
            const pct = Math.max(0, Math.min(100, (state.watchdog.remainingMs / state.watchdog.timeoutMs) * 100));
            wdtBar.style.width = `${pct}%`;
            wdtVal.textContent = `${(state.watchdog.remainingMs / 1000).toFixed(1)}s`;
            if (pct < 25) {
                wdtBar.style.backgroundColor = 'var(--accent-red)';
            } else if (pct < 50) {
                wdtBar.style.backgroundColor = 'var(--accent-amber)';
            } else {
                wdtBar.style.backgroundColor = 'var(--accent-green)';
            }
        }

        // Heap
        heapVal.textContent = `${(state.freeHeap / 1024).toFixed(1)} KB`;

        // Actuators
        updateLedVisuals(state.rgbLed);

        // Power Rails
        railScd.classList.toggle('rail-off', !state.mcp23008.pwrScd);
        railBme.classList.toggle('rail-off', !state.mcp23008.pwrBme);
        railPm.classList.toggle('rail-off', !state.mcp23008.pwrPm);
        railO3.classList.toggle('rail-off', !state.mcp23008.pwrO3);
        railGas.classList.toggle('rail-off', !state.mcp23008.pwrGas);

        // Fault buttons state
        if (state.faults) {
            document.querySelectorAll('.btn-fault[data-fault]').forEach(btn => {
                const f = btn.dataset.fault;
                btn.classList.toggle('fault-active', !!state.faults[f]);
            });
        }

        // Modular Satellites
        if (state.satellites) {
            updateSatellites(state.satellites, state);
        }

        // Sensors
        updateSensorSliders(state.sensors);

        // Telemetry
        updateTelemetryDisplay(state.telemetry);

        // Metrics
        if (state.metrics) {
            updateMetrics(state.metrics);
        }

        // Journal
        if (state.diagnosticJournal) {
            currentJournal = state.diagnosticJournal;
            renderJournalTable();
        }

        // Serial logs on init
        if (state.serialLogs && serialTerminal.children.length === 0) {
            state.serialLogs.forEach(msg => appendSerialLog(msg));
        }
    }

    function updateMetrics(m) {
        metTotalAcq.textContent = m.totalAcquisitions;
        metSensorErrs.textContent = m.sensorErrors;
        metWdtPanics.textContent = m.watchdogPanics;
        const total = m.packetsSent + m.packetsFailed;
        const rate = (total > 0) ? Math.round((m.packetsSent / total) * 100) : 100;
        metSuccessRate.textContent = `${rate}%`;
        metSuccessRate.className = `metric-num ${rate < 80 ? 'text-red' : (rate < 95 ? 'text-amber' : 'text-green')}`;
    }

    function renderJournalTable() {
        const filtered = currentJournal.filter(item => {
            if (currentFilter === 'ALL') return true;
            if (currentFilter === 'ERROR') return item.severity === 'ERROR' || item.severity === 'CRITICAL';
            if (currentFilter === 'WARN') return item.severity === 'WARN';
            if (currentFilter === 'I2C') return item.subsystem === 'I2C';
            if (currentFilter === 'CLOUD') return item.subsystem === 'CLOUD';
            return true;
        });

        journalCount.textContent = `${filtered.length} eventos`;
        journalTbody.innerHTML = filtered.slice(0, 50).map(item => `
            <tr>
                <td style="color: var(--text-muted); font-family: 'JetBrains Mono', monospace;">${item.timestamp}</td>
                <td><span class="badge-sev sev-${item.severity.toLowerCase()}">${item.severity}</span></td>
                <td style="font-weight: 600; color: var(--accent-cyan);">${item.subsystem}</td>
                <td><strong>${item.title}</strong><br><span style="color: var(--text-muted); font-size: 0.7rem;">${item.details}</span></td>
                <td style="color: #94a3b8; font-size: 0.72rem;">${item.systemAction || '--'}</td>
            </tr>
        `).join('');
    }

    // Journal filter buttons
    filterButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            filterButtons.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            currentFilter = btn.dataset.filter;
            renderJournalTable();
        });
    });

    // Simulation runtime controls
    btnTogglePlay.addEventListener('click', () => {
        fetch('/api/sim-control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'toggle_play' })
        });
    });

    btnReboot.addEventListener('click', () => {
        fetch('/api/sim-control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'reset' })
        });
    });

    // Fault Injection toggles
    document.querySelectorAll('.btn-fault[data-fault]').forEach(btn => {
        btn.addEventListener('click', () => {
            const fault = btn.dataset.fault;
            fetch('/api/fault', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ fault })
            });
        });
    });

    // Modular Satellite Hardware Controls
    let currentSatellites = {};
    function updateSatellites(satellites, state) {
        currentSatellites = satellites;
        Object.keys(satellites).forEach(id => {
            const sat = satellites[id];
            const unit = document.getElementById(`sat-unit-${id}`);
            const statusPill = document.getElementById(`sat-status-${id}`);
            const btnToggle = document.getElementById(`btn-sat-${id}`);

            if (unit) {
                unit.classList.toggle('unit-offline', !sat.plugged);
                unit.classList.toggle('unit-fault', sat.plugged && sat.fault !== 'NONE');
            }

            if (statusPill) {
                statusPill.textContent = sat.status;
                statusPill.className = `sat-status-pill ${
                    !sat.plugged ? 'status-offline' : (sat.fault !== 'NONE' ? 'status-fault' : 'status-online')
                }`;
            }

            if (btnToggle) {
                btnToggle.textContent = sat.plugged ? (id === 'sdcard' ? 'Ejetar Cartão' : 'Desconectar') : (id === 'sdcard' ? 'Inserir Cartão' : 'Conectar Módulo');
                btnToggle.className = `btn-sat-toggle ${sat.plugged ? 'btn-sat-disconnect' : 'btn-sat-connect'}`;
            }

            // Sub-status specifics
            if (id === 'gps') {
                const nmeaEl = document.getElementById('sat-nmea-status');
                const fixEl = document.getElementById('sat-gps-fix');
                if (nmeaEl) nmeaEl.textContent = sat.plugged ? (sat.fault === 'GARBAGE' ? 'Ruído / Corrompido' : '$GPRMC / $GPGGA (1Hz)') : 'Desconectado (0 B/s)';
                if (fixEl) fixEl.textContent = (sat.plugged && sat.fault !== 'NO_FIX') ? `3D Fix (${state.sensors.gpsLat}, ${state.sensors.gpsLon})` : 'Sem Fix / Sem Sinal';
            } else if (id === 'sdcard') {
                const fileEl = document.getElementById('sat-sd-file');
                const recEl = document.getElementById('sat-sd-records');
                if (fileEl) fileEl.textContent = sat.plugged && sat.fault !== 'IO_ERROR' ? '/datalog.csv (FAT32)' : 'Desmontado / Ejetado';
                if (recEl) recEl.textContent = sat.plugged && sat.fault !== 'IO_ERROR' ? `${state.sdCard ? state.sdCard.recordCount : 0} Registros` : 'Interrompido';
            } else if (id === 'modem') {
                const connEl = document.getElementById('sat-modem-conn');
                const qEl = document.getElementById('sat-modem-queue');
                if (connEl) connEl.textContent = sat.plugged ? (state.wifiConnected ? 'Conectado (RSSI -64dBm)' : 'Sem Sinal IP') : 'Desconectado';
                if (qEl) qEl.textContent = `${state.telemetry ? state.telemetry.queueCount : 0} / 80 pacotes`;
            } else if (id === 'scd41') {
                const el = document.getElementById('sat-scd-readout');
                if (el) el.textContent = sat.plugged ? `${state.sensors.co2} ppm | ${state.sensors.temp}°C` : 'NACK (0x62 Desconectado)';
            } else if (id === 'bme680') {
                const el = document.getElementById('sat-bme-readout');
                if (el) el.textContent = sat.plugged ? `${state.sensors.pres} hPa | ${(state.sensors.gasRes / 1000).toFixed(1)} kΩ` : 'NACK (0x77 Desconectado)';
            } else if (id === 'pm') {
                const el = document.getElementById('sat-pm-readout');
                if (el) el.textContent = sat.plugged ? `${state.sensors.pm25} µg/m³` : 'NACK (0x19 Desconectado)';
            } else if (id === 'ozone') {
                const el = document.getElementById('sat-ozone-readout');
                if (el) el.textContent = sat.plugged ? `${state.sensors.o3} ppb` : 'NACK (0x73 Desconectado)';
            } else if (id === 'multigas') {
                const el = document.getElementById('sat-gas-readout');
                if (el) el.textContent = sat.plugged ? `CO ${state.sensors.co} | NO2 ${state.sensors.no2}` : 'NACK (0x08 Desconectado)';
            }
        });
    }

    document.querySelectorAll('.btn-sat-toggle[data-sat]').forEach(btn => {
        btn.addEventListener('click', () => {
            const satId = btn.dataset.sat;
            const current = currentSatellites[satId];
            const nextPlugged = current ? !current.plugged : false;
            fetch('/api/satellite/toggle', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: satId, plugged: nextPlugged })
            });
        });
    });

    document.querySelectorAll('.btn-sat-fault[data-sat]').forEach(btn => {
        btn.addEventListener('click', () => {
            const satId = btn.dataset.sat;
            const faultType = btn.dataset.fault;
            const current = currentSatellites[satId];
            const nextFault = (current && current.fault === faultType) ? 'NONE' : faultType;
            btn.classList.toggle('active', nextFault !== 'NONE');
            fetch('/api/satellite/toggle', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: satId, fault: nextFault })
            });
        });
    });

    // Export Continuous Background Trace .log
    if (btnExportBgTrace) {
        btnExportBgTrace.addEventListener('click', () => {
            window.location.href = '/api/export/background-trace';
        });
    }

    // Export Continuous Background Trace .jsonl
    if (btnExportBgJsonl) {
        btnExportBgJsonl.addEventListener('click', () => {
            window.location.href = '/api/export/background-trace-jsonl';
        });
    }

    // Export diagnostics report JSON
    btnExportDiagnostics.addEventListener('click', () => {
        window.location.href = '/api/export/diagnostics';
    });

    // Export datalog.csv
    btnExportDatalog.addEventListener('click', () => {
        window.location.href = '/api/export/datalog';
    });

    function updateLedVisuals(led) {
        if (!led) return;
        const hex = `#${((1 << 24) + (led.r << 16) + (led.g << 8) + led.b).toString(16).slice(1)}`;
        neopixelCore.style.backgroundColor = hex;
        neopixelRing.style.boxShadow = `0 0 25px ${hex}, inset 0 0 10px rgba(255,255,255,0.4)`;
        neopixelName.textContent = led.colorName || hex;
        neopixelHex.textContent = hex.toUpperCase();
    }

    function updateSensorSliders(s) {
        if (!s) return;
        Object.keys(s).forEach(k => {
            if (sliders[k] && sliders[k].input) {
                sliders[k].input.value = s[k];
                let displayVal = sliders[k].scale ? (s[k] * sliders[k].scale).toFixed(1) : s[k];
                sliders[k].display.textContent = `${displayVal} ${sliders[k].unit}`;
            }
        });

        // Battery Mode Pill
        if (s.batMode) {
            batModeTag.className = `bat-mode-pill pill-${s.batMode.toLowerCase()}`;
            if (s.batMode === 'FULL') batModeTag.textContent = 'FULL (>3.50V)';
            else if (s.batMode === 'PARTIAL') batModeTag.textContent = 'PARTIAL (3.20V - 3.50V)';
            else batModeTag.textContent = 'EMERGENCY (<3.20V)';
        }
    }

    function updateTelemetryDisplay(tel) {
        if (!tel) return;
        queueCount.textContent = `${tel.queueCount}/${tel.maxQueue}`;
        lastSentTime.textContent = tel.lastSentTime;
        seqNum.textContent = `#${tel.sequenceNum}`;
        httpCodeTag.textContent = `${tel.lastHttpCode} OK`;
        httpCodeTag.className = `http-pill ${tel.lastHttpCode === 200 ? 'pill-200' : 'sev-error'}`;

        if (tel.lastPayload) {
            payloadJson.textContent = JSON.stringify(tel.lastPayload, null, 2);
        }
    }

    function appendSerialLog(msg) {
        const line = document.createElement('div');
        line.className = 'terminal-line';
        line.textContent = msg;
        serialTerminal.appendChild(line);
        serialTerminal.scrollTop = serialTerminal.scrollHeight;
    }

    // Quick Scenarios buttons
    document.querySelectorAll('.btn-scenario[data-scenario]').forEach(btn => {
        btn.addEventListener('click', () => {
            const scenario = btn.dataset.scenario;
            fetch('/api/scenario', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ scenario })
            });
        });
    });

    // Hardware Button Click & Hold simulation
    let btnTimer = null;
    let holdDuration = 0;

    hwBtnCfg.addEventListener('mousedown', () => {
        holdDuration = 0;
        btnHoldTimer.textContent = 'Pressionado... 0s/5s';
        btnTimer = setInterval(() => {
            holdDuration++;
            btnHoldTimer.textContent = `Segurando... ${holdDuration}s/5s`;
            if (holdDuration >= 5) {
                btnHoldTimer.textContent = 'Solte para forçar Modo AP!';
            }
        }, 1000);
    });

    const releaseBtn = () => {
        if (btnTimer) {
            clearInterval(btnTimer);
            btnTimer = null;
            if (holdDuration >= 5) {
                fetch('/api/scenario', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ scenario: 'toggle_ap_mode' })
                });
            } else if (holdDuration < 2) {
                fetch('/api/scenario', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ scenario: 'skip_warmup' })
                });
            }
            btnHoldTimer.textContent = 'Solto';
            holdDuration = 0;
        }
    };

    hwBtnCfg.addEventListener('mouseup', releaseBtn);
    hwBtnCfg.addEventListener('mouseleave', releaseBtn);

    // Terminal Input
    terminalForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const cmd = terminalCmd.value.trim();
        if (!cmd) return;
        terminalCmd.value = '';
        fetch('/api/serial', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: cmd })
        });
    });

    btnClearSerial.addEventListener('click', () => {
        serialTerminal.innerHTML = '';
    });

    // Copy JSON Payload
    btnCopyJson.addEventListener('click', () => {
        navigator.clipboard.writeText(payloadJson.textContent).then(() => {
            btnCopyJson.textContent = 'Copiado!';
            setTimeout(() => btnCopyJson.textContent = 'Copiar JSON', 1500);
        });
    });

    // Run C++ Test Suites Modal
    function runTests() {
        testModal.style.display = 'flex';
        testProgress.style.display = 'flex';
        if (testAuditBanner) testAuditBanner.style.display = 'none';
        testOutputLog.textContent = 'Compilando e executando test_suites.exe em Tempo Real 1.0x com auditoria de fundo...';

        fetch('/api/run-tests', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                testProgress.style.display = 'none';
                testOutputLog.textContent = data.output;
                if (testAuditBanner) {
                    testAuditBanner.style.display = 'flex';
                }
            })
            .catch(err => {
                testProgress.style.display = 'none';
                testOutputLog.textContent = `Erro ao executar testes: ${err.message}`;
            });
    }

    if (btnDownloadTestLog) {
        btnDownloadTestLog.addEventListener('click', () => {
            window.location.href = '/api/export/latest-test-log';
        });
    }

    if (btnDownloadTestJson) {
        btnDownloadTestJson.addEventListener('click', () => {
            window.location.href = '/api/export/latest-test-trace';
        });
    }

    btnRunTests.addEventListener('click', runTests);
    btnRetest.addEventListener('click', runTests);
    btnCloseModal.addEventListener('click', () => testModal.style.display = 'none');
    btnModalOk.addEventListener('click', () => testModal.style.display = 'none');
});
