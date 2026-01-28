// Status Elemente
const batteryVoltageElem = document.getElementById('batteryVoltage');
const batteryPercentageElem = document.getElementById('batteryPercentage');
const servoAnglesElem = document.getElementById('servoAngles');

const motor0PWMElem = document.getElementById('motor0PWM');
const motor1PWMElem = document.getElementById('motor1PWM');
const motor2PWMElem = document.getElementById('motor2PWM');
const motor3PWMElem = document.getElementById('motor3PWM');

const motor0FaultElem = document.getElementById('motor0Fault');

const motor0CurrentElem = document.getElementById('motor0Current');
const motor1CurrentElem = document.getElementById('motor1Current');

const firstLEDR = document.getElementById('firstLEDR');
const firstLEDG = document.getElementById('firstLEDG');
const firstLEDB = document.getElementById('firstLEDB');
const ledColorPicker = document.getElementById('ledColorPicker');
const ledRangeStart = document.getElementById('ledRangeStart');
const ledRangeEnd = document.getElementById('ledRangeEnd');
const ledRangeWrap = document.getElementById('ledRangeWrap');
const ledRangeStartVal = document.getElementById('ledRangeStartVal');
const ledRangeEndVal = document.getElementById('ledRangeEndVal');
const deviceNameEl = document.getElementById('deviceName');

// Steuerung Elemente
const canvas = document.getElementById('touchCanvas');
const ctx = canvas.getContext('2d');
const servoSlider = document.getElementById('servoSlider');

const motorAForwardButton = document.getElementById('motorAForward');
const motorABackwardButton = document.getElementById('motorABackward');

const motorBForwardButton = document.getElementById('motorBForward');
const motorBBackwardButton = document.getElementById('motorBBackward');

// Statusbox Einklappen
const statusHeader = document.querySelector('.status-header');
const toggleStatusButton = document.getElementById('toggleStatus');
const statusContent = document.querySelector('.status-content');

// Zentrierung der Buttons und Verhinderung der Textauswahl
document.querySelectorAll('.control-button').forEach(button => {
    button.addEventListener('touchstart', (e) => {
        e.preventDefault(); // Verhindert die Textauswahl bei Touch
    });
    button.addEventListener('mousedown', (e) => {
        e.preventDefault(); // Verhindert die Textauswahl bei Klick
    });
});

function bindActiveState(button) {
    if (!button) return;
    const on = () => button.classList.add('is-active');
    const off = () => button.classList.remove('is-active');
    button.addEventListener('mousedown', on);
    button.addEventListener('touchstart', on);
    button.addEventListener('mouseup', off);
    button.addEventListener('mouseleave', off);
    button.addEventListener('touchend', off);
    button.addEventListener('touchcancel', off);
}

bindActiveState(motorAForwardButton);
bindActiveState(motorABackwardButton);
bindActiveState(motorBForwardButton);
bindActiveState(motorBBackwardButton);

// Joystick Variablen
const canvasSize = canvas.width;
const centerX = canvasSize / 2;
const centerY = canvasSize / 2;
let touchX = centerX;
let touchY = centerY;
let isMouseDown = false;

let lastSentData = { x: 0, y: 0, servo: 90 };

// WebSocket Verbindung aufbauen
let socket;
let reconnectInterval = 1000; // Initialer Rekonnektion-Intervall in ms
const maxReconnectInterval = 30000; // Maximales Rekonnektion-Intervall in ms
let reconnectAttempts = 0;

// Funktion zum Senden von Nachrichten über WebSocket
function sendWebSocketMessage(message) {
  //if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify(message));
  //} else {
    alert("WebSocket ist nicht verbunden. Bitte versuche es später erneut.");
  //}
}

// Funktion zum Herstellen der WebSocket-Verbindung
function connectWebSocket() {
  const wsUrl = `ws://${window.location.hostname}/ws`;
  socket = new WebSocket(wsUrl);

  socket.onopen = function() {
    console.log("WebSocket verbunden");
    reconnectAttempts = 0; // Zurücksetzen der Versuche nach erfolgreicher Verbindung
    updateConnectionStatus("verbunden");
  };

  socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.batteryVoltage !== undefined) {
        batteryVoltageElem.textContent = data.batteryVoltage.toFixed(2);
    }
    if (data.batteryPercentage !== undefined) {
        batteryPercentageElem.textContent = data.batteryPercentage.toFixed(1);
    }
    if (data.servos) {
        servoAnglesElem.textContent = data.servos.join(', ');
    }
    if (data.motorPWMs) {
        motor0PWMElem.textContent = data.motorPWMs[0];
        motor1PWMElem.textContent = data.motorPWMs[1];
        motor2PWMElem.textContent = data.motorPWMs[2];
        motor3PWMElem.textContent = data.motorPWMs[3];
    }
    if (data.motorFaults && motor0FaultElem) {
        motor0FaultElem.textContent = data.motorFaults ? 'True' : 'False';
    }
    if (data.motorCurrents) {
        motor0CurrentElem.textContent = Math.round(data.motorCurrents[0]*1000);
        motor1CurrentElem.textContent = Math.round(data.motorCurrents[1]*1000);
    }
    if (data.firstLED) {
        firstLEDR.textContent = data.firstLED.r;
        firstLEDG.textContent = data.firstLED.g;
        firstLEDB.textContent = data.firstLED.b;
    }
};

  socket.onclose = function(event) {
    console.log(`WebSocket geschlossen: Code ${event.code}, Grund: ${event.reason}`);
    updateConnectionStatus("geschlossen");
    attemptReconnect();
  };

  socket.onerror = function(error) {
    console.error("WebSocket Fehler:", error);
    socket.close(); // Schließen der Verbindung, um onclose Event auszulösen
  };
}


// Funktion zum Versuch der Rekonnektion
function attemptReconnect() {
    if (reconnectAttempts * reconnectInterval >= maxReconnectInterval) {
      console.log("Maximales Rekonnektion-Intervall erreicht. Weitere Versuche werden eingestellt.");
      return;
    }
  
    setTimeout(() => {
      console.log(`Versuche, WebSocket erneut zu verbinden... (Versuch ${reconnectAttempts + 1})`);
      reconnectAttempts++;
      connectWebSocket();
      reconnectInterval = Math.min(reconnectInterval * 2, maxReconnectInterval); // Exponentielles Backoff
    }, reconnectInterval);
  }

// Funktion zur Aktualisierung des Verbindungsstatus in der UI
function updateConnectionStatus(status) {
    const statusElement = document.getElementById('connectionStatus');
    if (statusElement) {
      const statusText = statusElement.querySelector('span');
      if (statusText) {
        statusText.innerText = status;
        switch(status) {
          case 'verbunden':
            statusText.style.color = 'green';
            break;
          case 'geschlossen':
            statusText.style.color = 'red';
            break;
          default:
            statusText.style.color = 'black';
        }
      }
    }
  }

  
// Zeichnen des Joysticks
function draw() {
    ctx.clearRect(0, 0, canvasSize, canvasSize);

    // Großer Kreis
    ctx.beginPath();
    ctx.arc(centerX, centerY, canvasSize / 2 - 10, 0, 2 * Math.PI);
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.closePath();

    // Linie vom Zentrum zur Touchposition
    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.lineTo(touchX, touchY);
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.closePath();

    // Kleiner Kreis an der Touchposition
    ctx.beginPath();
    ctx.arc(touchX, touchY, 15, 0, 2 * Math.PI);
    ctx.fillStyle = '#4285f4';
    ctx.fill();
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.closePath();
}

function updateJoystickPosition(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const x = clientX - rect.left;
    const y = clientY - rect.top;
    const dx = x - centerX;
    const dy = y - centerY;
    const distance = Math.sqrt(dx * dx + dy * dy);
    const maxRadius = canvasSize / 2 - 15;

    if (distance > maxRadius) {
        const angle = Math.atan2(dy, dx);
        touchX = centerX + maxRadius * Math.cos(angle);
        touchY = centerY + maxRadius * Math.sin(angle);
    } else {
        touchX = x;
        touchY = y;
    }

    draw();
}

// Touch-Events für den Joystick
function handleTouch(event) {
    event.preventDefault();
    const touch = event.touches[0];
    if (!touch) {
        return;
    }
    updateJoystickPosition(touch.clientX, touch.clientY);
}

function handleTouchEnd() {
    touchX = centerX;
    touchY = centerY;
    draw();
}

// Maus-Events für den Joystick
function handleMouseDown(event) {
    event.preventDefault();
    isMouseDown = true;
    updateJoystickPosition(event.clientX, event.clientY);
}

function handleMouseMove(event) {
    if (!isMouseDown) {
        return;
    }
    event.preventDefault();
    updateJoystickPosition(event.clientX, event.clientY);
}

function handleMouseUp() {
    if (!isMouseDown) {
        return;
    }
    isMouseDown = false;
    handleTouchEnd();
}

function handleMouseLeave() {
    if (!isMouseDown) {
        return;
    }
    isMouseDown = false;
    handleTouchEnd();
}

// Slider-Event
function handleSliderChange() {
    sendData();
}

// Daten senden an den Server
function sendData() {
    console.log("SendData: " + socket.readyStat);
    const normalizedHorizontal = (touchX - centerX) / (canvasSize / 2 - 15);
    const normalizedVertical = (centerY - touchY) / (canvasSize / 2 - 15);
    const servoAngle = parseInt(servoSlider.value);

    const currentData = {
        x: parseFloat(normalizedVertical.toFixed(4)),
        y: parseFloat(normalizedHorizontal.toFixed(4)) * -1,
        servo0: servoAngle
    };

    //if (socket.readyState === WebSocket.OPEN) {
        if (
            currentData.x !== lastSentData.x ||
            currentData.y !== lastSentData.y ||
            currentData.servo0 !== lastSentData.servo0
        ) {
            console.log(JSON.stringify(currentData));
            socket.send(JSON.stringify(currentData));
            lastSentData = currentData;
        }
    //}
}

function hexToRgb(hex) {
    const h = hex.replace('#', '');
    if (h.length !== 6) return null;
    const r = parseInt(h.slice(0, 2), 16);
    const g = parseInt(h.slice(2, 4), 16);
    const b = parseInt(h.slice(4, 6), 16);
    if (Number.isNaN(r) || Number.isNaN(g) || Number.isNaN(b)) return null;
    return { r, g, b };
}

function sendLedColor(hex) {
    const rgb = hexToRgb(hex);
    if (!rgb) return;
    const range = getLedRange();
    const data = {
        led_set: { start: range.start, count: range.count, color: hex.toLowerCase() }
    };
    socket.send(JSON.stringify(data));
}

// Motor C Steuerung
function controlMotorA(direction) {
    //if (socket.readyState === WebSocket.OPEN) {
        const data = {
            motorA: direction
        };
        socket.send(JSON.stringify(data));
        console.log('Sent motorA command:', data);
    //}
}

// Motor C Steuerung
function controlMotorB(direction) {
    //if (socket.readyState === WebSocket.OPEN) {
        const data = {
            motorB: direction
        };
        socket.send(JSON.stringify(data));
        console.log('Sent motorB command:', data);
    //}
}

// Event Listener für Motor B Buttons
motorBForwardButton.addEventListener('mousedown', () => {
    controlMotorB('forward');
});

motorBForwardButton.addEventListener('touchstart', (e) => {
    e.preventDefault(); // Verhindert Textauswahl und andere Standardaktionen
    controlMotorB('forward');
});

motorBForwardButton.addEventListener('mouseup', () => {
    controlMotorB('stop');
});

motorBForwardButton.addEventListener('touchend', () => {
    controlMotorB('stop');
});

motorBBackwardButton.addEventListener('mousedown', () => {
    controlMotorB('backward');
});

motorBBackwardButton.addEventListener('touchstart', (e) => {
    e.preventDefault(); // Verhindert Textauswahl und andere Standardaktionen
    controlMotorB('backward');
});

motorBBackwardButton.addEventListener('mouseup', () => {
    controlMotorB('stop');
});

motorBBackwardButton.addEventListener('touchend', () => {
    controlMotorB('stop');
});

// Event Listener für Motor C Buttons
motorAForwardButton.addEventListener('mousedown', () => {
    controlMotorA('forward');
});

motorAForwardButton.addEventListener('touchstart', (e) => {
    e.preventDefault(); // Verhindert Textauswahl und andere Standardaktionen
    controlMotorA('forward');
});

motorAForwardButton.addEventListener('mouseup', () => {
    controlMotorA('stop');
});

motorAForwardButton.addEventListener('touchend', () => {
    controlMotorA('stop');
});

motorABackwardButton.addEventListener('mousedown', () => {
    controlMotorA('backward');
});

motorABackwardButton.addEventListener('touchstart', (e) => {
    e.preventDefault(); // Verhindert Textauswahl und andere Standardaktionen
    controlMotorA('backward');
});

motorABackwardButton.addEventListener('mouseup', () => {
    controlMotorA('stop');
});

motorABackwardButton.addEventListener('touchend', () => {
    controlMotorA('stop');
});

// Event Listener für Servo Slider
servoSlider.addEventListener('input', handleSliderChange);

if (ledColorPicker) {
    ledColorPicker.addEventListener('input', () => sendLedColor(ledColorPicker.value));
}

function getLedRange() {
    const start = ledRangeStart ? parseInt(ledRangeStart.value || '0', 10) : 0;
    const end = ledRangeEnd ? parseInt(ledRangeEnd.value || '0', 10) : start;
    const safeStart = Math.min(start, end);
    const safeEnd = Math.max(start, end);
    return { start: safeStart, count: (safeEnd - safeStart + 1) };
}

function syncLedRangeLabels() {
    if (!ledRangeStart || !ledRangeEnd) return;
    let start = parseInt(ledRangeStart.value || '0', 10);
    let end = parseInt(ledRangeEnd.value || '0', 10);
    if (start > end) {
        end = start;
        ledRangeEnd.value = String(end);
    }
    if (ledRangeStartVal) ledRangeStartVal.textContent = String(start);
    if (ledRangeEndVal) ledRangeEndVal.textContent = String(end);
    updateLedRangeTrack(start, end);
}

function updateLedRangeTrack(start, end) {
    if (!ledRangeWrap || !ledRangeStart) return;
    const max = parseInt(ledRangeStart.max || '1', 10);
    if (max <= 0) {
        ledRangeWrap.style.setProperty('--min-pct', '0%');
        ledRangeWrap.style.setProperty('--max-pct', '0%');
        return;
    }
    const minPct = Math.round((start / max) * 100);
    const maxPct = Math.round((end / max) * 100);
    ledRangeWrap.style.setProperty('--min-pct', `${minPct}%`);
    ledRangeWrap.style.setProperty('--max-pct', `${maxPct}%`);
}

function setupLedRange() {
    if (!ledRangeStart || !ledRangeEnd) return;
    ledRangeStart.addEventListener('input', syncLedRangeLabels);
    ledRangeEnd.addEventListener('input', syncLedRangeLabels);
    syncLedRangeLabels();
    fetch('/getConfig')
        .then(r => r.json())
        .then(cfg => {
            const count = parseInt(cfg.led_count || '0', 10);
            if (!Number.isNaN(count) && count > 0) {
                const max = String(count - 1);
                ledRangeStart.max = max;
                ledRangeEnd.max = max;
                ledRangeStart.value = '0';
                ledRangeEnd.value = max;
                syncLedRangeLabels();
            }
        })
        .catch(() => {});
}

function setupDeviceName() {
    fetch('/getConfig')
        .then(r => r.json())
        .then(cfg => {
            const name = (cfg.hotspot_ssid || cfg.wifi_ssid || 'TinkerThinker').trim();
            if (deviceNameEl) deviceNameEl.textContent = name;
            document.title = `${name} Steuerung`;
        })
        .catch(() => {});
}

// Event Listener für Touch Canvas
canvas.addEventListener('touchstart', handleTouch);
canvas.addEventListener('touchmove', handleTouch);
canvas.addEventListener('touchend', handleTouchEnd);
canvas.addEventListener('mousedown', handleMouseDown);
canvas.addEventListener('mousemove', handleMouseMove);
canvas.addEventListener('mouseleave', handleMouseLeave);
window.addEventListener('mouseup', handleMouseUp);

// Event Listener für Statusbox Einklappen
statusHeader.addEventListener('click', () => {
    statusContent.classList.toggle('collapsed');
    toggleStatusButton.textContent = statusContent.classList.contains('collapsed') ? '►' : '▼';
    toggleStatusButton.style.transform = statusContent.classList.contains('collapsed') ? 'rotate(0deg)' : 'rotate(180deg)';
});

window.onload = function() {
    connectWebSocket();
    setupLedRange();
    setupDeviceName();
    if (statusContent.classList.contains('collapsed')) {
        toggleStatusButton.textContent = '►';
        toggleStatusButton.style.transform = 'rotate(0deg)';
    }
    // Initiales Zeichnen des Joysticks
    draw();
    // Regelmäßig Daten senden (alle 200 ms)
    setInterval(sendData, 200);
};




