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

    // Motoren
    document.getElementById('motor_swap').checked = data.motor_swap;
    const motorsDiv = document.getElementById('motors');
    let motor_invert = data.motor_invert;
    let motor_deadband = data.motor_deadband;
    let motor_frequency = data.motor_frequency;

    let motorLabels = ['A', 'B', 'C', 'D'];
    for (let i=0; i<4; i++) {
      let mDiv = document.createElement('div');
      mDiv.classList.add('motor-block');
      mDiv.innerHTML = `
        <h4>Motor ${motorLabels[i]}</h4>
        <label>Umkehren:</label> <input type="checkbox" name="motor_invert_${i}" ${motor_invert[i] ? 'checked' : ''}><br>
        <label>Deadband:</label> <input type="number" name="motor_deadband_${i}" value="${motor_deadband[i]}" min="0" max="1024"><br>
        <label>Frequenz:</label> <input type="number" name="motor_frequency_${i}" value="${motor_frequency[i]}" min="100" max="100000"><br>
        <!-- Test Buttons für den Motor -->
        <div class="test-buttons">
          <button type="button" onclick="testMotor(${i}, 'forward')">Vorwärts</button>
          <button type="button" onclick="testMotor(${i}, 'backward')">Rückwärts</button>
          <button type="button" onclick="testMotor(${i}, 'stop')">Stopp</button>
        </div>
      `;
      motorsDiv.appendChild(mDiv);
    }

    // LED
    document.getElementById('led_count').value = data.led_count;

    // OTA
    document.getElementById('ota_enabled').checked = data.ota_enabled;

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
    data.servo_settings.forEach((servo, idx) => {
      let sDiv = document.createElement('div');
      sDiv.classList.add('servo-block');
      sDiv.innerHTML = `
        <h4>Servo ${idx}</h4>
        <label>Min Pulse:</label> <input type="number" name="servo${idx}_min" value="${servo.min_pulsewidth}" min="100" max="3000"><br>
        <label>Max Pulse:</label> <input type="number" name="servo${idx}_max" value="${servo.max_pulsewidth}" min="100" max="3000"><br>
        <!-- Test Buttons für den Servo -->
        <div class="test-buttons">
          <button type="button" onclick="testServo(${idx}, 0)">0°</button>
          <button type="button" onclick="testServo(${idx}, 180)">180°</button>
        </div>
      `;
      servosDiv.appendChild(sDiv);
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
  loadConfig();
};
