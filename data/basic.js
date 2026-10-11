const batteryBarFill = document.getElementById('batteryBarFill');
const batteryPercentageElem = document.getElementById('batteryPercentage');
const signalBarsElem = document.getElementById('signalBars');
const signalTextElem = document.getElementById('signalText');
const ledColorPicker = document.getElementById('ledColorPicker');
const deviceNameEl = document.getElementById('deviceName');

const canvas = document.getElementById('touchCanvas');
const ctx = canvas.getContext('2d');

const gripperOpenBtn = document.getElementById('gripperOpenBtn');
const gripperCloseBtn = document.getElementById('gripperCloseBtn');

const canvasSize = canvas.width;
const centerX = canvasSize / 2;
const centerY = canvasSize / 2;
let touchX = centerX;
let touchY = centerY;
let isMouseDown = false;
let ledColorInitialized = false;

let lastSentData = { x: 0, y: 0 };

let socket;
let reconnectInterval = 1000;
const maxReconnectInterval = 30000;
let reconnectAttempts = 0;

function sendWS(payload) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(payload));
    }
}

function updateBatteryUI(pct) {
    const clamped = Math.max(0, Math.min(100, Number(pct) || 0));
    if (batteryPercentageElem) {
        batteryPercentageElem.textContent = `${Math.round(clamped)}%`;
    }
    if (batteryBarFill) {
        batteryBarFill.style.width = `${clamped}%`;
        batteryBarFill.classList.remove('level-low', 'level-med', 'level-high');
        if (clamped <= 20) {
            batteryBarFill.classList.add('level-low');
        } else if (clamped <= 45) {
            batteryBarFill.classList.add('level-med');
        } else {
            batteryBarFill.classList.add('level-high');
        }
    }
}

function rssiToBars(rssi, connected) {
    if (!connected) return 0;
    if (rssi === undefined || rssi === null || rssi === 0) return 4;
    if (rssi >= -60) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -80) return 2;
    return 1;
}

function updateSignalUI(connected, rssi) {
    const barsCount = rssiToBars(rssi, connected);
    if (signalBarsElem) {
        const bars = signalBarsElem.querySelectorAll('.signal-bar');
        bars.forEach((bar, idx) => {
            if (idx < barsCount) {
                bar.classList.add('active');
            } else {
                bar.classList.remove('active');
            }
            bar.classList.toggle('disconnected', !connected);
        });
    }
    if (signalTextElem) {
        if (!connected) {
            signalTextElem.textContent = 'Getrennt';
            signalTextElem.style.color = '#c62828';
        } else {
            signalTextElem.textContent = (rssi && rssi < 0) ? `${rssi} dBm` : 'Verbunden';
            signalTextElem.style.color = '#2e7d32';
        }
    }
}

function rgbToHex(r, g, b) {
    const toHex = (n) => Math.max(0, Math.min(255, Number(n) || 0)).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

function connectWebSocket() {
    const wsUrl = `ws://${window.location.hostname}/ws`;
    socket = new WebSocket(wsUrl);

    socket.onopen = function() {
        reconnectAttempts = 0;
        reconnectInterval = 1000;
        updateSignalUI(true, 0);
    };

    socket.onmessage = (event) => {
        let data;
        try {
            data = JSON.parse(event.data);
        } catch (e) {
            return;
        }
        if (data.batteryPercentage !== undefined) {
            updateBatteryUI(data.batteryPercentage);
        }
        if (data.wifiRssi !== undefined) {
            updateSignalUI(true, data.wifiRssi);
        }
        if (data.firstLED && ledColorPicker && (Date.now() - lastLedSendMs > 500)) {
            const { r, g, b } = data.firstLED;
            ledColorPicker.value = rgbToHex(r, g, b);
        }
    };

    socket.onclose = function() {
        updateSignalUI(false, 0);
        attemptReconnect();
    };

    socket.onerror = function() {
        try {
            socket.close();
        } catch (_) {}
    };
}

function attemptReconnect() {
    if (reconnectAttempts * reconnectInterval >= maxReconnectInterval) {
        return;
    }
    setTimeout(() => {
        reconnectAttempts++;
        connectWebSocket();
        reconnectInterval = Math.min(reconnectInterval * 2, maxReconnectInterval);
    }, reconnectInterval);
}

function draw() {
    ctx.clearRect(0, 0, canvasSize, canvasSize);

    // Dezentes Fadenkreuz zur Orientierung
    ctx.beginPath();
    ctx.moveTo(centerX, 14);
    ctx.lineTo(centerX, canvasSize - 14);
    ctx.moveTo(14, centerY);
    ctx.lineTo(canvasSize - 14, centerY);
    ctx.strokeStyle = '#e5e7eb';
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.closePath();

    // Aeusserer Kreis
    ctx.beginPath();
    ctx.arc(centerX, centerY, canvasSize / 2 - 10, 0, 2 * Math.PI);
    ctx.strokeStyle = '#222';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.closePath();

    // Linie vom Zentrum zur Touchposition
    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.lineTo(touchX, touchY);
    ctx.strokeStyle = '#222';
    ctx.lineWidth = 2.5;
    ctx.stroke();
    ctx.closePath();

    // Joystick-Knopf
    ctx.beginPath();
    ctx.arc(touchX, touchY, 18, 0, 2 * Math.PI);
    ctx.fillStyle = '#4285f4';
    ctx.fill();
    ctx.strokeStyle = '#1a56db';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.closePath();
}

function updateJoystickPosition(clientX, clientY) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = (clientX - rect.left) * scaleX;
    const y = (clientY - rect.top) * scaleY;
    const dx = x - centerX;
    const dy = y - centerY;
    const distance = Math.sqrt(dx * dx + dy * dy);
    const maxRadius = canvasSize / 2 - 18;

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

function handleTouch(event) {
    event.preventDefault();
    const touch = event.touches[0];
    if (!touch) return;
    updateJoystickPosition(touch.clientX, touch.clientY);
}

function handleTouchEnd() {
    touchX = centerX;
    touchY = centerY;
    draw();
    sendDriveData(true);
}

function handleMouseDown(event) {
    event.preventDefault();
    isMouseDown = true;
    updateJoystickPosition(event.clientX, event.clientY);
}

function handleMouseMove(event) {
    if (!isMouseDown) return;
    event.preventDefault();
    updateJoystickPosition(event.clientX, event.clientY);
}

function handleMouseUp() {
    if (!isMouseDown) return;
    isMouseDown = false;
    handleTouchEnd();
}

function sendDriveData(force = false) {
    const maxRadius = canvasSize / 2 - 18;
    const normalizedHorizontal = (touchX - centerX) / maxRadius;
    const normalizedVertical = (centerY - touchY) / maxRadius;

    const currentData = {
        x: parseFloat(normalizedVertical.toFixed(4)),
        y: parseFloat((normalizedHorizontal * -1).toFixed(4))
    };

    if (force || currentData.x !== lastSentData.x || currentData.y !== lastSentData.y) {
        sendWS(currentData);
        lastSentData = currentData;
    }
}

let lastLedSendMs = 0;
function sendSingleLedColor(hex) {
    if (!hex || hex.length !== 7) return;
    lastLedSendMs = Date.now();
    sendWS({
        led_set: { start: 0, count: 1, color: hex.toLowerCase() }
    });
}

function bindGripperButton(button, action) {
    if (!button) return;
    let active = false;

    const start = (e) => {
        e.preventDefault();
        if (active) return;
        active = true;
        button.classList.add('is-active');
        sendWS({ gripper: action });
    };

    const stop = (e) => {
        if (e) e.preventDefault();
        if (!active) return;
        active = false;
        button.classList.remove('is-active');
        sendWS({ gripper: 'stop' });
    };

    button.addEventListener('mousedown', start);
    button.addEventListener('touchstart', start, { passive: false });
    button.addEventListener('mouseup', stop);
    button.addEventListener('mouseleave', stop);
    button.addEventListener('touchend', stop);
    button.addEventListener('touchcancel', stop);
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

canvas.addEventListener('touchstart', handleTouch, { passive: false });
canvas.addEventListener('touchmove', handleTouch, { passive: false });
canvas.addEventListener('touchend', handleTouchEnd);
canvas.addEventListener('touchcancel', handleTouchEnd);
canvas.addEventListener('mousedown', handleMouseDown);
canvas.addEventListener('mousemove', handleMouseMove);
canvas.addEventListener('mouseleave', handleMouseUp);
window.addEventListener('mouseup', handleMouseUp);

bindGripperButton(gripperOpenBtn, 'open');
bindGripperButton(gripperCloseBtn, 'close');

if (ledColorPicker) {
    ledColorPicker.addEventListener('input', () => sendSingleLedColor(ledColorPicker.value));
}

window.onload = function() {
    connectWebSocket();
    setupDeviceName();
    draw();
    setInterval(() => sendDriveData(false), 150);
};
