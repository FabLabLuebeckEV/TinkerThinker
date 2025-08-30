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

// Joystick Variablen
const canvasSize = canvas.width;
const centerX = canvasSize / 2;
const centerY = canvasSize / 2;
let touchX = centerX;
let touchY = centerY;

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
    if (data.motorFaults) {
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

// Touch-Events für den Joystick
function handleTouch(event) {
    event.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const touch = event.touches[0];
    const x = touch.clientX - rect.left;
    const y = touch.clientY - rect.top;

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

function handleTouchEnd() {
    touchX = centerX;
    touchY = centerY;
    draw();
}

// Slider-Event
function handleSliderChange() {
    sendData();
}

// Daten senden an den Server
function sendData() {
    console.log("SendData: " + socket.readyStat);
    const normalizedX = (touchX - centerX) / (canvasSize / 2 - 15);
    const normalizedY = (centerY - touchY) / (canvasSize / 2 - 15);
    const servoAngle = parseInt(servoSlider.value);

    const currentData = {
        x: parseFloat(normalizedX.toFixed(4))*-1,
        y: parseFloat(normalizedY.toFixed(4))*-1,
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

// Event Listener für Touch Canvas
canvas.addEventListener('touchstart', handleTouch);
canvas.addEventListener('touchmove', handleTouch);
canvas.addEventListener('touchend', handleTouchEnd);

// Event Listener für Statusbox Einklappen
statusHeader.addEventListener('click', () => {
    statusContent.classList.toggle('collapsed');
    toggleStatusButton.textContent = statusContent.classList.contains('collapsed') ? '►' : '▼';
    toggleStatusButton.style.transform = statusContent.classList.contains('collapsed') ? 'rotate(0deg)' : 'rotate(180deg)';
});

window.onload = function() {
    connectWebSocket();
    // Initiales Zeichnen des Joysticks
    draw();
    // Regelmäßig Daten senden (alle 200 ms)
    setInterval(sendData, 200);
};




