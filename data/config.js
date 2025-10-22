// WebSocket-Parameter
let socket;
let reconnectInterval = 1000; // Initialer Rekonnektion-Intervall in ms
const maxReconnectInterval = 30000; // Maximales Rekonnektion-Intervall in ms
let reconnectAttempts = 0;

// Funktion zum Senden von Nachrichten über WebSocket
function sendWebSocketMessage(message) {
  //if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify(message));
  //} else {
  //  alert("WebSocket ist nicht verbunden. Bitte versuche es später erneut.");
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

  socket.onmessage = function(event) {
    let data;
    try {
      data = JSON.parse(event.data);
    } catch (e) {
      console.error("Fehler beim Parsen der WebSocket-Nachricht:", e);
      return;
    }

    if (data.restart) {
      // Anzeigen des Neustart-Popups
      document.getElementById('newWifiInfo').innerText = `
        WLAN Modus: ${document.getElementById('wifi_mode').value}
        SSID: ${document.getElementById('wifi_mode').value === 'AP' ? document.getElementById('hotspot_ssid').value : document.getElementById('wifi_ssid').value}
      `;
      document.getElementById('restartPopup').style.display = 'block';
      // Trigger Neustart nach kurzer Verzögerung
      setTimeout(() => {
        window.location.href = "/reboot";
      }, 3000);
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

// Test Motor Funktion
function testMotor(motorIndex, action) {
  let motorChar = String.fromCharCode(65 + motorIndex); // 'A', 'B', etc.
  let command = {};
  command[`motor${motorChar}`] = action; // Beispiel: { "motorA": "forward" }
  sendWebSocketMessage(command);
}

// Direkte PWM-Testfunktion
function testMotorPWM(motorIndex, pwm) {
  let motorChar = String.fromCharCode(65 + motorIndex);
  let command = {};
  command[`motor${motorChar}`] = String(parseInt(pwm, 10));
  sendWebSocketMessage(command);
}

// Test Servo Funktion
function testServo(servoIndex, angle) {
  let command = {};
  command[`servo${servoIndex}`] = angle; // Beispiel: { "servo0": 0 } oder { "servo1": 180 }
  sendWebSocketMessage(command);
}

// Funktion zum Umschalten der WLAN Felder
function toggleWifiFields() {
  const mode = document.getElementById('wifi_mode').value;
  if (mode === 'AP') {
    document.getElementById('ap_fields').style.display = 'block';
    document.getElementById('sta_fields').style.display = 'none';
  } else {
    document.getElementById('ap_fields').style.display = 'none';
    document.getElementById('sta_fields').style.display = 'block';
  }
}

function updateWifiDisableUI(disabled) {
  const btn = document.getElementById('wifi_disable_btn');
  const notice = document.getElementById('wifi_disable_notice');
  if (!btn || !notice) return;
  if (disabled) {
    btn.disabled = true;
    btn.textContent = 'WLAN deaktiviert (bis Neustart)';
    notice.style.display = 'block';
  } else {
    btn.disabled = false;
    btn.textContent = 'WLAN deaktivieren (bis Neustart)';
    notice.style.display = 'none';
  }
}

async function handleWifiDisableClick() {
  const btn = document.getElementById('wifi_disable_btn');
  const notice = document.getElementById('wifi_disable_notice');
  if (!btn || !notice) return;
  if (btn.disabled) return;

  if (!confirm('WLAN wirklich deaktivieren? Die Weboberfläche ist danach bis zum Neustart nicht erreichbar.')) {
    return;
  }

  btn.disabled = true;
  btn.textContent = 'WLAN wird deaktiviert...';
  notice.style.display = 'block';
  notice.textContent = 'WLAN wird deaktiviert. Die Verbindung bricht gleich ab.';

  try {
    const res = await fetch('/wifi/disable', { method: 'POST' });
    if (res.ok) {
      updateWifiDisableUI(true);
      notice.textContent = 'WLAN wird deaktiviert. Bitte Gerät neu starten, um WLAN wieder einzuschalten.';
    } else if (res.status === 409) {
      updateWifiDisableUI(true);
      notice.textContent = 'WLAN ist bereits deaktiviert. Bitte Gerät neu starten.';
    } else {
      throw new Error(`HTTP ${res.status}`);
    }
  } catch (error) {
    console.error('Fehler beim Deaktivieren des WLAN:', error);
    notice.textContent = 'Verbindung getrennt – vermutlich wurde WLAN deaktiviert.';
  }
}

function setupWifiDisableButton() {
  const btn = document.getElementById('wifi_disable_btn');
  if (btn) {
    btn.addEventListener('click', handleWifiDisableClick);
  }
}

function setTurnGainUI(val) {
  const num = document.getElementById('drive_turn_gain');
  const slider = document.getElementById('drive_turn_gain_slider');
  const label = document.getElementById('drive_turn_gain_val');
  if (!num || !slider || !label) return;
  let value = parseFloat(val);
  if (isNaN(value)) value = 1.0;
  if (value < 0) value = 0;
  if (value > 2.5) value = 2.5;
  num.value = value.toFixed(2);
  slider.value = Math.round(value * 100);
  label.textContent = value.toFixed(2);
}

function setCurveStrengthUI(val) {
  const num = document.getElementById('motor_curve_strength');
  const slider = document.getElementById('motor_curve_strength_slider');
  const label = document.getElementById('motor_curve_strength_val');
  if (!num || !slider || !label) return;
  let value = parseFloat(val);
  if (isNaN(value)) value = 0;
  if (value < -0.8) value = -0.8;
  if (value > 3.0) value = 3.0;
  num.value = value.toFixed(2);
  slider.value = Math.round(value * 100);
  label.textContent = value.toFixed(2);
}

function updateCurveStrengthDisabled() {
  const typeSel = document.getElementById('motor_curve_type');
  const num = document.getElementById('motor_curve_strength');
  const slider = document.getElementById('motor_curve_strength_slider');
  const disabled = typeSel && typeSel.value !== 'expo';
  if (num) num.disabled = disabled;
  if (slider) slider.disabled = disabled;
}

function setupDriveProfileControls() {
  const turnSlider = document.getElementById('drive_turn_gain_slider');
  const turnNum = document.getElementById('drive_turn_gain');
  if (turnSlider) {
    turnSlider.addEventListener('input', () => {
      setTurnGainUI(parseInt(turnSlider.value, 10) / 100);
    });
  }
  if (turnNum) {
    turnNum.addEventListener('input', () => {
      setTurnGainUI(turnNum.value);
    });
  }

  const curveSlider = document.getElementById('motor_curve_strength_slider');
  const curveNum = document.getElementById('motor_curve_strength');
  if (curveSlider) {
    curveSlider.addEventListener('input', () => {
      setCurveStrengthUI(parseInt(curveSlider.value, 10) / 100);
    });
  }
  if (curveNum) {
    curveNum.addEventListener('input', () => {
      setCurveStrengthUI(curveNum.value);
    });
  }

  const curveType = document.getElementById('motor_curve_type');
  if (curveType) {
    curveType.addEventListener('change', updateCurveStrengthDisabled);
  }
}

// Collapsible für erweiterte Einstellungen
function setupCollapsibles() {
  var coll = document.getElementsByClassName("collapsible");
  for (let i = 0; i < coll.length; i++) {
    coll[i].addEventListener("click", function() {
      this.classList.toggle("active");
      // Korrekte Suche des .content Elements innerhalb des übergeordneten <section> Elements
      var content = this.closest('section').querySelector(".content");
      if (content.style.display === "block") {
        content.style.display = "none";
      } else {
        content.style.display = "block";
      }
    });
  }
}

// Laden der Config
async function loadConfig() {
  try {
    let res = await fetch('/getConfig');
    if (!res.ok) {
      throw new Error(`HTTP error! status: ${res.status}`);
    }
    let data = await res.json();

    // WiFi
    document.getElementById('wifi_mode').value = data.wifi_mode;
    toggleWifiFields();
    document.getElementById('wifi_ssid').value = data.wifi_ssid;
    document.getElementById('wifi_password').value = data.wifi_password;
    document.getElementById('hotspot_ssid').value = data.hotspot_ssid;
    document.getElementById('hotspot_password').value = data.hotspot_password;
    updateWifiDisableUI(!!data.wifi_disabled_until_restart);

    // Motoren
    document.getElementById('motor_swap').checked = data.motor_swap;
    const motorsDiv = document.getElementById('motors');
    motorsDiv.innerHTML = '';
    let motor_invert = data.motor_invert;
    let motor_deadband = data.motor_deadband;
    let motor_frequency = data.motor_frequency;

    let motorLabels = ['A', 'B', 'C', 'D'];
    for (let i=0; i<4; i++) {
      let mDiv = document.createElement('div');
      mDiv.classList.add('motor-block');
      mDiv.innerHTML = `
        <h4>Motor ${motorLabels[i]}</h4>
        <div class="row">
          <label>Umkehren:</label> <input type="checkbox" name="motor_invert_${i}" ${motor_invert[i] ? 'checked' : ''}>
        </div>
        <div class="row" style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
          <label for="motor_deadband_${i}">Deadband:</label>
          <input type="number" id="motor_deadband_${i}" name="motor_deadband_${i}" value="${motor_deadband[i]}" min="0" max="255" style="width:90px;">
          <input type="range" id="db_slider_${i}" min="0" max="255" value="${Math.min(255, Math.max(0, motor_deadband[i]))}">
          <span id="db_val_${i}">${motor_deadband[i]}</span>
          <button type="button" onclick="testMotorPWM(${i}, document.getElementById('motor_deadband_${i}').value)">Test Vorwärts (DB)</button>
          <button type="button" onclick="testMotorPWM(${i}, -document.getElementById('motor_deadband_${i}').value)">Test Rückwärts (DB)</button>
        </div>
        <div class="row">
          <label>Frequenz:</label> <input type="number" name="motor_frequency_${i}" value="${motor_frequency[i]}" min="100" max="100000">
        </div>
        <div class="row test-buttons">
          <button type="button" onclick="testMotor(${i}, 'forward')">Vorwärts</button>
          <button type="button" onclick="testMotor(${i}, 'backward')">Rückwärts</button>
          <button type="button" onclick="testMotor(${i}, 'stop')">Stopp</button>
        </div>
      `;
      motorsDiv.appendChild(mDiv);
      // Slider <-> Number sync
      const num = mDiv.querySelector(`#motor_deadband_${i}`);
      const slider = mDiv.querySelector(`#db_slider_${i}`);
      const lbl = mDiv.querySelector(`#db_val_${i}`);
      const sync = (fromSlider) => {
        if (fromSlider) num.value = slider.value; else slider.value = Math.min(255, Math.max(0, parseInt(num.value||'0',10)));
        lbl.textContent = num.value;
      };
      num.addEventListener('input', () => sync(false));
      slider.addEventListener('input', () => sync(true));
      sync(true);
    }

    // LED
    document.getElementById('led_count').value = data.led_count;
    const ledNum = document.getElementById('led_count');
    const ledSl = document.getElementById('led_count_slider');
    const ledLbl = document.getElementById('led_count_val');
    ledSl.value = Math.max(parseInt(ledNum.min||'1',10), Math.min(parseInt(ledNum.max||'300',10), data.led_count||30));
    ledLbl.textContent = ledNum.value;
    const syncLed = (fromSlider)=>{ if (fromSlider) ledNum.value = ledSl.value; else ledSl.value = ledNum.value; ledLbl.textContent = ledNum.value; };
    ledNum.addEventListener('input', ()=>syncLed(false));
    ledSl.addEventListener('input', ()=>syncLed(true));

    // OTA
    document.getElementById('ota_enabled').checked = data.ota_enabled;

    if (document.getElementById('drive_mixer')) {
      document.getElementById('drive_mixer').value = data.drive_mixer || 'arcade';
    }
    setTurnGainUI(data.drive_turn_gain !== undefined ? data.drive_turn_gain : 1.0);
    if (document.getElementById('drive_axis_deadband')) {
      document.getElementById('drive_axis_deadband').value = data.drive_axis_deadband !== undefined ? data.drive_axis_deadband : 16;
    }
    if (document.getElementById('motor_curve_type')) {
      document.getElementById('motor_curve_type').value = data.motor_curve_type || 'linear';
    }
    setCurveStrengthUI(data.motor_curve_strength !== undefined ? data.motor_curve_strength : 0.0);
    updateCurveStrengthDisabled();

    // BT scan timings
    if (typeof data.bt_scan_on_normal_ms !== 'undefined') {
      document.getElementById('bt_scan_on_normal_ms').value = data.bt_scan_on_normal_ms;
      document.getElementById('bt_scan_off_normal_ms').value = data.bt_scan_off_normal_ms;
      document.getElementById('bt_scan_on_sta_ms').value = data.bt_scan_on_sta_ms;
      document.getElementById('bt_scan_off_sta_ms').value = data.bt_scan_off_sta_ms;
      document.getElementById('bt_scan_on_ap_ms').value = data.bt_scan_on_ap_ms;
      document.getElementById('bt_scan_off_ap_ms').value = data.bt_scan_off_ap_ms;
    }

    // Servos
    const servosDiv = document.getElementById('servos');
    servosDiv.innerHTML = '';
    data.servo_settings.forEach((servo, idx) => {
      let sDiv = document.createElement('div');
      sDiv.classList.add('servo-block');
      sDiv.innerHTML = `
        <h4>Servo ${idx}</h4>
        <div class="row" style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
          <label for="servo${idx}_min">Min Pulse:</label>
          <input type="number" id="servo${idx}_min" name="servo${idx}_min" value="${servo.min_pulsewidth}" min="100" max="3000" style="width:100px;">
          <input type="range" id="servo${idx}_min_slider" min="100" max="3000" value="${servo.min_pulsewidth}"> <span id="servo${idx}_min_val">${servo.min_pulsewidth}</span>
        </div>
        <div class="row" style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
          <label for="servo${idx}_max">Max Pulse:</label>
          <input type="number" id="servo${idx}_max" name="servo${idx}_max" value="${servo.max_pulsewidth}" min="100" max="3000" style="width:100px;">
          <input type="range" id="servo${idx}_max_slider" min="100" max="3000" value="${servo.max_pulsewidth}"> <span id="servo${idx}_max_val">${servo.max_pulsewidth}</span>
        </div>
        <div class="test-buttons">
          <button type="button" onclick="testServo(${idx}, 0)">0°</button>
          <button type="button" onclick="testServo(${idx}, 90)">90°</button>
          <button type="button" onclick="testServo(${idx}, 180)">180°</button>
        </div>
      `;
      servosDiv.appendChild(sDiv);
      // Servo sliders sync
      const minNum = sDiv.querySelector(`#servo${idx}_min`);
      const minS = sDiv.querySelector(`#servo${idx}_min_slider`);
      const minV = sDiv.querySelector(`#servo${idx}_min_val`);
      const maxNum = sDiv.querySelector(`#servo${idx}_max`);
      const maxS = sDiv.querySelector(`#servo${idx}_max_slider`);
      const maxV = sDiv.querySelector(`#servo${idx}_max_val`);
      const syncMin = (fromSlider)=>{ if(fromSlider) minNum.value=minS.value; else minS.value=minNum.value; minV.textContent=minNum.value; };
      const syncMax = (fromSlider)=>{ if(fromSlider) maxNum.value=maxS.value; else maxS.value=maxNum.value; maxV.textContent=maxNum.value; };
      minNum.addEventListener('input', ()=>syncMin(false));
      minS.addEventListener('input', ()=>syncMin(true));
      maxNum.addEventListener('input', ()=>syncMax(false));
      maxS.addEventListener('input', ()=>syncMax(true));
      syncMin(true); syncMax(true);
    });
  } catch (error) {
    console.error("Fehler beim Laden der Konfiguration:", error);
    alert("Fehler beim Laden der Konfiguration. Bitte versuche es später erneut.");
  }
}

// Event Listener für das Formular
document.getElementById('configForm').addEventListener('submit', function(e) {
  // Validierung WLAN Passwort
  let mode = document.getElementById('wifi_mode').value;
  if (mode === 'STA') {
    let pass = document.getElementById('wifi_password').value;
    if (pass.length < 8) {
      alert("Das WiFi-Passwort muss mindestens 8 Zeichen lang sein!");
      e.preventDefault();
      return;
    }
  }else if (mode === 'AP') {
    let pass = document.getElementById('hotspot_password').value;
    if (pass.length < 8 && pass.length > 0) {
      alert("Das Hotspot-Passwort muss mindestens 8 Zeichen lang sein!");
      e.preventDefault();
      return;
    }
  }
  // Weitere Validierungen können hier hinzugefügt werden
  
});

// Reset Config Funktion
function resetConfig() {
  if (confirm("Bist du sicher, dass du auf Werkseinstellungen zurücksetzen möchtest?")) {
    window.location.href = "/resetconfig";
  }
}

// Reboot Funktion mit Popup
function reboot() {
  // Abrufen der aktuellen WLAN Einstellungen
  fetch('/getConfig').then(response => {
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    return response.json();
  }).then(data => {
    document.getElementById('newWifiInfo').innerText = `
      WLAN Modus: ${data.wifi_mode}
      SSID: ${data.wifi_mode === 'AP' ? data.hotspot_ssid : data.wifi_ssid}
    `;
    document.getElementById('restartPopup').style.display = 'block';
    // Trigger Neustart nach kurzer Verzögerung
    setTimeout(() => {
      window.location.href = "/reboot";
    }, 3000);
  }).catch(error => {
    console.error("Fehler beim Abrufen der Konfiguration:", error);
    alert("Fehler beim Abrufen der Konfiguration. Bitte versuche es später erneut.");
  });
}

// Funktion zum Schließen des Popups
function closePopup() {
  document.getElementById('restartPopup').style.display = 'none';
}

// Initialisieren der WebSocket-Verbindung und Collapsibles beim Laden der Seite
window.onload = function() {
  connectWebSocket();
  setupCollapsibles();
  setupWifiDisableButton();
  setupDriveProfileControls();
  loadConfig();
};
