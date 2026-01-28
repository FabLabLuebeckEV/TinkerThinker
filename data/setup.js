let currentStep = 1;
let ws;
let configData = {};

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
    ws.onopen = () => { console.log("WebSocket connected"); };
    ws.onmessage = (event) => {
        console.log("WS Message:", event.data);
    };
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
    ws.send(JSON.stringify({["motor"+letter]:"forward"}));
    setTimeout(()=>{
        ws.send(JSON.stringify({["motor"+letter]:"stop"}));
        document.getElementById('motor'+letter+'Question').style.display='block';
    },1000);
}

function motorMoved(letter, side) {
    // Motor stoppen
    ws.send(JSON.stringify({["motor"+letter]:"stop"}));
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
    ws.send(JSON.stringify({["motor"+letter]: dir}));
    setTimeout(()=>{
        ws.send(JSON.stringify({["motor"+letter]:"stop"}));
    },1000);
}

function testDeadband(letter) {
    const slider = document.getElementById('db_slider_'+letter);
    let val = slider ? slider.value : 0;
    let idx = letterToIndex(letter);
    // Raw PWM senden (Deadband soll nicht dazwischenfunken)
    ws.send(JSON.stringify({"motor_raw":{"motor":idx,"pwm":parseInt(val)}}));
    setTimeout(()=>{
        ws.send(JSON.stringify({"motor_raw":{"motor":idx,"pwm":0}}));
    },1000);
}

function confirmDeadband(letter) {
    let val = document.getElementById('db_slider_'+letter).value;
    motorSettings[letter].deadband = parseInt(val);
    // Motor stoppen
    let idx = letterToIndex(letter);
    ws.send(JSON.stringify({"motor":idx,"pwm":0}));

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

function finalizeSetup() {
    // POST /config mit allen Werten
    let formData = new FormData();
    // Übernehmen wir aus configData vorhandene Werte:
    formData.append("wifi_mode", configData.wifi_mode);
    formData.append("wifi_ssid", configData.wifi_ssid);
    formData.append("wifi_password", configData.wifi_password);
    const wifiNameInput = document.getElementById('wifi_name');
    const newHotspotName = wifiNameInput ? wifiNameInput.value.trim() : "";
    formData.append("hotspot_ssid", newHotspotName || configData.hotspot_ssid);
    formData.append("hotspot_password", configData.hotspot_password);

    // Motoren: invert, side egal hier, side wurde genutzt um interne Logik zu entscheiden.
    // Wir nehmen invert und deadband. Frequency lassen wir auf default aus config
    let letters = ['A','B','C','D'];
    for (let i=0; i<4; i++){
        let lettr = letters[i];
        if (motorSettings[lettr].invert) {
            formData.append(`motor_invert_${i}`, "on");
        }
        formData.append(`motor_deadband_${i}`, motorSettings[lettr].deadband.toString());

        // frequency beibehalten
        let freq = configData.motor_frequency[i] || 5000;
        formData.append(`motor_frequency_${i}`, freq.toString());
    }

    formData.append("led_count", configData.led_count.toString());
    if (configData.ota_enabled) formData.append("ota_enabled","on");

    for (let i=0; i<3; i++){
        formData.append(`servo${i}_min`, configData.servo_settings[i].min_pulsewidth.toString());
        formData.append(`servo${i}_max`, configData.servo_settings[i].max_pulsewidth.toString());
    }

    fetch('/config',{
        method:'POST',
        body:formData
    }).then(()=>{
        goToStep(16);
        setTimeout(() => {
            fetch('/reboot').catch(()=>{});
        }, 400);
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
