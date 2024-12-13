let currentStep = 1;
let ws;
let configData = {};
let swapNeeded = false;

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
      });
}

function testMotor(letter) {
    // Motor starten (vorwärts)
    ws.send(JSON.stringify({["motor"+letter]:"forward"}));
    setTimeout(()=>{
        document.getElementById('motor'+letter+'Question').style.display='block';
    },1000);
}

function motorMoved(letter, side) {
    // Motor stoppen
    ws.send(JSON.stringify({["motor"+letter]:"stop"}));
    motorSettings[letter].side = side;
    // Weiter
    if (letter==='A') goToStep(3);
    else if (letter==='B') goToStep(6);
    else if (letter==='C') goToStep(9);
    else if (letter==='D') goToStep(12);
}

function directionChosen(letter, dir) {
    // invert = true, wenn backward
    motorSettings[letter].invert = (dir==='backward');
    // Weiter zum Deadband
    if (letter==='A') goToStep(4);
    else if (letter==='B') goToStep(7);
    else if (letter==='C') goToStep(10);
    else if (letter==='D') goToStep(13);
}

function testDeadband(letter) {
    let val = document.getElementById('db_slider_'+letter).value;
    // Motor direkt mit pwm ansteuern:
    // motorIndex: A=0, B=1, C=2, D=3
    let idx = letterToIndex(letter);
    ws.send(JSON.stringify({"motor":idx,"pwm":parseInt(val)}));
}

function confirmDeadband(letter) {
    let val = document.getElementById('db_slider_'+letter).value;
    motorSettings[letter].deadband = parseInt(val);
    // Motor stoppen
    let idx = letterToIndex(letter);
    ws.send(JSON.stringify({"motor":idx,"pwm":0}));

    // Nächster Schritt
    if (letter==='A') goToStep(5); // Motor B Start
    else if (letter==='B') goToStep(8); // Motor C
    else if (letter==='C') goToStep(11); // Motor D
    else if (letter==='D') goToStep(14); // Swap
}

function letterToIndex(letter) {
    // A=0, B=1, C=2, D=3
    return letter.charCodeAt(0) - 65; 
}

function setSwap(value) {
    swapNeeded = value;
    // per WebSocket senden
    ws.send(JSON.stringify({"swap": swapNeeded}));
    // Weiter zum finalen Schritt
    goToStep(15);
}

function finalizeSetup() {
    // POST /config mit allen Werten
    let formData = new FormData();
    // Übernehmen wir aus configData vorhandene Werte:
    formData.append("wifi_mode", configData.wifi_mode);
    formData.append("wifi_ssid", configData.wifi_ssid);
    formData.append("wifi_password", configData.wifi_password);
    formData.append("hotspot_ssid", configData.hotspot_ssid);
    formData.append("hotspot_password", configData.hotspot_password);

    if (swapNeeded) formData.append("motor_swap","on");

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
    });
}
