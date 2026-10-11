let currentStep = 1;
let ws;
let configData = {};
let reconnectTimer = null;
let reconnectDelayMs = 1000;
const maxReconnectDelayMs = 30000;

// motorSettings speichert für jeden Motor: side (left/right/gripp/none), invert (bool), deadband (int)
let motorSettings = {
    A: {side:null, invert:false, deadband:0},
    B: {side:null, invert:false, deadband:0},
    C: {side:null, invert:false, deadband:0},
    D: {side:null, invert:false, deadband:0}
};

document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    loadConfig();
    setupDeadbandSliders();
});

function goToStep(n) {
    document.getElementById('step'+currentStep).classList.remove('active');
    currentStep = n;
    document.getElementById('step'+n).classList.add('active');
}

function renderMotorRoleButtons(guiMode) {
    const letters = ['A', 'B', 'C', 'D'];
    letters.forEach(letter => {
        const container = document.getElementById('motor' + letter + 'Question');
        if (!container) return;
        if (guiMode === 'arm') {
            container.innerHTML = `
                <p>Welches Gelenk des Roboter-Arms hat sich bewegt?</p>
                <button onclick="motorMoved('${letter}','base')">Rotierende Basis (Drehkranz)</button>
                <button onclick="motorMoved('${letter}','arm1')">Armteil 1 (Unterarm / Schulter)</button>
                <button onclick="motorMoved('${letter}','arm2')">Armteil 2 (Mittelarm / Ellbogen)</button>
                <button onclick="motorMoved('${letter}','arm3')">Armteil 3 (Handgelenk / Oberarm)</button>
                <button onclick="motorMoved('${letter}','none')">Kein Motor</button>
            `;
        } else {
            container.innerHTML = `
                <p>Welcher Motor hat sich bewegt?</p>
                <button onclick="motorMoved('${letter}','left')">Linker Motor</button>
                <button onclick="motorMoved('${letter}','right')">Rechter Motor</button>
                <button onclick="motorMoved('${letter}','gripp')">Greifer Motor</button>
                <button onclick="motorMoved('${letter}','none')">Kein Motor</button>
            `;
        }
    });
}

function updateSetupIntroByGuiMode() {
    const sel = document.getElementById('gui_mode');
    const intro = document.getElementById('setupIntroText');
    if (!sel || !intro) return;
    renderMotorRoleButtons(sel.value);
    if (sel.value === 'arm') {
        intro.textContent = 'Roboter-Arm ausgewählt: Wir ordnen jetzt die 4 DC-Motoren der rotierenden Basis sowie den 3 Armteilen (Unterarm, Mittelarm, Handgelenk) zu. Nicht belegte Motoren ("Kein Motor") werden automatisch übersprungen.';
    } else if (sel.value === 'drive') {
        intro.textContent = 'Erweiterter Fahr-Roboter ausgewählt: Wir ordnen jetzt die Fahrmotoren (Links/Rechts) und Zusatz-/Greifermotoren zu. Nicht belegte Motoren ("Kein Motor") werden automatisch übersprungen.';
    } else {
        intro.textContent = 'Workshop-Roboter (Basic) ausgewählt: Wir konfigurieren jetzt die 2 Kettenmotoren (Links/Rechts) und den Greifer-Motor (Auf/Zu). Nicht belegte Motoren ("Kein Motor") werden automatisch übersprungen.';
    }
}

function updateDirectionLabels(letter, side) {
    const titleEl = document.getElementById('dirTitle' + letter);
    const promptEl = document.getElementById('dirPrompt' + letter);
    const fwdBtn = document.getElementById('dirFwdBtn' + letter);
    const bwdBtn = document.getElementById('dirBwdBtn' + letter);
    if (side === 'gripp') {
        if (titleEl) titleEl.textContent = `Drehrichtung Greifer (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Öffnet oder schließt sich der Greifer beim Test von Motor ${letter}?`;
        if (fwdBtn) fwdBtn.textContent = 'Greifer Auf (Öffnet)';
        if (bwdBtn) bwdBtn.textContent = 'Greifer Zu (Schließt)';
    } else if (side === 'base') {
        if (titleEl) titleEl.textContent = `Drehrichtung Basis (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Dreht sich die Basis beim Test im Uhrzeigersinn (Rechts ↻) oder gegen den Uhrzeigersinn (Links ↺)?`;
        if (fwdBtn) fwdBtn.textContent = 'Rechts ↻ (+)';
        if (bwdBtn) bwdBtn.textContent = 'Links ↺ (-)';
    } else if (side === 'arm1' || side === 'arm2' || side === 'arm3') {
        const names = { arm1: 'Armteil 1 (Schulter)', arm2: 'Armteil 2 (Ellbogen)', arm3: 'Armteil 3 (Handgelenk)' };
        if (titleEl) titleEl.textContent = `Bewegungsrichtung ${names[side]} (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Bewegt sich ${names[side]} beim Test nach oben/zurück (+) oder nach unten/vorne (-)?`;
        if (fwdBtn) fwdBtn.textContent = 'Hoch / Zurück (+)';
        if (bwdBtn) bwdBtn.textContent = 'Runter / Vor (-)';
    } else {
        if (titleEl) titleEl.textContent = `Fahrtrichtung Motor ${letter}`;
        if (promptEl) promptEl.textContent = `Fährt der Roboter bei "Motor ${letter} vorwärts" tatsächlich vorwärts oder rückwärts?`;
        if (fwdBtn) fwdBtn.textContent = 'Vorwärts';
        if (bwdBtn) bwdBtn.textContent = 'Rückwärts';
    }
}

function connectWebSocket() {
    ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    ws.onopen = () => {
        console.log("WebSocket connected");
        reconnectDelayMs = 1000;
        updateConnectionStatus("verbunden");
    };
    ws.onmessage = (event) => {
        console.log("WS Message:", event.data);
    };
    ws.onclose = (event) => {
        console.log(`WebSocket closed: Code ${event.code}, reason: ${event.reason}`);
        updateConnectionStatus("geschlossen");
        scheduleReconnect();
    };
    ws.onerror = (error) => {
        console.error("WebSocket error:", error);
        updateConnectionStatus("fehler");
        try {
            ws.close();
        } catch (_) {}
    };
}

function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connectWebSocket();
        reconnectDelayMs = Math.min(reconnectDelayMs * 2, maxReconnectDelayMs);
    }, reconnectDelayMs);
}

function updateConnectionStatus(status) {
    const statusElement = document.getElementById('connectionStatus');
    if (!statusElement) return;
    const statusText = statusElement.querySelector('span');
    if (!statusText) return;
    statusText.innerText = status;
    if (status === 'verbunden') statusText.style.color = 'green';
    else if (status === 'geschlossen') statusText.style.color = 'red';
    else statusText.style.color = '#b45309';
}

function sendWS(payload) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return false;
    ws.send(JSON.stringify(payload));
    return true;
}

function loadConfig() {
    fetch('/getConfig')
      .then(r=>r.json())
      .then(data=>{
        configData = data;
        applyDeviceName(data);
        if (configData.hotspot_ssid) {
            const el = document.getElementById('wifi_name');
            if (el) el.value = configData.hotspot_ssid;
        }
        const guiModeEl = document.getElementById('gui_mode');
        if (guiModeEl) {
            guiModeEl.value = configData.gui_mode || 'basic';
            guiModeEl.addEventListener('change', updateSetupIntroByGuiMode);
            updateSetupIntroByGuiMode();
        }
        initDeadbandValues();
      });
}

function applyDeviceName(cfg) {
    const name = (cfg.hotspot_ssid || cfg.wifi_ssid || 'TinkerThinker').trim();
    const el1 = document.getElementById('deviceName');
    const el2 = document.getElementById('deviceName2');
    if (el1) el1.textContent = name;
    if (el2) el2.textContent = name;
    document.title = `${name} Setup`;
}

function testMotor(letter) {
    // Motor starten (vorwärts) für max. 1s
    sendWS({["motor"+letter]:"forward"});
    setTimeout(()=>{
        sendWS({["motor"+letter]:"stop"});
        document.getElementById('motor'+letter+'Question').style.display='block';
    },1000);
}

function nextMotorStartStep(letter) {
    if (letter === 'A') return 6;  // Motor B Zuordnung
    if (letter === 'B') return 9;  // Motor C Zuordnung
    if (letter === 'C') return 12; // Motor D Zuordnung
    return 15;                     // Fertig
}

function motorMoved(letter, side) {
    // Motor stoppen
    sendWS({["motor"+letter]:"stop"});
    motorSettings[letter].side = side;

    // Wenn "Kein Motor" gewählt wurde, Richtung und Deadband für diesen Motor überspringen
    if (side === 'none') {
        const idx = letterToIndex(letter);
        motorSettings[letter].invert = !!(configData.motor_invert && configData.motor_invert[idx]);
        motorSettings[letter].deadband = (configData.motor_deadband && configData.motor_deadband[idx] !== undefined)
            ? parseInt(configData.motor_deadband[idx], 10)
            : 0;
        goToStep(nextMotorStartStep(letter));
        return;
    }

    updateDirectionLabels(letter, side);

    // Weiter zur Richtungseinstellung
    if (letter==='A') goToStep(4);
    else if (letter==='B') goToStep(7);
    else if (letter==='C') goToStep(10);
    else if (letter==='D') goToStep(13);
}

function directionChosen(letter, dir) {
    // invert = true, wenn backward
    motorSettings[letter].invert = (dir==='backward');
    // Weiter zum Deadband
    if (letter==='A') goToStep(5);
    else if (letter==='B') goToStep(8);
    else if (letter==='C') goToStep(11);
    else if (letter==='D') goToStep(14);
}

function testDirection(letter, dir) {
    // Richtung 1s testen (vorwärts/rückwärts)
    sendWS({["motor"+letter]: dir});
    setTimeout(()=>{
        sendWS({["motor"+letter]:"stop"});
    },1000);
}

function testDeadband(letter) {
    const slider = document.getElementById('db_slider_'+letter);
    let val = slider ? slider.value : 0;
    let idx = letterToIndex(letter);
    // Raw PWM senden (Deadband soll nicht dazwischenfunken)
    sendWS({"motor_raw":{"motor":idx,"pwm":parseInt(val, 10)}});
    setTimeout(()=>{
        sendWS({"motor_raw":{"motor":idx,"pwm":0}});
    },1000);
}

function confirmDeadband(letter) {
    let val = document.getElementById('db_slider_'+letter).value;
    motorSettings[letter].deadband = parseInt(val, 10);
    // Motor stoppen
    let idx = letterToIndex(letter);
    sendWS({"motor_raw":{"motor":idx,"pwm":0}});

    // Nächster Schritt
    goToStep(nextMotorStartStep(letter));
}

function letterToIndex(letter) {
    // A=0, B=1, C=2, D=3
    return letter.charCodeAt(0) - 65; 
}

function resolveDrivePair() {
    const letters = ['A', 'B', 'C', 'D'];
    let leftIdx = -1;
    let rightIdx = -1;
    for (const letter of letters) {
        const idx = letterToIndex(letter);
        const side = motorSettings[letter].side;
        if (side === 'left' && leftIdx < 0) leftIdx = idx;
        if (side === 'right' && rightIdx < 0) rightIdx = idx;
    }

    if (leftIdx < 0 && Number.isInteger(configData.motor_left_gui)) {
        leftIdx = configData.motor_left_gui;
    }
    if (rightIdx < 0 && Number.isInteger(configData.motor_right_gui)) {
        rightIdx = configData.motor_right_gui;
    }

    if (leftIdx < 0) leftIdx = 2;
    if (rightIdx < 0 || rightIdx === leftIdx) {
        rightIdx = [0, 1, 2, 3].find(i => i !== leftIdx);
    }
    return { leftIdx, rightIdx };
}

function resolveGripperMotor(drivePair) {
    const letters = ['A', 'B', 'C', 'D'];
    for (const letter of letters) {
        if (motorSettings[letter].side === 'gripp') {
            return letterToIndex(letter);
        }
    }
    if (Number.isInteger(configData.motor_gripper_gui) &&
        configData.motor_gripper_gui !== drivePair.leftIdx &&
        configData.motor_gripper_gui !== drivePair.rightIdx) {
        return configData.motor_gripper_gui;
    }
    const fallback = [0, 1, 2, 3].find(i => i !== drivePair.leftIdx && i !== drivePair.rightIdx);
    return fallback !== undefined ? fallback : 0;
}

function finalizeSetup() {
    // POST /config mit allen Werten
    let formData = new FormData();
    // Bestehenden WiFi-Modus behalten (AP/STA), Radio-Mode wird in Firmware beim Boot gesetzt.
    formData.append("wifi_mode", configData.wifi_mode || "AP");
    formData.append("wifi_ssid", configData.wifi_ssid || "");
    formData.append("wifi_password", configData.wifi_password || "");
    const wifiNameInput = document.getElementById('wifi_name');
    const newHotspotName = wifiNameInput ? wifiNameInput.value.trim() : "";
    formData.append("hotspot_ssid", newHotspotName || configData.hotspot_ssid || "TinkerThinkerAP");
    formData.append("hotspot_password", configData.hotspot_password || "");

    const guiModeEl = document.getElementById('gui_mode');
    const guiMode = guiModeEl ? guiModeEl.value : (configData.gui_mode || "basic");
    formData.append("gui_mode", guiMode);

    if (guiMode === 'arm') {
        try {
            const raw = localStorage.getItem('tinkerthinker_arm_config_v1');
            const armCfg = raw ? JSON.parse(raw) : {};
            const letters = ['A', 'B', 'C', 'D'];
            for (const letter of letters) {
                const role = motorSettings[letter].side;
                if (role === 'base' || role === 'arm1' || role === 'arm2' || role === 'arm3') {
                    const mIdx = letterToIndex(letter);
                    armCfg[role] = armCfg[role] || {};
                    armCfg[role].motorIndex = mIdx;
                    armCfg[role].invert = false; // Invert wird bereits in Firmware motor_invert_X gespeichert
                    if (mIdx === 0) armCfg[role].sensorMode = '1'; // Motor A = Sensor 2 (GPIO36)
                    else if (mIdx === 1) armCfg[role].sensorMode = '0'; // Motor B = Sensor 1 (GPIO34)
                }
            }
            localStorage.setItem('tinkerthinker_arm_config_v1', JSON.stringify(armCfg));
        } catch (_) {}
    }

    const drivePair = resolveDrivePair();
    const gripperIdx = resolveGripperMotor(drivePair);
    formData.append("motor_left_gui", String(drivePair.leftIdx));
    formData.append("motor_right_gui", String(drivePair.rightIdx));
    formData.append("motor_gripper_gui", String(gripperIdx));

    // Motoren: invert + deadband; Frequenz beibehalten
    let letters = ['A','B','C','D'];
    for (let i=0; i<4; i++){
        let lettr = letters[i];
        if (motorSettings[lettr].invert) {
            formData.append(`motor_invert_${i}`, "on");
        }
        formData.append(`motor_deadband_${i}`, motorSettings[lettr].deadband.toString());

        // frequency beibehalten
        let freq = (configData.motor_frequency && configData.motor_frequency[i] !== undefined)
            ? configData.motor_frequency[i]
            : 5000;
        formData.append(`motor_frequency_${i}`, freq.toString());
    }

    formData.append("led_count", String(configData.led_count || 30));
    if (configData.ota_enabled) formData.append("ota_enabled","on");
    if (configData.legacy_board) formData.append("legacy_board", "on");

    for (let i=0; i<7; i++){
        const minPw = configData.servo_settings?.[i]?.min_pulsewidth ?? 500;
        const maxPw = configData.servo_settings?.[i]?.max_pulsewidth ?? 2500;
        formData.append(`servo${i}_min`, String(minPw));
        formData.append(`servo${i}_max`, String(maxPw));
    }

    fetch('/config',{
        method:'POST',
        body:formData
    }).then(()=>{
        goToStep(16);
        setTimeout(() => {
            fetch('/reboot').catch(()=>{});
        }, 400);
    }).catch((err) => {
        console.error('Setup speichern fehlgeschlagen:', err);
        alert('Speichern fehlgeschlagen. Bitte erneut versuchen.');
    });
}

function setupDeadbandSliders() {
    const letters = ['A','B','C','D'];
    letters.forEach(letter => {
        const slider = document.getElementById('db_slider_' + letter);
        if (!slider) return;
        slider.addEventListener('input', () => updateDeadbandValue(letter));
    });
}

function initDeadbandValues() {
    const letters = ['A','B','C','D'];
    letters.forEach((letter, i) => {
        const slider = document.getElementById('db_slider_' + letter);
        const val = (configData.motor_deadband && configData.motor_deadband[i] !== undefined)
            ? parseInt(configData.motor_deadband[i], 10)
            : (slider ? parseInt(slider.value || "0", 10) : 0);
        motorSettings[letter].deadband = val;
        if (configData.motor_invert && configData.motor_invert[i] !== undefined) {
            motorSettings[letter].invert = !!configData.motor_invert[i];
        }
        if (slider) {
            slider.value = val;
            updateDeadbandValue(letter);
        }
    });
}

function updateDeadbandValue(letter) {
    const slider = document.getElementById('db_slider_' + letter);
    const label = document.getElementById('db_value_' + letter);
    if (!slider || !label) return;
    label.textContent = slider.value;
}
