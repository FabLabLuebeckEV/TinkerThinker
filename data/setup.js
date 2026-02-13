let currentStep = 1;
let ws;
let configData = {};
let reconnectTimer = null;
let reconnectDelayMs = 1000;
const maxReconnectDelayMs = 30000;

// motorSettings speichert für jeden Motor: side (left/right), invert (bool), deadband (int)
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

function motorMoved(letter, side) {
    // Motor stoppen
    sendWS({["motor"+letter]:"stop"});
    motorSettings[letter].side = side;
    // Weiter
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
    motorSettings[letter].deadband = parseInt(val);
    // Motor stoppen
    let idx = letterToIndex(letter);
    sendWS({"motor":idx,"pwm":0});

    // Nächster Schritt
    if (letter==='A') goToStep(6); // Motor B Start
    else if (letter==='B') goToStep(9); // Motor C
    else if (letter==='C') goToStep(12); // Motor D
    else if (letter==='D') goToStep(15); // Fertig
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

    const drivePair = resolveDrivePair();
    formData.append("motor_left_gui", String(drivePair.leftIdx));
    formData.append("motor_right_gui", String(drivePair.rightIdx));

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

    for (let i=0; i<3; i++){
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
        if (!slider) return;
        const val = (configData.motor_deadband && configData.motor_deadband[i] !== undefined)
            ? configData.motor_deadband[i]
            : parseInt(slider.value || "0", 10);
        slider.value = val;
        updateDeadbandValue(letter);
    });
}

function updateDeadbandValue(letter) {
    const slider = document.getElementById('db_slider_' + letter);
    const label = document.getElementById('db_value_' + letter);
    if (!slider || !label) return;
    label.textContent = slider.value;
}
