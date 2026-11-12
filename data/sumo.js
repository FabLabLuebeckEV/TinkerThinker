// SchrottBot-Sumo: Arme (2 Servos gegenläufig), Rampe hoch, Rampe vor.
// Fahren, Akku, Funk und LED kommen aus basic.js (sendWS, gemeinsame WebSocket-Verbindung).
(() => {
    const ROLES = [
        { key: 'arms', ws: 'sumo_arms', slider: 'armsSlider', val: 'armsVal' },
        { key: 'lift', ws: 'sumo_lift', slider: 'liftSlider', val: 'liftVal' },
        { key: 'push', ws: 'sumo_push', slider: 'pushSlider', val: 'pushVal' },
    ];
    const SERVO_ROWS = [
        { key: 'arm_l', name: 'Arm links' },
        { key: 'arm_r', name: 'Arm rechts' },
        { key: 'lift', name: 'Rampe hoch (Heber)' },
        { key: 'push', name: 'Rampe vor (Schieber)' },
    ];

    const dragging = {};   // Regler, die gerade bedient werden, nicht von der Telemetrie überschreiben
    const pending = {};    // zuletzt gewünschter Wert je Rolle, gesendet höchstens alle 50 ms
    let sendTimer = null;

    function showValue(role, v) {
        const el = document.getElementById(role.val);
        if (el) el.textContent = `${Math.round(v)} %`;
    }

    function flush() {
        sendTimer = null;
        const msg = {};
        Object.keys(pending).forEach(k => { msg[k] = pending[k]; delete pending[k]; });
        if (Object.keys(msg).length) sendWS(msg);
    }

    function setRole(role, v) {
        pending[role.ws] = Number(v);
        showValue(role, v);
        if (!sendTimer) sendTimer = setTimeout(flush, 50);
    }

    ROLES.forEach(role => {
        const s = document.getElementById(role.slider);
        const start = () => { dragging[role.key] = true; };
        const end = () => { dragging[role.key] = false; setRole(role, s.value); };
        s.addEventListener('input', () => setRole(role, s.value));
        s.addEventListener('pointerdown', start);
        s.addEventListener('touchstart', start, { passive: true });
        s.addEventListener('pointerup', end);
        s.addEventListener('touchend', end);
        s.addEventListener('change', end);
    });

    function armsTo(v) {
        const s = document.getElementById('armsSlider');
        s.value = v;
        setRole(ROLES[0], v);
    }
    document.getElementById('armsClose').addEventListener('click', () => armsTo(0));
    document.getElementById('armsOpen').addEventListener('click', () => armsTo(100));

    // Telemetrie aus basic.js: [Arme, Rampe hoch, Rampe vor] in %, z.B. wenn der Controller sie bewegt
    window.onSumoTelemetry = (vals) => {
        ROLES.forEach((role, i) => {
            const v = Number(vals[i]);
            if (!Number.isFinite(v) || dragging[role.key] || pending[role.ws] !== undefined) return;
            document.getElementById(role.slider).value = v;
            showValue(role, v);
        });
    };

    // ── Servo-Einstellungen ─────────────────────────────────
    const grid = document.getElementById('sumoGrid');
    const status = document.getElementById('sumoStatus');
    const inputs = {};

    function numInput(min, max) {
        const e = document.createElement('input');
        e.type = 'number'; e.min = min; e.max = max; e.step = 1;
        return e;
    }

    SERVO_ROWS.forEach(row => {
        const label = document.createElement('span');
        label.textContent = row.name;
        const servo = document.createElement('select');
        for (let n = 1; n <= 7; n++) {
            const o = document.createElement('option');
            o.value = n; o.textContent = `Servo ${n}`;
            servo.appendChild(o);
        }
        const a0 = numInput(0, 180);
        const a1 = numInput(0, 180);
        grid.append(label, servo, a0, a1);
        inputs[row.key] = { servo, a0, a1 };
    });

    function fill(cfg) {
        SERVO_ROWS.forEach(row => {
            const c = cfg[row.key];
            if (!c) return;
            inputs[row.key].servo.value = c.servo;
            inputs[row.key].a0.value = c.a0;
            inputs[row.key].a1.value = c.a1;
        });
    }

    function loadSettings() {
        fetch('/sumo/config')
            .then(r => {
                if (!r.ok) throw new Error(`HTTP ${r.status}`);
                return r.json();
            })
            .then(fill)
            .catch(e => { status.textContent = `Laden fehlgeschlagen: ${e.message}`; status.style.color = '#c62828'; });
    }

    document.getElementById('sumoSave').addEventListener('click', () => {
        const body = {};
        for (const row of SERVO_ROWS) {
            const i = inputs[row.key];
            const a0 = parseInt(i.a0.value, 10);
            const a1 = parseInt(i.a1.value, 10);
            if (!(a0 >= 0 && a0 <= 180 && a1 >= 0 && a1 <= 180)) {
                status.textContent = `${row.name}: Winkel 0-180 eingeben`;
                status.style.color = '#c62828';
                return;
            }
            body[row.key] = { servo: parseInt(i.servo.value, 10), a0, a1 };
        }
        status.textContent = 'Speichern…';
        status.style.color = '#555';
        fetch('/sumo/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
            .then(r => r.text().then(t => {
                if (!r.ok) throw new Error(`HTTP ${r.status}: ${t}`);
                return JSON.parse(t);
            }))
            .then(cfg => { fill(cfg); status.textContent = 'Gespeichert'; status.style.color = '#2e7d32'; })
            .catch(e => { status.textContent = `Fehler: ${e.message}`; status.style.color = '#c62828'; });
    });

    loadSettings();
})();
