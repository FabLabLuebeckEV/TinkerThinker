// TinkerThinker Roboter-Arm Steuerung, Kinematik-Animation & Endanschlag-Homing
// Hardware-Aufbau:
// - 4 DC-Motoren am Board, aber nur 2 Hardware-Stromsensoren:
//   * Sensor 1 (S1, GPIO 34) -> Motor B (Standard: Arm 2 / Mittelarm)
//   * Sensor 2 (S2, GPIO 36) -> Motor A (Standard: Arm 1 / Unterarm)
//   * Motor C (Basis)        -> KEIN Stromsensor, KEINE Winkelbegrenzung (360° endlos)
//   * Motor D (Arm 3)        -> KEIN Stromsensor (Position & Homing über Laufzeit am Endanschlag)
// - Da kein Positions-Feedback (kein Encoder/Poti) verbaut ist, wird die Position zwischen
//   den Anschlägen über die Laufzeit geschätzt ("geraten") und Home liegt direkt in einem
//   mechanischen Endanschlag (Max- oder Min-Position). Im Blockierfall (Stall) zieht der
//   Motor maximalen Strom (I_stall).

let socket = null;
let reconnectInterval = 1000;
const maxReconnectInterval = 30000;
let reconnectAttempts = 0;
let lastLedSendMs = 0;

// Live-Strommesswerte vom ESP32 (2 Hardware-Kanäle: [0]=Sensor 1 / Motor B, [1]=Sensor 2 / Motor A)
let boardCurrents = [0.0, 0.0];
// Gemessener Ruhestrom-Offset (Baseline bei stehenden Motoren), verhindert Fehlauslösungen ohne Motoren
let idleCurrents = [0.0, 0.0];
let idleSampleInitialized = false;
let boardPWMs = [0, 0, 0, 0];

// Globales Tempo (30..100 %)
let globalSpeedPct = 80;

// Greifer-Zustand (Servo)
const gripperState = {
    servoIndex: 0,
    angle: 90,
    openAngle: 35,
    closeAngle: 145,
    movingDir: 0 // -1 = öffnet, +1 = schließt, 0 = steht
};

// Kinematik- & Konfigurations-Zustand der 4 Achsen
// Vor dem ersten Homing stehen die 3 Armteile auf einer geschätzten Mittelposition,
// damit man sofort sieht, dass die Position geschätzt ist, und Homing sanft in den Endanschlag fährt.
const joints = {
    base: {
        id: 'base',
        label: 'Basis',
        motorIndex: 2,      // Motor C (M2) - ohne Stromsensor
        invert: false,
        hasLimits: false,   // 360° endlos ohne Begrenzung
        sensorMode: 'none', // Basis braucht keine Strommessung
        stallLimit: 99.0,
        time90: 3.0,        // Sekunden für 90° Drehung bei 100% PWM
        angle: 0.0,         // Geschätzter Winkel (0..360°)
        visualAngle: 0.0,   // Sanft geglätteter Winkel für die Animation (kein Springen)
        homeEnd: 'max',
        activeDir: 0,
        activePwm: 0,
        moveStartMs: 0,
        stalledDir: 0,
        lastCurrent: 0.0,
        isHomed: true
    },
    arm1: {
        id: 'arm1',
        label: 'Arm 1 (Unterarm)',
        motorIndex: 0,      // Motor A (M0) -> hat Hardware-Stromsensor 2 (GPIO 36)
        invert: false,
        hasLimits: true,
        minDeg: -90.0,      // Min-Anschlag (-90° links waagerecht)
        maxDeg: 90.0,       // Max-Anschlag (+90° rechts waagerecht, 180° Gesamtbereich)
        homeEnd: 'max',     // Home liegt im Max- oder Min-Endanschlag
        sensorMode: '1',    // Sensor 2 (Index 1 = GPIO 36 / Motor A)
        stallLimit: 1.10,   // Maximalstrom im Stall (A)
        time90: 3.5,
        angle: 0.0,         // Startpose senkrecht nach oben (0°)
        visualAngle: 0.0,
        activeDir: 0,
        activePwm: 0,
        moveStartMs: 0,
        stalledDir: 0,
        lastCurrent: 0.0,
        isHomed: false
    },
    arm2: {
        id: 'arm2',
        label: 'Arm 2 (Mittelarm)',
        motorIndex: 1,      // Motor B (M1) -> hat Hardware-Stromsensor 1 (GPIO 34)
        invert: false,
        hasLimits: true,
        minDeg: -90.0,      // Min-Anschlag (-90°)
        maxDeg: 90.0,       // Max-Anschlag (+90°, 180° Gesamtbereich)
        homeEnd: 'max',     // Home im Max- oder Min-Endanschlag
        sensorMode: '0',    // Sensor 1 (Index 0 = GPIO 34 / Motor B)
        stallLimit: 1.05,   // Maximalstrom im Stall (A)
        time90: 3.2,
        angle: 0.0,         // Startpose in Mittelstellung (0°)
        visualAngle: 0.0,
        activeDir: 0,
        activePwm: 0,
        moveStartMs: 0,
        stalledDir: 0,
        lastCurrent: 0.0,
        isHomed: false
    },
    arm3: {
        id: 'arm3',
        label: 'Arm 3 (Handgelenk)',
        motorIndex: 3,      // Motor D (M3) -> KEIN Hardware-Stromsensor
        invert: false,
        hasLimits: true,
        minDeg: -90.0,      // Min-Anschlag (-90° über die linke Seite gerade nach unten)
        maxDeg: 90.0,       // Max-Anschlag (+90° Startposition gerade nach oben, 180° Bereich)
        homeEnd: 'max',     // Home im Max-Endanschlag (+90°)
        sensorMode: 'none', // Kein Sensor vorhanden (nur 2 Sensoren auf dem Board!)
        stallLimit: 0.95,
        time90: 2.2,
        angle: 90.0,        // Startpose am Maximum (+90° gerade nach oben)
        visualAngle: 90.0,
        activeDir: 0,
        activePwm: 0,
        moveStartMs: 0,
        stalledDir: 0,
        lastCurrent: 0.0,
        isHomed: false
    }
};

// Homing-State-Machine (fährt direkt in den gewählten Min- oder Max-Endanschlag)
const homing = {
    active: false,
    queue: [],
    currentJointId: null,
    phase: 'idle', // 'seek_endstop', 'pause'
    phaseStartMs: 0,
    targetDurationMs: 0,
    targetDir: 1,
    targetAngle: 0
};

const STORAGE_KEY = 'tinkerthinker_arm_config_v4';

function getHomeAngle(joint) {
    if (!joint.hasLimits) return 0.0;
    return (joint.homeEnd === 'min') ? joint.minDeg : joint.maxDeg;
}

function getHomeDir(joint) {
    return (joint.homeEnd === 'min') ? -1 : 1;
}

// --- WebSocket & Telemetrie ---

function sendWS(payload) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(payload));
        return true;
    }
    return false;
}

function updateSignalDisplay(rssi, connected) {
    const barsEl = document.getElementById('signalBars');
    const textEl = document.getElementById('signalText');
    if (!barsEl || !textEl) return;

    if (!connected) {
        barsEl.setAttribute('data-bars', '0');
        textEl.textContent = 'Getrennt';
        textEl.style.color = '#ef4444';
        return;
    }

    let bars = 4;
    if (typeof rssi === 'number' && rssi < 0) {
        if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
        textEl.textContent = `${rssi} dBm`;
    } else {
        bars = 4;
        textEl.textContent = 'Online';
    }
    textEl.style.color = '#cbd5e1';
    barsEl.setAttribute('data-bars', String(bars));
}

function updateBatteryDisplay(percentage, voltage) {
    const fillEl = document.getElementById('batteryBarFill');
    const textEl = document.getElementById('batteryPercentage');
    if (!fillEl || !textEl) return;

    const pct = Math.max(0, Math.min(100, Math.round(Number(percentage) || 0)));
    fillEl.style.width = `${pct}%`;

    if (pct <= 20) fillEl.style.background = '#ef4444';
    else if (pct <= 45) fillEl.style.background = '#f59e0b';
    else fillEl.style.background = '#22c55e';

    // Kompakte, gleichbleibende Breite (kein Springen in der Statusleiste auf dem Handy)
    textEl.textContent = `${pct}%`;
    if (typeof voltage === 'number' && voltage > 0) {
        textEl.title = `Akkuspannung: ${voltage.toFixed(2)} V`;
    }
}

function rgbToHex(r, g, b) {
    const toHex = (n) => Math.max(0, Math.min(255, n | 0)).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

function anyMotorActive() {
    return Object.values(joints).some(j => j.activeDir !== 0 && j.activePwm > 0);
}

function getJointCurrent(joint) {
    if (joint.sensorMode === '0') return boardCurrents[0] || 0.0;
    if (joint.sensorMode === '1') return boardCurrents[1] || 0.0;
    return 0.0;
}

function getJointIdleBaseline(joint) {
    if (joint.sensorMode === '0') return idleCurrents[0] || 0.0;
    if (joint.sensorMode === '1') return idleCurrents[1] || 0.0;
    return 0.0;
}

// Prüft, ob ein echter Blockierstrom (Maximalstrom im Stall) vorliegt:
// 1. Gelenk besitzt einen der 2 Stromsensoren ('0' oder '1')
// 2. Strom erreicht den eingestellten Stall-Maximalstrom (curr >= stallLimit)
// 3. Strom ist gegenüber dem Ruhestrom-Offset tatsächlich um mindestens 0.30 A angestiegen
//    (verhindert Fehlauslösungen, wenn gar keine Motoren angeschlossen sind!)
function isJointStalledByCurrent(joint) {
    if (joint.sensorMode !== '0' && joint.sensorMode !== '1') return false;
    const curr = getJointCurrent(joint);
    const base = getJointIdleBaseline(joint);
    const delta = curr - base;
    return (curr >= joint.stallLimit) && (delta >= 0.30);
}

function formatAmp(val) {
    const clamped = Math.max(0, Math.min(9.99, Number(val) || 0));
    return clamped.toFixed(2);
}

function updateCurrentUI() {
    const i0 = Math.max(0, boardCurrents[0] || 0);
    const i1 = Math.max(0, boardCurrents[1] || 0);
    const readout = document.getElementById('currentReadout');
    const dot = document.getElementById('currDot');
    if (readout) {
        readout.textContent = `S1:${formatAmp(i0)}A S2:${formatAmp(i1)}A`;
    }
    const maxDelta = Math.max(0, i0 - idleCurrents[0], i1 - idleCurrents[1]);
    if (dot) {
        if (maxDelta >= 0.65) dot.style.background = '#ef4444';
        else if (maxDelta >= 0.25) dot.style.background = '#f59e0b';
        else dot.style.background = '#22c55e';
    }

    ['arm1', 'arm2', 'arm3'].forEach(id => {
        const j = joints[id];
        j.lastCurrent = getJointCurrent(j);
        const badge = document.getElementById('curr' + id.charAt(0).toUpperCase() + id.slice(1));
        if (!badge) return;
        const sensorPrefix = (j.sensorMode === '0') ? 'S1: ' : (j.sensorMode === '1' ? 'S2: ' : '');

        if (j.stalledDir !== 0) {
            badge.textContent = `Stall ${formatAmp(j.lastCurrent)}A`;
            badge.className = 'joint-curr-tag tag-stall';
        } else if (j.sensorMode === 'none') {
            badge.textContent = 'Zeit-Modus';
            badge.className = 'joint-curr-tag tag-nofault';
        } else {
            badge.textContent = `${sensorPrefix}${formatAmp(j.lastCurrent)} A`;
            const delta = j.lastCurrent - getJointIdleBaseline(j);
            badge.className = (j.lastCurrent >= j.stallLimit * 0.85 && delta >= 0.20)
                ? 'joint-curr-tag tag-warn'
                : 'joint-curr-tag';
        }
    });
}

function connectWebSocket() {
    const wsUrl = `ws://${window.location.hostname}/ws`;
    socket = new WebSocket(wsUrl);

    socket.onopen = function() {
        reconnectAttempts = 0;
        reconnectInterval = 1000;
        updateSignalDisplay(null, true);
    };

    socket.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            if (data.batteryPercentage !== undefined) {
                updateBatteryDisplay(data.batteryPercentage, data.batteryVoltage);
            }
            if (data.wifiRssi !== undefined) {
                updateSignalDisplay(data.wifiRssi, true);
            }
            if (Array.isArray(data.motorCurrents) && data.motorCurrents.length >= 2) {
                boardCurrents[0] = Number(data.motorCurrents[0]) || 0;
                boardCurrents[1] = Number(data.motorCurrents[1]) || 0;

                // Wenn kein Motor läuft, Ruhestrom-Baseline nachführen
                if (!anyMotorActive()) {
                    if (!idleSampleInitialized) {
                        idleCurrents[0] = boardCurrents[0];
                        idleCurrents[1] = boardCurrents[1];
                        idleSampleInitialized = true;
                    } else {
                        idleCurrents[0] = idleCurrents[0] * 0.85 + boardCurrents[0] * 0.15;
                        idleCurrents[1] = idleCurrents[1] * 0.85 + boardCurrents[1] * 0.15;
                    }
                }
                updateCurrentUI();
            }
            if (Array.isArray(data.motorPWMs) && data.motorPWMs.length >= 4) {
                boardPWMs = data.motorPWMs.map(v => Number(v) || 0);
            }
            if (Array.isArray(data.servos) && data.servos.length > gripperState.servoIndex && gripperState.movingDir === 0) {
                const srvAng = Number(data.servos[gripperState.servoIndex]);
                if (!Number.isNaN(srvAng)) {
                    gripperState.angle = srvAng;
                    syncGripperUI(false);
                }
            }
            if (data.firstLED) {
                const picker = document.getElementById('armLedPicker');
                if (picker && (Date.now() - lastLedSendMs > 500)) {
                    picker.value = rgbToHex(data.firstLED.r, data.firstLED.g, data.firstLED.b);
                }
            }
        } catch (err) {
            console.error('WS Fehler:', err);
        }
    };

    socket.onclose = function() {
        updateSignalDisplay(0, false);
        if (reconnectAttempts * reconnectInterval < maxReconnectInterval) {
            setTimeout(() => {
                reconnectAttempts++;
                connectWebSocket();
                reconnectInterval = Math.min(reconnectInterval * 2, maxReconnectInterval);
            }, reconnectInterval);
        }
    };

    socket.onerror = function() {
        try { socket.close(); } catch (_) {}
    };
}

// --- Motor- & Servo-Ansteuerung ---

function motorKeyForIndex(idx) {
    const letters = ['A', 'B', 'C', 'D'];
    return 'motor' + (letters[idx] || 'A');
}

function sendJointMotorCommand(joint, dir, pwmMagnitude) {
    const mag = Math.max(0, Math.min(255, Math.round(pwmMagnitude)));
    if (dir === 0 || mag === 0) {
        joint.activeDir = 0;
        joint.activePwm = 0;
        sendWS({ [motorKeyForIndex(joint.motorIndex)]: 'stop' });
        return;
    }

    // Nur blockieren, wenn ein ECHTER Stromsensor-Anschlag (Stall) in genau dieser Richtung vorliegt!
    // Das geschätzte Winkel-Limit (-90°/+90°) stoppt den Motor NICHT, da die Anzeige nur geschätzt ist
    // und der echte Arm weiter Strom bekommen muss, bis er wirklich am mechanischen Anschlag ankommt.
    if (joint.hasLimits && !homing.active) {
        if (dir < 0 && joint.stalledDir < 0) {
            showStatusOverlay(`${joint.label}: Strom-Anschlag (${joint.minDeg}°) erreicht`, 1400);
            joint.activeDir = 0;
            joint.activePwm = 0;
            sendWS({ [motorKeyForIndex(joint.motorIndex)]: 'stop' });
            return;
        }
        if (dir > 0 && joint.stalledDir > 0) {
            const sign = joint.maxDeg > 0 ? '+' : '';
            showStatusOverlay(`${joint.label}: Strom-Anschlag (${sign}${joint.maxDeg}°) erreicht`, 1400);
            joint.activeDir = 0;
            joint.activePwm = 0;
            sendWS({ [motorKeyForIndex(joint.motorIndex)]: 'stop' });
            return;
        }
    }

    // Bei Bewegung in Gegenrichtung Stall-Flag löschen
    if (joint.stalledDir !== 0 && joint.stalledDir !== dir) {
        joint.stalledDir = 0;
    }

    if (joint.activeDir !== dir) {
        joint.moveStartMs = performance.now();
    }
    joint.activeDir = dir;
    joint.activePwm = mag;

    const hwDir = joint.invert ? -dir : dir;
    const signedPwm = hwDir * mag;
    sendWS({ [motorKeyForIndex(joint.motorIndex)]: String(signedPwm) });
}

function stopAllMotors(cancelHoming = true) {
    if (cancelHoming && homing.active) {
        homing.active = false;
        homing.queue = [];
        homing.currentJointId = null;
        homing.phase = 'idle';
        showStatusOverlay('Homing abgebrochen - Alle Motoren gestoppt', 2000);
    }
    Object.values(joints).forEach(j => {
        j.activeDir = 0;
        j.activePwm = 0;
    });
    gripperState.movingDir = 0;
    sendWS({ motorA: 'stop', motorB: 'stop', motorC: 'stop', motorD: 'stop' });
    document.querySelectorAll('.arm-hold-btn.active').forEach(b => b.classList.remove('active'));
}

function setGripperServo(angleDeg) {
    const clamped = Math.max(0, Math.min(180, Math.round(angleDeg)));
    gripperState.angle = clamped;
    syncGripperUI(true);
}

function syncGripperUI(sendToEsp) {
    const slider = document.getElementById('gripperServoSlider');
    const lbl = document.getElementById('gripperAngleLabel');
    if (slider && document.activeElement !== slider) {
        slider.value = String(gripperState.angle);
    }
    if (lbl) {
        lbl.textContent = `${Math.round(gripperState.angle)}°`;
    }
    if (sendToEsp) {
        sendWS({ ['servo' + gripperState.servoIndex]: Math.round(gripperState.angle) });
    }
}

// --- Endanschlag-Homing & Strombegrenzungs-Logik ---

let overlayHideTimer = null;
function showStatusOverlay(text, autoHideMs = 0) {
    const box = document.getElementById('armStatusOverlay');
    const span = document.getElementById('armStatusText');
    if (!box || !span) return;
    span.textContent = text;
    box.style.display = 'flex';
    if (overlayHideTimer) {
        clearTimeout(overlayHideTimer);
        overlayHideTimer = null;
    }
    if (autoHideMs > 0 && !homing.active) {
        overlayHideTimer = setTimeout(() => {
            if (!homing.active) box.style.display = 'none';
        }, autoHideMs);
    }
}

function startHomingSequence(jointIds) {
    stopAllMotors(false);
    homing.active = true;
    homing.queue = [...jointIds];
    homing.currentJointId = null;
    homing.phase = 'idle';
    advanceHomingQueue(performance.now());
}

function advanceHomingQueue(nowMs) {
    if (!homing.active) return;
    if (homing.queue.length === 0) {
        homing.active = false;
        homing.currentJointId = null;
        homing.phase = 'idle';
        stopAllMotors(false);
        showStatusOverlay('✓ Homing fertig - Alle Armteile im Home-Endanschlag', 2800);
        return;
    }

    const nextId = homing.queue.shift();
    const j = joints[nextId];
    if (!j || !j.hasLimits) {
        advanceHomingQueue(nowMs);
        return;
    }

    homing.currentJointId = nextId;
    homing.phase = 'seek_endstop';
    homing.phaseStartMs = nowMs;
    homing.targetDir = getHomeDir(j);
    homing.targetAngle = getHomeAngle(j);
    j.stalledDir = 0;

    // Da wir kein Positions-Feedback haben, wissen wir vor dem Homen nicht sicher,
    // wo der echte Arm steht. Falls die Anzeige bereits am Home-Anschlag steht,
    // setzen wir die geschätzte Position für die Fahrt auf die Bereichsmitte zurück
    // (gleitet sanft hin, ohne harten Sprung im Bild) oder fahren den verbleibenden Weg.
    const homePwm = 220; // ~86% PWM für kräftige Fahrt bis in den Endanschlag
    const speedFactor = homePwm / 255.0;
    const remainingDeg = Math.abs(homing.targetAngle - j.angle);

    // Berechne Laufzeit bis zum Home-Endanschlag (mindestens 0.9 s Nachdrückzeit für den echten Motor)
    const estDeg = Math.max(25.0, remainingDeg);
    homing.targetDurationMs = ((estDeg / 90.0) * (j.time90 / speedFactor)) * 1000;

    const endName = (j.homeEnd === 'min') ? `Min (${j.minDeg}°)` : `Max (+${j.maxDeg}°)`;
    const modeInfo = (j.sensorMode === 'none') ? 'Zeit-Anschlag' : 'bis Stall-Strom / Anschlag';
    showStatusOverlay(`Homing ${j.label} -> ${endName} (${modeInfo})...`);
    sendJointMotorCommand(j, homing.targetDir, homePwm);
}

function updateHomingAndPhysics(dtSec, nowMs) {
    // 1. Greifer-Tasten-Fahrt (falls gehalten)
    if (gripperState.movingDir !== 0) {
        const step = gripperState.movingDir * 95 * dtSec; // 95°/s
        const minA = Math.min(gripperState.openAngle, gripperState.closeAngle);
        const maxA = Math.max(gripperState.openAngle, gripperState.closeAngle);
        const dirSign = (gripperState.closeAngle >= gripperState.openAngle) ? 1 : -1;
        let nextA = gripperState.angle + step * dirSign;
        nextA = Math.max(minA, Math.min(maxA, nextA));
        if (Math.round(nextA) !== Math.round(gripperState.angle)) {
            gripperState.angle = nextA;
            syncGripperUI(true);
        }
    }

    // 2. Laufendes Endanschlag-Homing verarbeiten
    if (homing.active && homing.currentJointId) {
        const j = joints[homing.currentJointId];
        const elapsedMs = nowMs - homing.phaseStartMs;

        if (homing.phase === 'seek_endstop') {
            // Nach 280 ms Anlaufstrom-Blanking auf echten Stall-Maximalstrom prüfen
            const currentTriggered = (elapsedMs > 280) && isJointStalledByCurrent(j);
            const reachedVisualEnd = (homing.targetDir > 0)
                ? (j.angle >= j.maxDeg - 0.1)
                : (j.angle <= j.minDeg + 0.1);
            const timeTriggered = (elapsedMs >= homing.targetDurationMs) && reachedVisualEnd;

            if (currentTriggered || timeTriggered) {
                sendJointMotorCommand(j, 0, 0);
                j.angle = homing.targetAngle;
                j.stalledDir = 0;
                j.isHomed = true;
                homing.phase = 'pause';
                homing.phaseStartMs = nowMs;
                const curr = getJointCurrent(j);
                const reason = currentTriggered
                    ? `Stall erkannt (${curr.toFixed(2)}A)`
                    : 'Endanschlag erreicht';
                const sign = homing.targetAngle > 0 ? '+' : '';
                showStatusOverlay(`✓ ${j.label}: ${reason} -> Home (${sign}${homing.targetAngle}°)`);
                updateCurrentUI();
                return;
            }
        } else if (homing.phase === 'pause') {
            if (elapsedMs >= 260) {
                advanceHomingQueue(nowMs);
            }
            return;
        }
    }

    // 3. Winkel-Integration & Live-Stromabschaltung für alle Gelenke
    Object.values(joints).forEach(j => {
        if (j.activeDir !== 0 && j.activePwm > 0) {
            const elapsedMoveMs = nowMs - j.moveStartMs;

            // Live-Überstromschutz im manuellen Betrieb (nur für die 2 Gelenke MIT Stromsensor!)
            // NUR dieser echte Hardware-Stall schaltet den Motor automatisch ab!
            if (!homing.active && j.hasLimits && elapsedMoveMs > 280 && isJointStalledByCurrent(j)) {
                const blockedDir = j.activeDir;
                const curr = getJointCurrent(j);
                sendJointMotorCommand(j, 0, 0);
                j.stalledDir = blockedDir;
                // Winkel auf den erreichten mechanischen Endanschlag kalibrieren (visualAngle gleitet sanft nach)
                j.angle = (blockedDir < 0) ? j.minDeg : j.maxDeg;
                j.isHomed = true;
                showStatusOverlay(`⚡ Stall-Strom ${j.label} (${curr.toFixed(2)}A) - Anschlag erreicht!`, 2600);
                updateCurrentUI();
                return;
            }

            // Winkeländerung aus Laufzeit und PWM berechnen ("Raten" der Position über die Zeit)
            const degPerSecAtFull = 90.0 / Math.max(0.5, j.time90);
            const speedRatio = j.activePwm / 255.0;
            const deltaDeg = j.activeDir * degPerSecAtFull * speedRatio * dtSec;

            if (!j.hasLimits) {
                // Rotierende Basis: 360° endlos ohne Begrenzung
                j.angle = (j.angle + deltaDeg) % 360;
                if (j.angle < 0) j.angle += 360;
                j.visualAngle = j.angle;
            } else {
                // Wichtig: Am geschätzten Limit (-90° / +90°) begrenzen wir NUR die Anzeige,
                // senden aber WEITER Strom an den Motor, solange die Taste gedrückt wird
                // (bis der echte Stromsensor-Anschlag auslöst oder der Nutzer loslässt)!
                j.angle = Math.max(j.minDeg, Math.min(j.maxDeg, j.angle + deltaDeg));
            }
        }

        // Sanftes Nachführen von visualAngle (verhindert jegliches Springen in der Animation!)
        if (j.hasLimits) {
            const diff = j.angle - j.visualAngle;
            if (Math.abs(diff) < 0.15) {
                j.visualAngle = j.angle;
            } else {
                const maxCatchupStep = 240.0 * dtSec; // Max 240°/s sanftes Angleichen
                j.visualAngle += Math.sign(diff) * Math.min(Math.abs(diff), maxCatchupStep);
            }
        }
    });

    updateJointCardsUI();
}

function updateJointCardsUI() {
    // Basis (360° endlos)
    const baseLbl = document.getElementById('angleBase');
    if (baseLbl) baseLbl.textContent = `${Math.round(joints.base.visualAngle)}°`;

    // Arm 1, 2, 3 (jeweils -90° bis +90° = 180° Bereich)
    ['arm1', 'arm2', 'arm3'].forEach(id => {
        const j = joints[id];
        const suffix = id.charAt(0).toUpperCase() + id.slice(1);
        const angleEl = document.getElementById('angle' + suffix);
        const barEl = document.getElementById('bar' + suffix);
        const minLbl = document.getElementById('minLbl' + suffix);
        const maxLbl = document.getElementById('maxLbl' + suffix);

        const dispAngle = Math.round(j.visualAngle);
        if (angleEl) {
            const sign = dispAngle > 0 ? '+' : '';
            angleEl.textContent = `${sign}${dispAngle}°`;
        }
        if (barEl) {
            const pct = ((j.visualAngle - j.minDeg) / (j.maxDeg - j.minDeg)) * 100;
            barEl.style.width = `${Math.max(0, Math.min(100, pct)).toFixed(1)}%`;
        }
        if (minLbl) {
            minLbl.textContent = `${j.minDeg}°${j.homeEnd === 'min' ? ' (Home)' : ''}`;
        }
        if (maxLbl) {
            const sign = j.maxDeg > 0 ? '+' : '';
            maxLbl.textContent = `${sign}${j.maxDeg}°${j.homeEnd === 'max' ? ' (Home)' : ''}`;
        }
    });
}

// --- CAD-getreue Canvas-Animation (Seitenansicht + 360° Drehkranz-Radar) ---

function drawRoundedRect(ctx, x, y, w, h, r) {
    const rad = Math.min(r, Math.abs(w) / 2, Math.abs(h) / 2);
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, rad);
    ctx.fill();
    ctx.stroke();
}

function drawScrewHead(ctx, x, y, r, angleRad, highlight = false) {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(angleRad);
    ctx.fillStyle = highlight ? '#f59e0b' : '#575b62';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    ctx.arc(0, 0, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
    // Schraubenschlitz
    ctx.strokeStyle = '#1f242b';
    ctx.lineWidth = 2.0;
    ctx.beginPath();
    ctx.moveTo(-r * 0.72, -r * 0.25);
    ctx.lineTo(r * 0.72, r * 0.25);
    ctx.stroke();
    ctx.restore();
}

function drawWormGear(ctx, x, y, w, h, phaseRad, active = false) {
    ctx.save();
    ctx.translate(x, y);
    ctx.fillStyle = active ? '#fbbf24' : '#94a3b8';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.3;
    drawRoundedRect(ctx, -w / 2, -h / 2, w, h, 3);

    // Schnecken-Gewindegänge (wandern bei Bewegung)
    ctx.strokeStyle = '#334155';
    ctx.lineWidth = 1.4;
    const pitch = 4.5;
    const shift = ((phaseRad * 3) % pitch + pitch) % pitch;
    for (let ly = -h / 2 + shift; ly < h / 2 - 2; ly += pitch) {
        ctx.beginPath();
        ctx.moveTo(-w / 2 - 1, ly - 1.5);
        ctx.lineTo(w / 2 + 1, ly + 1.5);
        ctx.stroke();
    }
    ctx.restore();
}

function renderArmCanvas() {
    const canvas = document.getElementById('armCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const W = canvas.width;
    const H = canvas.height;

    ctx.clearRect(0, 0, W, H);

    // 1. CAD-Hintergrund mit feinem Konstruktionsgitter (wie in Fusion 360)
    ctx.fillStyle = '#1b222c';
    ctx.fillRect(0, 0, W, H);

    ctx.strokeStyle = 'rgba(148, 163, 184, 0.10)';
    ctx.lineWidth = 1;
    const gridStep = 20;
    for (let gx = 0; gx < W; gx += gridStep) {
        ctx.beginPath();
        ctx.moveTo(gx, 0);
        ctx.lineTo(gx, H);
        ctx.stroke();
    }
    for (let gy = 0; gy < H; gy += gridStep) {
        ctx.beginPath();
        ctx.moveTo(0, gy);
        ctx.lineTo(W, gy);
        ctx.stroke();
    }

    // Bodenlinie (grün wie die Fusion X/Y-Achse)
    const baseBottomY = H - 18;
    const baseCenterX = W * 0.54;
    ctx.strokeStyle = 'rgba(34, 197, 94, 0.45)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(12, baseBottomY);
    ctx.lineTo(W - 12, baseBottomY);
    ctx.stroke();

    // Maßstabsfaktor
    const S = 0.62;

    ctx.save();
    ctx.translate(baseCenterX, baseBottomY);

    // --- A) ROTIERENDE BASIS (Basis:1 + Rotorteller:1 + Deckel:1 + Armstutzen:1, unendlich 360°) ---
    const baseW = 150 * S;
    const baseH1 = 10 * S;
    const rotorH = 26 * S;

    // Unterer Sockelring (ocker/braun wie in Fusion)
    ctx.fillStyle = '#b47128';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.5;
    drawRoundedRect(ctx, -baseW / 2, -baseH1, baseW, baseH1, 3);

    // Rotorteller (grau-metallisch mit wandernden Dreh-Markierungen)
    const rotorGrad = ctx.createLinearGradient(-baseW / 2, 0, baseW / 2, 0);
    rotorGrad.addColorStop(0, '#6b7280');
    rotorGrad.addColorStop(0.5, '#adb5bd');
    rotorGrad.addColorStop(1, '#6b7280');
    ctx.fillStyle = rotorGrad;
    drawRoundedRect(ctx, -baseW / 2, -(baseH1 + rotorH), baseW, rotorH, 4);

    // Rotations-Indikatorstreifen auf dem Drehkranz
    ctx.save();
    ctx.beginPath();
    ctx.rect(-baseW / 2 + 2, -(baseH1 + rotorH) + 2, baseW - 4, rotorH - 4);
    ctx.clip();
    const stripeSpacing = 18;
    const stripeOffset = ((joints.base.visualAngle * 1.2) % stripeSpacing + stripeSpacing) % stripeSpacing;
    ctx.strokeStyle = (joints.base.activeDir !== 0) ? 'rgba(56, 189, 248, 0.55)' : 'rgba(30, 41, 59, 0.35)';
    ctx.lineWidth = 2;
    for (let sx = -baseW / 2 + stripeOffset - stripeSpacing; sx < baseW / 2 + stripeSpacing; sx += stripeSpacing) {
        ctx.beginPath();
        ctx.moveTo(sx, -(baseH1 + rotorH));
        ctx.lineTo(sx, -baseH1);
        ctx.stroke();
    }
    ctx.restore();

    // Deckelring oben auf dem Rotorteller
    ctx.fillStyle = '#c57d30';
    drawRoundedRect(ctx, -baseW / 2 + 2, -(baseH1 + rotorH + 3), baseW - 4, 3.5, 2);

    // Gelenk 1 Ursprung (Umdrehung 18 bei Z = 65.2 mm)
    const j1Y = -65.2 * S;

    // --- B) ARMTEIL 1 (Unterarm: -90° links waagerecht über 0° senkrecht bis +90° rechts waagerecht = 180°) ---
    ctx.save();
    ctx.translate(0, j1Y);
    const rad1 = (joints.arm1.visualAngle * Math.PI) / 180.0;
    ctx.rotate(rad1);

    // L-förmiger brauner Arm 1 (Länge 125 mm nach oben, +30 mm Knick nach rechts zum Gelenk 2)
    const a1Len = 125 * S;
    const a1OffX = 30 * S;
    const a1W = 28 * S;

    ctx.fillStyle = (joints.arm1.stalledDir !== 0) ? '#dc2626' : '#9c6b30';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    ctx.roundRect(-a1W / 2, -a1Len - a1W / 2, a1W, a1Len + a1W, a1W / 2);
    ctx.roundRect(-a1W / 2, -a1Len - a1W / 2, a1W + a1OffX, a1W, a1W / 2);
    ctx.fill();
    ctx.stroke();

    // Dunkelgraue Schneckenfixierung-Klammer an Arm 1 (wie im Fusion-Modell)
    ctx.fillStyle = '#4b5057';
    drawRoundedRect(ctx, -a1W / 2 - 2, -a1Len * 0.80, a1W + a1OffX + 4, 24 * S, 2);

    // Schraube Gelenk 1 (unten im Armstutzen) - dreht weiter solange Motor bestromt wird
    const liveSpin1 = (joints.arm1.activeDir !== 0) ? (performance.now() * 0.008 * joints.arm1.activeDir) : rad1;
    drawScrewHead(ctx, 0, 0, 6.5 * S, liveSpin1, joints.arm1.activeDir !== 0);

    // --- C) ARMTEIL 2 (Mittelarm: -90° über 0° Startpose bis +90° = 180° Bereich) ---
    ctx.save();
    ctx.translate(a1OffX, -a1Len);
    const rad2 = (joints.arm2.visualAngle * Math.PI) / 180.0;
    ctx.rotate(rad2);

    const a2Up = 90 * S;    // 90 mm nach oben
    const a2Left = -95 * S; // 95 mm nach links zum Gelenk 3
    const a2W = 28 * S;

    // Schnecke 3 oben auf dem horizontalen Balken von Arm 2 (dreht weiter solange Motor bestromt wird)
    const wormPhase3 = (joints.arm3.activeDir !== 0)
        ? (performance.now() * 0.012 * joints.arm3.activeDir)
        : (joints.arm3.visualAngle * Math.PI) / 45.0;
    drawWormGear(
        ctx,
        a2Left + 27 * S,
        -a2Up - 10 * S,
        18 * S,
        24 * S,
        wormPhase3,
        joints.arm3.activeDir !== 0
    );

    // Grauer L-förmiger Mittelarm (arm2:1)
    ctx.fillStyle = (joints.arm2.stalledDir !== 0) ? '#dc2626' : '#858a92';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    // Vertikaler Schenkel
    ctx.roundRect(-a2W / 2, -a2Up + a2W * 0.15, a2W, a2Up + a2W * 0.35, a2W * 0.42);
    // Horizontaler Schenkel nach links + Rundung nach oben für Gelenk 3
    ctx.roundRect(a2Left - a2W / 2, -a2Up - a2W * 0.15, Math.abs(a2Left) + a2W, a2W, 4);
    ctx.roundRect(a2Left - a2W / 2, -a2Up - a2W * 0.55, a2W, a2W * 1.4, a2W * 0.45);
    ctx.fill();
    ctx.stroke();

    // Schraube Gelenk 2 (Ellbogen) - dreht weiter solange Motor bestromt wird
    const liveSpin2 = (joints.arm2.activeDir !== 0) ? (performance.now() * 0.008 * joints.arm2.activeDir) : rad2;
    drawScrewHead(ctx, 0, 0, 6.5 * S, liveSpin2, joints.arm2.activeDir !== 0);

    // --- D) ARMTEIL 3 (Handgelenk: +90° = Maximum gerade nach oben, schwenkt 180° über links bis -90° gerade nach unten) ---
    ctx.save();
    ctx.translate(a2Left, -a2Up);
    const delta3Rad = ((joints.arm3.visualAngle - 90.0) * Math.PI) / 180.0;
    ctx.rotate(delta3Rad);

    const a3Len = 44 * S;
    const a3W = 18 * S;

    // Brauner Handgelenk-Arm (Arm3:1)
    ctx.fillStyle = (joints.arm3.stalledDir !== 0) ? '#dc2626' : '#9c6b30';
    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1.5;
    drawRoundedRect(ctx, -a3W / 2, -a3Len, a3W, a3Len, 4);

    // Kleines Servo-Gehäuse im Handgelenk
    ctx.fillStyle = '#475569';
    drawRoundedRect(ctx, -a3W * 0.32, -a3Len + 4, a3W * 0.64, a3Len * 0.62, 2);

    // Schraube Gelenk 3 (Handgelenk) - dreht weiter solange Motor bestromt wird
    const liveSpin3 = (joints.arm3.activeDir !== 0) ? (performance.now() * 0.008 * joints.arm3.activeDir) : delta3Rad;
    drawScrewHead(ctx, 0, 0, 6.0 * S, liveSpin3, joints.arm3.activeDir !== 0);

    // --- E) GREIFER MIT SERVO (Greiferbacken:1 + Backenzahnrad:1) ---
    ctx.save();
    ctx.translate(0, -a3Len);

    // Öffnungsweite aus Servo-Winkel (0..180°) berechnen
    let openRatio = (gripperState.angle - gripperState.closeAngle) /
                    ((gripperState.openAngle - gripperState.closeAngle) || 1);
    openRatio = Math.max(0, Math.min(1, openRatio));
    const jawSpread = (3 + openRatio * 14) * S; // Abstand der Backen

    // Greifer-Traverse (ocker/braun)
    ctx.fillStyle = '#b47128';
    drawRoundedRect(ctx, -20 * S, -7 * S, 40 * S, 7 * S, 2);

    // Backenzahnrad (kleines violettes Zahnrad in der Mitte)
    ctx.fillStyle = '#a78bfa';
    ctx.beginPath();
    ctx.arc(0, -3.5 * S, 4.5 * S, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // Linke und rechte Greiferbacke (silber/anthrazit wie in Fusion)
    const jawW = 7 * S;
    const jawH = 22 * S;
    ctx.fillStyle = '#cbd5e1';
    ctx.strokeStyle = '#1e293b';
    drawRoundedRect(ctx, -jawSpread - jawW, -7 * S - jawH, jawW, jawH, 2.5);
    drawRoundedRect(ctx, jawSpread, -7 * S - jawH, jawW, jawH, 2.5);

    ctx.restore(); // Ende Greifer
    ctx.restore(); // Ende Arm 3
    ctx.restore(); // Ende Arm 2
    ctx.restore(); // Ende Arm 1

    // Vorderer transparenter Armstutzen-Rahmen (wie im Fusion-Screenshot im Vordergrund)
    ctx.strokeStyle = 'rgba(226, 232, 240, 0.75)';
    ctx.lineWidth = 2.0;
    ctx.strokeRect(-22 * S, -(50 + 34) * S, 44 * S, 38 * S);

    ctx.restore(); // Ende Haupt-Koordinatensystem

    // --- F) 360° DREHKRANZ TOP-DOWN RADAR (oben links im Canvas) ---
    const radarX = 54;
    const radarY = 54;
    const radarR = 36;

    ctx.save();
    ctx.translate(radarX, radarY);

    ctx.fillStyle = 'rgba(15, 23, 42, 0.82)';
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.4)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(0, 0, radarR, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // Fadenkreuz
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.2)';
    ctx.beginPath();
    ctx.moveTo(-radarR, 0); ctx.lineTo(radarR, 0);
    ctx.moveTo(0, -radarR); ctx.lineTo(0, radarR);
    ctx.stroke();

    // Rotierender Arm-Richtungszeiger (Draufsicht)
    const baseRad = ((joints.base.visualAngle - 90) * Math.PI) / 180.0;
    ctx.rotate(baseRad);
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(radarR - 6, 0);
    ctx.stroke();

    ctx.fillStyle = '#f59e0b';
    ctx.beginPath();
    ctx.arc(radarR - 6, 0, 4, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();

    // Radar-Beschriftung
    ctx.fillStyle = '#cbd5e1';
    ctx.font = 'bold 11px Segoe UI, sans-serif';
    ctx.fillText(`Basis: ${Math.round(joints.base.visualAngle)}°`, 18, radarY + radarR + 14);

    // Live-Winkellegende oben rechts im Canvas
    ctx.fillStyle = 'rgba(15, 23, 42, 0.82)';
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.3)';
    ctx.lineWidth = 1;
    drawRoundedRect(ctx, W - 134, 8, 126, 90, 6);

    const allHomed = joints.arm1.isHomed && joints.arm2.isHomed && joints.arm3.isHomed;
    ctx.font = 'bold 10px Segoe UI, sans-serif';
    ctx.fillStyle = allHomed ? '#22c55e' : '#f59e0b';
    ctx.fillText(allHomed ? '● Kalibriert (Home)' : '○ Geschätzt (Homen!)', W - 126, 22);

    ctx.font = '11px Segoe UI, sans-serif';
    ctx.fillStyle = '#fbbf24';
    ctx.fillText(`A1 (S2):  ${Math.round(joints.arm1.visualAngle)}°`, W - 126, 39);
    ctx.fillStyle = '#cbd5e1';
    ctx.fillText(`A2 (S1):  ${Math.round(joints.arm2.visualAngle)}°`, W - 126, 55);
    ctx.fillStyle = '#fb923c';
    ctx.fillText(`A3 (Zeit): ${Math.round(joints.arm3.visualAngle)}°`, W - 126, 71);
    ctx.fillStyle = '#a78bfa';
    ctx.fillText(`Greifer:  ${Math.round(gripperState.angle)}°`, W - 126, 87);
}

// --- Touch-Tasten, Dual-Joysticks & Einstellungen ---

function bindHoldButtons() {
    // Gelenk-Tasten (Basis, Arm 1, Arm 2, Arm 3)
    document.querySelectorAll('.arm-hold-btn[data-joint]').forEach(btn => {
        const jointId = btn.getAttribute('data-joint');
        const dir = parseInt(btn.getAttribute('data-dir'), 10);
        const j = joints[jointId];
        if (!j) return;

        const startMove = (e) => {
            e.preventDefault();
            if (homing.active) return;
            btn.classList.add('active');
            const pwm = Math.round((globalSpeedPct / 100) * 255);
            sendJointMotorCommand(j, dir, pwm);
        };

        const stopMove = (e) => {
            if (e) e.preventDefault();
            btn.classList.remove('active');
            if (j.activeDir === dir) {
                sendJointMotorCommand(j, 0, 0);
            }
        };

        btn.addEventListener('pointerdown', startMove);
        btn.addEventListener('pointerup', stopMove);
        btn.addEventListener('pointercancel', stopMove);
        btn.addEventListener('pointerleave', (e) => {
            if (btn.classList.contains('active')) stopMove(e);
        });
        btn.addEventListener('contextmenu', e => e.preventDefault());
    });

    // Greifer Auf/Zu Hold-Buttons (in beiden Modi)
    const bindGripBtn = (btnId, dir) => {
        const btn = document.getElementById(btnId);
        if (!btn) return;
        const start = (e) => {
            e.preventDefault();
            btn.classList.add('active');
            gripperState.movingDir = dir;
        };
        const stop = (e) => {
            if (e) e.preventDefault();
            btn.classList.remove('active');
            gripperState.movingDir = 0;
        };
        btn.addEventListener('pointerdown', start);
        btn.addEventListener('pointerup', stop);
        btn.addEventListener('pointercancel', stop);
        btn.addEventListener('pointerleave', (e) => {
            if (btn.classList.contains('active')) stop(e);
        });
        btn.addEventListener('contextmenu', e => e.preventDefault());
    };

    bindGripBtn('btnGripOpen', -1);
    bindGripBtn('btnGripClose', 1);
    bindGripBtn('btnJoyGripOpen', -1);
    bindGripBtn('btnJoyGripClose', 1);

    // Greifer-Slider
    const gripSlider = document.getElementById('gripperServoSlider');
    if (gripSlider) {
        gripSlider.addEventListener('input', () => {
            setGripperServo(parseInt(gripSlider.value, 10));
        });
    }

    // Einzel-Home-Buttons pro Armteil
    document.querySelectorAll('.arm-mini-home-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const jid = btn.getAttribute('data-joint');
            if (!jid) return;
            const j = joints[jid];
            // Falls das Gelenk schon am Home-Endanschlag angezeigt wird, der Nutzer aber
            // trotzdem auf ⌂ tippt (weil der echte Arm ohne Feedback woanders steht),
            // starten wir die Schätzung sanft aus der Mitte Richtung Endanschlag.
            if (j && Math.abs(j.angle - getHomeAngle(j)) < 3.0) {
                j.angle = (j.minDeg + j.maxDeg) * 0.5;
            }
            startHomingSequence([jid]);
        });
    });

    // Haupt-Aktionsbuttons unter der Animation
    const btnHomeAll = document.getElementById('btnHomeAll');
    if (btnHomeAll) {
        btnHomeAll.addEventListener('click', () => {
            if (homing.active) {
                stopAllMotors(true);
            } else {
                // Falls alle Gelenke in der Anzeige bereits am Home-Anschlag stehen,
                // der Nutzer aber erneut "Arm Homen" drückt, setzen wir die Schätzung
                // auf die Mittelpose, damit alle 3 Achsen sauber in den Endanschlag fahren.
                const allAtHome = ['arm1', 'arm2', 'arm3'].every(
                    id => Math.abs(joints[id].angle - getHomeAngle(joints[id])) < 3.0
                );
                if (allAtHome) {
                    ['arm1', 'arm2', 'arm3'].forEach(id => {
                        joints[id].angle = (joints[id].minDeg + joints[id].maxDeg) * 0.5;
                    });
                }
                // Sequenzielles Homing von oben nach unten: Arm 3 -> Arm 2 -> Arm 1
                startHomingSequence(['arm3', 'arm2', 'arm1']);
            }
        });
    }

    const btnSetZero = document.getElementById('btnSetZero');
    if (btnSetZero) {
        btnSetZero.addEventListener('click', () => {
            stopAllMotors(true);
            joints.base.angle = 0.0;
            joints.base.visualAngle = 0.0;
            ['arm1', 'arm2', 'arm3'].forEach(id => {
                const j = joints[id];
                j.angle = getHomeAngle(j);
                j.stalledDir = 0;
                j.isHomed = true;
            });
            updateJointCardsUI();
            updateCurrentUI();
            showStatusOverlay('✓ Aktuelle Position als Home-Endanschlag gesetzt', 2200);
        });
    }

    const btnStopAll = document.getElementById('btnStopAll');
    if (btnStopAll) {
        btnStopAll.addEventListener('click', () => stopAllMotors(true));
    }

    // Tempo-Slider
    const speedSlider = document.getElementById('armSpeedSlider');
    const speedVal = document.getElementById('armSpeedVal');
    if (speedSlider) {
        speedSlider.addEventListener('input', () => {
            globalSpeedPct = parseInt(speedSlider.value, 10) || 80;
            if (speedVal) speedVal.textContent = `${globalSpeedPct}%`;
        });
    }

    // LED-Farbwähler
    const ledPicker = document.getElementById('armLedPicker');
    if (ledPicker) {
        const sendColor = () => {
            const now = Date.now();
            if (now - lastLedSendMs < 60) return;
            lastLedSendMs = now;
            sendWS({ led_set: { start: 0, count: 1, color: ledPicker.value } });
        };
        ledPicker.addEventListener('input', sendColor);
        ledPicker.addEventListener('change', () => {
            sendWS({ led_set: { start: 0, count: 1, color: ledPicker.value } });
        });
    }
}

function setupModeTabs() {
    const tabs = [
        { btn: 'tabPadMode', panel: 'panelPadMode' },
        { btn: 'tabJoyMode', panel: 'panelJoyMode' },
        { btn: 'tabCalibMode', panel: 'panelCalibMode' }
    ];
    tabs.forEach(t => {
        const b = document.getElementById(t.btn);
        if (!b) return;
        b.addEventListener('click', () => {
            stopAllMotors(false);
            tabs.forEach(other => {
                const ob = document.getElementById(other.btn);
                const op = document.getElementById(other.panel);
                if (ob) ob.classList.toggle('active', other.btn === t.btn);
                if (op) op.classList.toggle('active', other.panel === t.panel);
            });
        });
    });
}

function setupDualJoysticks() {
    const createStick = (canvasId, jointXId, jointYId) => {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        const W = canvas.width;
        const H = canvas.height;
        const R = W * 0.36;
        let knobX = 0;
        let knobY = 0;
        let dragging = false;

        const draw = () => {
            ctx.clearRect(0, 0, W, H);
            ctx.save();
            ctx.translate(W / 2, H / 2);

            ctx.fillStyle = '#1e293b';
            ctx.strokeStyle = '#475569';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.arc(0, 0, R, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();

            ctx.strokeStyle = 'rgba(148, 163, 184, 0.25)';
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(-R, 0); ctx.lineTo(R, 0);
            ctx.moveTo(0, -R); ctx.lineTo(0, R);
            ctx.stroke();

            ctx.fillStyle = dragging ? '#38bdf8' : '#0284c7';
            ctx.beginPath();
            ctx.arc(knobX, knobY, 20, 0, Math.PI * 2);
            ctx.fill();
            ctx.restore();
        };

        const applyAxes = (nx, ny) => {
            if (homing.active) return;
            const jx = joints[jointXId];
            const jy = joints[jointYId];
            const maxPwm = Math.round((globalSpeedPct / 100) * 255);

            if (jx) {
                if (Math.abs(nx) < 0.22) sendJointMotorCommand(jx, 0, 0);
                else sendJointMotorCommand(jx, nx > 0 ? 1 : -1, Math.abs(nx) * maxPwm);
            }
            if (jy) {
                const valY = -ny;
                if (Math.abs(valY) < 0.22) sendJointMotorCommand(jy, 0, 0);
                else sendJointMotorCommand(jy, valY > 0 ? 1 : -1, Math.abs(valY) * maxPwm);
            }
        };

        const updateFromEvent = (e) => {
            const rect = canvas.getBoundingClientRect();
            let dx = (e.clientX - rect.left) * (W / rect.width) - W / 2;
            let dy = (e.clientY - rect.top) * (H / rect.height) - H / 2;
            const dist = Math.hypot(dx, dy);
            if (dist > R) {
                dx = (dx / dist) * R;
                dy = (dy / dist) * R;
            }
            knobX = dx;
            knobY = dy;
            draw();
            applyAxes(dx / R, dy / R);
        };

        const release = () => {
            if (!dragging) return;
            dragging = false;
            knobX = 0;
            knobY = 0;
            draw();
            applyAxes(0, 0);
        };

        canvas.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            dragging = true;
            canvas.setPointerCapture(e.pointerId);
            updateFromEvent(e);
        });
        canvas.addEventListener('pointermove', (e) => {
            if (!dragging) return;
            e.preventDefault();
            updateFromEvent(e);
        });
        canvas.addEventListener('pointerup', release);
        canvas.addEventListener('pointercancel', release);
        draw();
    };

    createStick('joyLeftCanvas', 'base', 'arm1');
    createStick('joyRightCanvas', 'arm3', 'arm2');
}

// --- Persistente Arm-Konfiguration (localStorage + /getConfig) ---

function loadArmConfig() {
    try {
        const raw = localStorage.getItem(STORAGE_KEY);
        if (raw) {
            const saved = JSON.parse(raw);
            ['base', 'arm1', 'arm2', 'arm3'].forEach(id => {
                if (saved[id]) {
                    const j = joints[id];
                    if (saved[id].motorIndex !== undefined) j.motorIndex = Number(saved[id].motorIndex);
                    if (saved[id].invert !== undefined) j.invert = Boolean(saved[id].invert);
                    if (saved[id].sensorMode !== undefined) {
                        // 'auto' aus älteren Konfigurationen nicht mehr zulassen (nur '0', '1', 'none')
                        const sm = String(saved[id].sensorMode);
                        j.sensorMode = (sm === '0' || sm === '1' || sm === 'none') ? sm : j.sensorMode;
                    }
                    if (saved[id].homeEnd === 'min' || saved[id].homeEnd === 'max') {
                        j.homeEnd = saved[id].homeEnd;
                    }
                    if (saved[id].stallLimit !== undefined) j.stallLimit = Number(saved[id].stallLimit);
                    if (saved[id].time90 !== undefined) j.time90 = Number(saved[id].time90);
                }
            });
            if (saved.gripper) {
                if (saved.gripper.servoIndex !== undefined) gripperState.servoIndex = Number(saved.gripper.servoIndex);
                if (saved.gripper.openAngle !== undefined) gripperState.openAngle = Number(saved.gripper.openAngle);
                if (saved.gripper.closeAngle !== undefined) gripperState.closeAngle = Number(saved.gripper.closeAngle);
            }
        }
    } catch (_) {}

    // Basis hat niemals einen Stromsensor
    joints.base.sensorMode = 'none';

    // Formularfelder synchronisieren
    ['base', 'arm1', 'arm2', 'arm3'].forEach(id => {
        const j = joints[id];
        const mEl = document.getElementById('cfgMotor_' + id);
        const iEl = document.getElementById('cfgInv_' + id);
        const sEl = document.getElementById('cfgSensor_' + id);
        const hEl = document.getElementById('cfgHomeEnd_' + id);
        const stEl = document.getElementById('cfgStall_' + id);
        const tEl = document.getElementById('cfgTime90_' + id);
        if (mEl) mEl.value = String(j.motorIndex);
        if (iEl) iEl.checked = Boolean(j.invert);
        if (sEl) sEl.value = String(j.sensorMode);
        if (hEl) hEl.value = String(j.homeEnd);
        if (stEl) stEl.value = String(j.stallLimit);
        if (tEl) tEl.value = String(j.time90);
    });

    const srvEl = document.getElementById('cfgServo_gripper');
    const goEl = document.getElementById('cfgGripOpenDeg');
    const gcEl = document.getElementById('cfgGripCloseDeg');
    if (srvEl) srvEl.value = String(gripperState.servoIndex);
    if (goEl) goEl.value = String(gripperState.openAngle);
    if (gcEl) gcEl.value = String(gripperState.closeAngle);
}

function saveArmConfigFromUI() {
    const toSave = {};
    ['base', 'arm1', 'arm2', 'arm3'].forEach(id => {
        const j = joints[id];
        const mEl = document.getElementById('cfgMotor_' + id);
        const iEl = document.getElementById('cfgInv_' + id);
        const sEl = document.getElementById('cfgSensor_' + id);
        const hEl = document.getElementById('cfgHomeEnd_' + id);
        const stEl = document.getElementById('cfgStall_' + id);
        const tEl = document.getElementById('cfgTime90_' + id);

        if (mEl) j.motorIndex = parseInt(mEl.value, 10) || 0;
        if (iEl) j.invert = Boolean(iEl.checked);
        if (sEl) j.sensorMode = sEl.value;
        if (hEl && (hEl.value === 'min' || hEl.value === 'max')) j.homeEnd = hEl.value;
        if (stEl) j.stallLimit = parseFloat(stEl.value) || 1.0;
        if (tEl) j.time90 = parseFloat(tEl.value) || 3.0;

        toSave[id] = {
            motorIndex: j.motorIndex,
            invert: j.invert,
            sensorMode: j.sensorMode,
            homeEnd: j.homeEnd,
            stallLimit: j.stallLimit,
            time90: j.time90
        };
    });

    joints.base.sensorMode = 'none';

    const srvEl = document.getElementById('cfgServo_gripper');
    const goEl = document.getElementById('cfgGripOpenDeg');
    const gcEl = document.getElementById('cfgGripCloseDeg');
    if (srvEl) gripperState.servoIndex = parseInt(srvEl.value, 10) || 0;
    if (goEl) gripperState.openAngle = parseInt(goEl.value, 10) || 35;
    if (gcEl) gripperState.closeAngle = parseInt(gcEl.value, 10) || 145;

    toSave.gripper = {
        servoIndex: gripperState.servoIndex,
        openAngle: gripperState.openAngle,
        closeAngle: gripperState.closeAngle
    };

    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(toSave));
    } catch (_) {}

    updateJointCardsUI();
    updateCurrentUI();
    showStatusOverlay('✓ Sensoren, Endanschläge & Zuordnung gespeichert', 2000);
}

function setupDeviceName() {
    fetch('/getConfig')
        .then(r => r.json())
        .then(cfg => {
            const name = (cfg.hotspot_ssid || cfg.wifi_ssid || 'TinkerThinker').trim();
            const el = document.getElementById('deviceName');
            if (el) el.textContent = name;
            document.title = `${name} Roboter-Arm`;
        })
        .catch(() => {});
}

// --- Animations-Loop (60 FPS) ---

let lastFrameMs = performance.now();
function animationLoop(nowMs) {
    const dtSec = Math.min(0.1, Math.max(0.001, (nowMs - lastFrameMs) / 1000.0));
    lastFrameMs = nowMs;

    updateHomingAndPhysics(dtSec, nowMs);
    renderArmCanvas();

    requestAnimationFrame(animationLoop);
}

window.addEventListener('DOMContentLoaded', () => {
    loadArmConfig();
    bindHoldButtons();
    setupModeTabs();
    setupDualJoysticks();

    const saveBtn = document.getElementById('btnSaveArmConfig');
    if (saveBtn) {
        saveBtn.addEventListener('click', saveArmConfigFromUI);
    }

    connectWebSocket();
    setupDeviceName();
    updateJointCardsUI();
    updateCurrentUI();
    requestAnimationFrame(animationLoop);
});
