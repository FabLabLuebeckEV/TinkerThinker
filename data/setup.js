let currentStep = 1;
let ws;
let configData = {};
let reconnectTimer = null;
let reconnectDelayMs = 1000;
const maxReconnectDelayMs = 30000;
const ARM_STORAGE_KEY = 'tinkerthinker_arm_config_v4';

// motorSettings speichert für jeden Motor: side (left/right/gripp/base/arm1/arm2/arm3/none), invert (bool), deadband (int)
let motorSettings = {
    A: { side: null, invert: false, deadband: 0 },
    B: { side: null, invert: false, deadband: 0 },
    C: { side: null, invert: false, deadband: 0 },
    D: { side: null, invert: false, deadband: 0 }
};

const ARM_JOINT_META = {
    base: { num: '①', badgeClass: 'num-1', badgeNum: '1', title: '① Basis (Drehkranz)', sub: '360° frei · OHNE Stromsensor (Motor C/D)', needsSensor: false },
    arm1: { num: '②', badgeClass: 'num-2', badgeNum: '2', title: '② Arm 1 (Unterarm / Schulter)', sub: '180° · ⚡ MIT Stromsensor (Motor A/B)', needsSensor: true },
    arm2: { num: '③', badgeClass: 'num-3', badgeNum: '3', title: '③ Arm 2 (Mittelarm / Ellbogen)', sub: '180° · ⚡ MIT Stromsensor (Motor A/B)', needsSensor: true },
    arm3: { num: '④', badgeClass: 'num-4', badgeNum: '4', title: '④ Arm 3 (Handgelenk / Oberarm)', sub: '180° links vom gelben Teil · OHNE Sensor (Motor C/D)', needsSensor: false }
};

document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    loadConfig();
    setupDeadbandSliders();
});

function isArmMode() {
    const sel = document.getElementById('gui_mode');
    return sel ? (sel.value === 'arm') : (configData.gui_mode === 'arm');
}

function goToStep(n) {
    const prevEl = document.getElementById('step' + currentStep);
    if (prevEl) prevEl.classList.remove('active');
    currentStep = n;
    const nextEl = document.getElementById('step' + n);
    if (nextEl) nextEl.classList.add('active');

    // Diagramm-Karte im Roboter-Arm-Modus ab Schritt 2 bis 16 einblenden
    const diagCard = document.getElementById('armSetupDiagramCard');
    if (diagCard) {
        diagCard.style.display = (isArmMode() && n >= 2 && n <= 16) ? 'block' : 'none';
    }

    // Aktives Gelenk im Diagramm hervorheben
    let highlight = null;
    if (n === 4 || n === 5) highlight = motorSettings.A.side;
    else if (n === 7 || n === 8) highlight = motorSettings.B.side;
    else if (n === 10 || n === 11) highlight = motorSettings.C.side;
    else if (n === 13 || n === 14) highlight = motorSettings.D.side;
    else if (n === 15) highlight = 'gripper';

    updateArmDiagramStatus(highlight);
}

function startMotorWizard() {
    resetMotorAssignments();
    if (isArmMode()) {
        // Im Roboter-Arm-Modus immer mit den Anschlüssen OHNE Stromsensor starten (Motor C, dann D, danach A und B)
        goToStep(9);
    } else {
        goToStep(3);
    }
}

function restartMotorWizard() {
    // Alle laufenden Motoren sicher stoppen
    ['A', 'B', 'C', 'D'].forEach(l => sendWS({ ['motor' + l]: 'stop' }));
    resetMotorAssignments();
    goToStep(2);
}

function resetMotorAssignments() {
    ['A', 'B', 'C', 'D'].forEach(letter => {
        motorSettings[letter].side = null;
        const qEl = document.getElementById('motor' + letter + 'Question');
        if (qEl) qEl.style.display = 'none';
        const errEl = document.getElementById('portError' + letter);
        if (errEl) {
            errEl.style.display = 'none';
            errEl.innerHTML = '';
        }
    });
    updateArmDiagramStatus(null);
}

function updateArmDiagramStatus(highlightJoint) {
    if (!isArmMode()) return;

    const assignedByRole = {};
    ['A', 'B', 'C', 'D'].forEach(letter => {
        const s = motorSettings[letter].side;
        if (s && s !== 'none') {
            assignedByRole[s] = letter;
        }
    });

    ['base', 'arm1', 'arm2', 'arm3'].forEach(role => {
        const legendEl = document.getElementById('legend_' + role);
        const statusEl = document.getElementById('status_' + role);
        const svgGroup = document.getElementById('svgPart_' + role);
        const assignedMotor = assignedByRole[role];

        if (legendEl) {
            legendEl.classList.toggle('active-joint', highlightJoint === role);
            legendEl.classList.toggle('assigned-joint', !!assignedMotor);
        }
        if (statusEl) {
            if (assignedMotor) {
                statusEl.textContent = `✓ Motor ${assignedMotor}`;
                statusEl.classList.add('done');
            } else {
                statusEl.textContent = 'Offen';
                statusEl.classList.remove('done');
            }
        }
        if (svgGroup) {
            svgGroup.style.opacity = (!highlightJoint || highlightJoint === role) ? '1' : '0.42';
        }
    });

    const gripLegend = document.getElementById('legend_gripper');
    const gripStatus = document.getElementById('status_gripper');
    const gripSvg = document.getElementById('svgPart_gripper');
    const srvSel = document.getElementById('setup_servo_ch');
    const srvIdx = srvSel ? (parseInt(srvSel.value, 10) || 0) : 0;

    if (gripLegend) {
        gripLegend.classList.toggle('active-joint', highlightJoint === 'gripper');
    }
    if (gripStatus) {
        gripStatus.textContent = `Servo ${srvIdx + 1}`;
        if (currentStep > 15) gripStatus.classList.add('done');
        else gripStatus.classList.remove('done');
    }
    if (gripSvg) {
        gripSvg.style.opacity = (!highlightJoint || highlightJoint === 'gripper') ? '1' : '0.5';
    }
}

function renderMotorRoleButtons(guiMode) {
    const letters = ['A', 'B', 'C', 'D'];
    letters.forEach(letter => {
        const container = document.getElementById('motor' + letter + 'Question');
        const badgeEl = document.getElementById('sensorBadge' + letter);
        const descEl = document.getElementById('testDesc' + letter);
        const hasSensor = (letter === 'A' || letter === 'B');

        if (badgeEl) {
            badgeEl.style.display = (guiMode === 'arm') ? 'inline-flex' : 'none';
        }
        if (descEl) {
            if (guiMode === 'arm') {
                if (!hasSensor) {
                    descEl.innerHTML = `Wir testen jetzt <strong>Motor ${letter} (OHNE Stromsensor)</strong>. Hier sollten <strong>① Basis</strong> oder <strong>④ Arm 3 (Handgelenk)</strong> angeschlossen sein. Klicke auf Testen und wähle dann die Nummer, die sich bewegt hat:`;
                } else {
                    descEl.innerHTML = `Wir testen jetzt <strong>Motor ${letter} (⚡ MIT Stromsensor)</strong>. Hier müssen <strong>② Arm 1 (Unterarm)</strong> oder <strong>③ Arm 2 (Mittelarm)</strong> angeschlossen sein. Klicke auf Testen und wähle die Nummer:`;
                }
            } else {
                descEl.textContent = `Wir bewegen jetzt Motor ${letter} vorwärts. Bitte beobachte, welcher Motor sich bewegt, und klicke dann auf den entsprechenden Button.`;
            }
        }

        if (!container) return;
        if (guiMode === 'arm') {
            container.innerHTML = `
                <p style="font-weight:700; margin-top:12px;">Welches nummerierte Teil (① bis ④) hat sich bewegt?</p>
                <div class="role-choice-grid">
                    <button type="button" class="role-choice-btn" onclick="motorMoved('${letter}','base')">
                        <span class="num-badge num-1">1</span>
                        <div>
                            Basis (Drehkranz)
                            <span class="role-sub">360° frei · OHNE Stromsensor</span>
                        </div>
                    </button>
                    <button type="button" class="role-choice-btn" onclick="motorMoved('${letter}','arm1')">
                        <span class="num-badge num-2">2</span>
                        <div>
                            Arm 1 (Unterarm / Schulter)
                            <span class="role-sub">⚡ MIT Stromsensor (Motor A/B)</span>
                        </div>
                    </button>
                    <button type="button" class="role-choice-btn" onclick="motorMoved('${letter}','arm2')">
                        <span class="num-badge num-3">3</span>
                        <div>
                            Arm 2 (Mittelarm / Ellbogen)
                            <span class="role-sub">⚡ MIT Stromsensor (Motor A/B)</span>
                        </div>
                    </button>
                    <button type="button" class="role-choice-btn" onclick="motorMoved('${letter}','arm3')">
                        <span class="num-badge num-4">4</span>
                        <div>
                            Arm 3 (Handgelenk / Oberarm)
                            <span class="role-sub">Links vom gelben Teil · OHNE Sensor</span>
                        </div>
                    </button>
                    <button type="button" class="role-choice-btn none-btn" onclick="motorMoved('${letter}','none')">
                        Kein Motor / Nichts hat sich bewegt
                    </button>
                </div>
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
    const armHint = document.getElementById('armSetupHintBox');
    const diagCard = document.getElementById('armSetupDiagramCard');
    if (!sel || !intro) return;

    renderMotorRoleButtons(sel.value);
    if (armHint) armHint.style.display = (sel.value === 'arm') ? 'block' : 'none';
    if (diagCard) diagCard.style.display = (sel.value === 'arm' && currentStep >= 2 && currentStep <= 16) ? 'block' : 'none';

    if (sel.value === 'arm') {
        intro.textContent = 'Roboter-Arm ausgewählt: Orientiere dich an den Nummern ① bis ⑤ in der Grafik oben. Wir starten mit Motor C & D (ohne Stromsensor) und prüfen danach Motor A & B (mit Stromsensor).';
        updateArmDiagramStatus(null);
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
        if (titleEl) titleEl.textContent = `① Drehrichtung Basis (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Dreht sich ① Basis (Drehkranz) beim Test im Uhrzeigersinn (Rechts ↻) oder gegen den Uhrzeigersinn (Links ↺)?`;
        if (fwdBtn) fwdBtn.textContent = 'Rechts ↻ (+ Uhrzeigersinn)';
        if (bwdBtn) bwdBtn.textContent = 'Links ↺ (- Gegen Uhrzeigersinn)';
    } else if (side === 'arm1') {
        if (titleEl) titleEl.textContent = `② Bewegungsrichtung Arm 1 / Unterarm (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Bewegt sich ② Arm 1 (Unterarm) beim Test nach rechts/hinten (+90°) oder nach links/vorne (-90°)?`;
        if (fwdBtn) fwdBtn.textContent = 'Nach rechts / hinten (+90°)';
        if (bwdBtn) bwdBtn.textContent = 'Nach links / vorne (-90°)';
    } else if (side === 'arm2') {
        if (titleEl) titleEl.textContent = `③ Bewegungsrichtung Arm 2 / Mittelarm (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Bewegt sich ③ Arm 2 (Mittelarm) beim Test nach oben/rechts (+90°) oder nach unten/links (-90°)?`;
        if (fwdBtn) fwdBtn.textContent = 'Hoch / Rechts (+90°)';
        if (bwdBtn) bwdBtn.textContent = 'Runter / Links (-90°)';
    } else if (side === 'arm3') {
        if (titleEl) titleEl.textContent = `④ Bewegungsrichtung Arm 3 / Handgelenk (Motor ${letter})`;
        if (promptEl) promptEl.textContent = `Bewegt sich ④ Arm 3 (Handgelenk) beim Test nach oben zur senkrechten Startposition (+90°, weg vom gelben Bauteil) oder über die linke Seite nach unten (-90°)?`;
        if (fwdBtn) fwdBtn.textContent = 'Nach oben zur Startposition (+90°)';
        if (bwdBtn) bwdBtn.textContent = 'Über linke Seite nach unten (-90°)';
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
      .then(r => r.json())
      .then(data => {
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
        if (configData.arm_config && configData.arm_config.gripper) {
            const g = configData.arm_config.gripper;
            const sCh = document.getElementById('setup_servo_ch');
            const gOpen = document.getElementById('setup_grip_open');
            const gClose = document.getElementById('setup_grip_close');
            if (sCh && g.servoIndex !== undefined) sCh.value = String(g.servoIndex);
            if (gOpen && g.openAngle !== undefined) gOpen.value = String(g.openAngle);
            if (gClose && g.closeAngle !== undefined) gClose.value = String(g.closeAngle);
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
    const errEl = document.getElementById('portError' + letter);
    if (errEl) errEl.style.display = 'none';
    // Motor starten (vorwärts) für max. 1s
    sendWS({ ["motor" + letter]: "forward" });
    setTimeout(() => {
        sendWS({ ["motor" + letter]: "stop" });
        const qEl = document.getElementById('motor' + letter + 'Question');
        if (qEl) qEl.style.display = 'block';
    }, 1000);
}

function nextMotorStartStep(letter) {
    if (isArmMode()) {
        // Reihenfolge beim Roboter-Arm: C (9) -> D (12) -> A (3) -> B (6) -> Greifer-Servo (15)
        if (letter === 'C') return 12; // Nach C kommt Motor D (auch ohne Stromsensor)
        if (letter === 'D') return 3;  // Nach D kommt Motor A (mit Stromsensor)
        if (letter === 'A') return 6;  // Nach A kommt Motor B (mit Stromsensor)
        return 15;                     // Nach B kommt ⑤ Greifer-Servo
    }
    // Reihenfolge für Basic & Drive: A (3) -> B (6) -> C (9) -> D (12) -> Speichern (16)
    if (letter === 'A') return 6;
    if (letter === 'B') return 9;
    if (letter === 'C') return 12;
    return 16;
}

function showWrongPortError(letter, side) {
    const errEl = document.getElementById('portError' + letter);
    if (!errEl) return;

    const meta = ARM_JOINT_META[side];
    const portHasSensor = (letter === 'A' || letter === 'B');

    if (portHasSensor && !meta.needsSensor) {
        // Motor A oder B (MIT Stromsensor), aber Basis (1) oder Arm 3 (4) bewegt sich!
        errEl.innerHTML = `
            <h3>⚠️ Hey, Fehler! Stromsensor wird hier nicht benötigt!</h3>
            <p>Du testest gerade <strong>Motor ${letter} (⚡ Anschluss MIT Stromsensor)</strong>, aber es hat sich <strong>${meta.title}</strong> bewegt.</p>
            <p>Unsere Platine hat nur 2 Stromsensoren (auf <strong>Motor A</strong> und <strong>Motor B</strong>). Diese werden zwingend für <strong>② Arm 1 (Unterarm)</strong> und <strong>③ Arm 2 (Mittelarm)</strong> benötigt, um deren Endanschläge automatisch zu erkennen!</p>
            <div class="port-error-steps">
                <strong>Bitte jetzt am Board umstecken:</strong>
                <ol>
                    <li>Ziehe das Kabel von <strong>${meta.title}</strong> an <strong>Motor ${letter}</strong> ab und stecke es auf <strong>Motor C</strong> oder <strong>Motor D</strong> (Anschluss ohne Stromsensor).</li>
                    <li>Stecke stattdessen <strong>② Arm 1 (Unterarm)</strong> oder <strong>③ Arm 2 (Mittelarm)</strong> an <strong>Motor ${letter}</strong> an.</li>
                    <li>Klicke danach auf den roten Button, um das Setup sauber von vorne zu beginnen.</li>
                </ol>
            </div>
            <button type="button" class="btn-restart-error" onclick="restartMotorWizard()">🔄 Kabel umgesteckt - Von vorne beginnen</button>
        `;
    } else if (!portHasSensor && meta.needsSensor) {
        // Motor C oder D (OHNE Stromsensor), aber Arm 1 (2) oder Arm 2 (3) bewegt sich!
        errEl.innerHTML = `
            <h3>⚠️ Hey, Fehler! Falscher Anschluss (Stromsensor fehlt hier)!</h3>
            <p>Du testest gerade <strong>Motor ${letter} (🔌 Anschluss OHNE Stromsensor)</strong>, aber es hat sich <strong>${meta.title}</strong> bewegt.</p>
            <p><strong>${meta.title}</strong> braucht zwingend einen <strong>Stromsensor</strong> für die Endanschlag-Erkennung und darf deshalb nur an <strong>Motor A</strong> oder <strong>Motor B</strong> angeschlossen werden!</p>
            <div class="port-error-steps">
                <strong>Bitte jetzt am Board umstecken:</strong>
                <ol>
                    <li>Stecke das Kabel von <strong>${meta.title}</strong> von <strong>Motor ${letter}</strong> auf <strong>Motor A</strong> oder <strong>Motor B</strong> (mit Stromsensor) um.</li>
                    <li>Stecke an <strong>Motor C</strong> und <strong>Motor D</strong> ausschließlich <strong>① Basis</strong> und <strong>④ Arm 3 (Handgelenk)</strong> an.</li>
                    <li>Klicke danach auf den roten Button, um das Setup sauber von vorne zu beginnen.</li>
                </ol>
            </div>
            <button type="button" class="btn-restart-error" onclick="restartMotorWizard()">🔄 Kabel umgesteckt - Von vorne beginnen</button>
        `;
    }

    errEl.style.display = 'block';
    errEl.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function showDuplicateRoleError(letter, side, existingLetter) {
    const errEl = document.getElementById('portError' + letter);
    if (!errEl) return;
    const meta = ARM_JOINT_META[side];
    errEl.innerHTML = `
        <h3>⚠️ Hey, Fehler! ${meta.title} ist bereits vergeben!</h3>
        <p>Du hast <strong>${meta.title}</strong> in diesem Durchlauf bereits <strong>Motor ${existingLetter}</strong> zugeordnet.</p>
        <p>Bitte prüfe, welches Teil sich wirklich bei <strong>Motor ${letter}</strong> bewegt hat, oder starte das Setup neu, falls du dich beim vorherigen Motor verklickt hast.</p>
        <button type="button" class="btn-restart-error" onclick="restartMotorWizard()">🔄 Zuordnung zurücksetzen &amp; von vorne beginnen</button>
    `;
    errEl.style.display = 'block';
}

function motorMoved(letter, side) {
    // Motor stoppen
    sendWS({ ["motor" + letter]: "stop" });

    const errEl = document.getElementById('portError' + letter);
    if (errEl) errEl.style.display = 'none';

    // Im Roboter-Arm-Modus: prüfen, ob der Motor am richtigen Anschlusstyp (mit/ohne Stromsensor) steckt!
    if (isArmMode() && side !== 'none' && ARM_JOINT_META[side]) {
        const meta = ARM_JOINT_META[side];
        const portHasSensor = (letter === 'A' || letter === 'B');
        if (portHasSensor !== meta.needsSensor) {
            showWrongPortError(letter, side);
            return;
        }
        // Prüfen, ob dieses Gelenk schon einem anderen Motor zugeordnet wurde
        const otherLetter = ['A', 'B', 'C', 'D'].find(l => l !== letter && motorSettings[l].side === side);
        if (otherLetter) {
            showDuplicateRoleError(letter, side, otherLetter);
            return;
        }
    }

    motorSettings[letter].side = side;
    updateArmDiagramStatus(side);

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
    if (letter === 'A') goToStep(4);
    else if (letter === 'B') goToStep(7);
    else if (letter === 'C') goToStep(10);
    else if (letter === 'D') goToStep(13);
}

function directionChosen(letter, dir) {
    // invert = true, wenn backward
    motorSettings[letter].invert = (dir === 'backward');
    // Weiter zum Deadband
    if (letter === 'A') goToStep(5);
    else if (letter === 'B') goToStep(8);
    else if (letter === 'C') goToStep(11);
    else if (letter === 'D') goToStep(14);
}

function testDirection(letter, dir) {
    // Richtung 1s testen (vorwärts/rückwärts)
    sendWS({ ["motor" + letter]: dir });
    setTimeout(() => {
        sendWS({ ["motor" + letter]: "stop" });
    }, 1000);
}

function testDeadband(letter) {
    const slider = document.getElementById('db_slider_' + letter);
    let val = slider ? slider.value : 0;
    let idx = letterToIndex(letter);
    // Raw PWM senden (Deadband soll nicht dazwischenfunken)
    sendWS({ "motor_raw": { "motor": idx, "pwm": parseInt(val, 10) } });
    setTimeout(() => {
        sendWS({ "motor_raw": { "motor": idx, "pwm": 0 } });
    }, 1000);
}

function confirmDeadband(letter) {
    let val = document.getElementById('db_slider_' + letter).value;
    motorSettings[letter].deadband = parseInt(val, 10);
    // Motor stoppen
    let idx = letterToIndex(letter);
    sendWS({ "motor_raw": { "motor": idx, "pwm": 0 } });

    // Nächster Schritt
    goToStep(nextMotorStartStep(letter));
}

function testSetupGripper(action) {
    const srvSel = document.getElementById('setup_servo_ch');
    const openEl = document.getElementById('setup_grip_open');
    const closeEl = document.getElementById('setup_grip_close');
    const ch = srvSel ? (parseInt(srvSel.value, 10) || 0) : 0;
    const deg = (action === 'open')
        ? (openEl ? parseInt(openEl.value, 10) : 35)
        : (closeEl ? parseInt(closeEl.value, 10) : 145);
    const clamped = Math.max(0, Math.min(180, isNaN(deg) ? 90 : deg));
    sendWS({
        servo: {
            channel: ch,
            angle: clamped
        }
    });
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

function buildArmConfigPayload() {
    let armCfg = {};
    if (configData.arm_config && typeof configData.arm_config === 'object') {
        armCfg = JSON.parse(JSON.stringify(configData.arm_config));
    } else {
        try {
            const raw = localStorage.getItem(ARM_STORAGE_KEY) || localStorage.getItem('tinkerthinker_arm_config_v1');
            if (raw) armCfg = JSON.parse(raw);
        } catch (_) {}
    }

    const defaults = {
        base: { motorIndex: 2, invert: false, sensorMode: 'none', homeEnd: 'max', stallLimit: 9.9, time90: 3.5 },
        arm1: { motorIndex: 0, invert: false, sensorMode: '1', homeEnd: 'max', stallLimit: 1.05, time90: 3.0 },
        arm2: { motorIndex: 1, invert: false, sensorMode: '0', homeEnd: 'max', stallLimit: 0.95, time90: 2.6 },
        arm3: { motorIndex: 3, invert: false, sensorMode: 'none', homeEnd: 'max', stallLimit: 0.85, time90: 2.0 }
    };

    ['base', 'arm1', 'arm2', 'arm3'].forEach(role => {
        armCfg[role] = Object.assign({}, defaults[role], armCfg[role] || {});
    });

    const letters = ['A', 'B', 'C', 'D'];
    for (const letter of letters) {
        const role = motorSettings[letter].side;
        if (role === 'base' || role === 'arm1' || role === 'arm2' || role === 'arm3') {
            const mIdx = letterToIndex(letter);
            armCfg[role].motorIndex = mIdx;
            // Da motor_invert_X direkt in der ESP32-Firmware gespeichert wird, bleibt Software-Invert false
            armCfg[role].invert = false;
            if (role === 'base' || role === 'arm3') {
                armCfg[role].sensorMode = 'none';
            } else if (mIdx === 0) {
                armCfg[role].sensorMode = '1'; // Motor A = Sensor 2 (GPIO 36)
            } else if (mIdx === 1) {
                armCfg[role].sensorMode = '0'; // Motor B = Sensor 1 (GPIO 34)
            }
        }
    }

    const srvSel = document.getElementById('setup_servo_ch');
    const openEl = document.getElementById('setup_grip_open');
    const closeEl = document.getElementById('setup_grip_close');
    armCfg.gripper = {
        servoIndex: srvSel ? (parseInt(srvSel.value, 10) || 0) : 0,
        openAngle: openEl ? (parseInt(openEl.value, 10) || 35) : 35,
        closeAngle: closeEl ? (parseInt(closeEl.value, 10) || 145) : 145
    };

    return armCfg;
}

function finalizeSetup() {
    // POST /config mit allen Werten
    let formData = new FormData();
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
        const armCfg = buildArmConfigPayload();
        const armJson = JSON.stringify(armCfg);
        formData.append("arm_config", armJson);
        try {
            localStorage.setItem(ARM_STORAGE_KEY, armJson);
            localStorage.setItem('tinkerthinker_arm_config_v1', armJson);
        } catch (_) {}
    }

    const drivePair = resolveDrivePair();
    const gripperIdx = resolveGripperMotor(drivePair);
    formData.append("motor_left_gui", String(drivePair.leftIdx));
    formData.append("motor_right_gui", String(drivePair.rightIdx));
    formData.append("motor_gripper_gui", String(gripperIdx));

    // Motoren: invert + deadband; Frequenz beibehalten
    let letters = ['A', 'B', 'C', 'D'];
    for (let i = 0; i < 4; i++) {
        let lettr = letters[i];
        if (motorSettings[lettr].invert) {
            formData.append(`motor_invert_${i}`, "on");
        }
        formData.append(`motor_deadband_${i}`, motorSettings[lettr].deadband.toString());

        let freq = (configData.motor_frequency && configData.motor_frequency[i] !== undefined)
            ? configData.motor_frequency[i]
            : 5000;
        formData.append(`motor_frequency_${i}`, freq.toString());
    }

    formData.append("led_count", String(configData.led_count || 30));
    if (configData.ota_enabled) formData.append("ota_enabled", "on");
    if (configData.legacy_board) formData.append("legacy_board", "on");

    for (let i = 0; i < 7; i++) {
        const minPw = configData.servo_settings?.[i]?.min_pulsewidth ?? 500;
        const maxPw = configData.servo_settings?.[i]?.max_pulsewidth ?? 2500;
        formData.append(`servo${i}_min`, String(minPw));
        formData.append(`servo${i}_max`, String(maxPw));
    }

    fetch('/config', {
        method: 'POST',
        body: formData
    }).then(() => {
        goToStep(17);
        setTimeout(() => {
            fetch('/reboot').catch(() => {});
        }, 400);
    }).catch((err) => {
        console.error('Setup speichern fehlgeschlagen:', err);
        alert('Speichern fehlgeschlagen. Bitte erneut versuchen.');
    });
}

function setupDeadbandSliders() {
    const letters = ['A', 'B', 'C', 'D'];
    letters.forEach(letter => {
        const slider = document.getElementById('db_slider_' + letter);
        if (!slider) return;
        slider.addEventListener('input', () => updateDeadbandValue(letter));
    });
}

function initDeadbandValues() {
    const letters = ['A', 'B', 'C', 'D'];
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
