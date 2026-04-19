(() => {
  // ── Constants ────────────────────────────────────────────
  const BUTTON_CODES = ['BTN_A','BTN_B','BTN_X','BTN_Y','BUTTON_L1','BUTTON_R1','BUTTON_L2','BUTTON_R2','BUTTON_STICK_L','BUTTON_STICK_R'];
  const DPAD_DIRS    = ['UP','DOWN','LEFT','RIGHT'];
  const EDGES        = ['press','release','hold'];
  const AXES         = ['X','Y','RX','RY'];
  const ACTION_TYPES = ['motor_direct','drive_pair','servo_set','servo_toggle_band','servo_nudge','servo_axes','led_set','gpio_set','speed_adjust'];
  const INPUT_TYPES  = ['button','dpad','axis_pair'];

  const EXAMPLES = [
    { input:{type:'dpad',dir:'UP',edge:'hold'},
      action:{type:'motor_direct',motor:0,pwm:200} },
    { input:{type:'button',code:'BTN_A',edge:'press'},
      action:{type:'led_set',start:0,count:5,color:'#ff0000'} },
    { input:{type:'axis_pair',x:'X',y:'Y',deadband:16,invertX:false,invertY:false},
      action:{type:'drive_pair',target:'gui'} },
  ];

  // ── State ─────────────────────────────────────────────────
  let bindingModel  = []; // [{id, input, action}]
  let nextId        = 0;
  let selectedKey   = null;
  const panelEditors = {}; // id -> {collectInput, collectAction}
  const st = document.getElementById('status');

  // ── DOM helpers ───────────────────────────────────────────
  function el(tag, attrs, children) {
    const e = document.createElement(tag);
    if (attrs) Object.entries(attrs).forEach(([k,v]) => {
      if (k === 'class') e.className = v;
      else if (k === 'style') e.setAttribute('style', v);
      else e[k] = v;
    });
    if (children) {
      const arr = Array.isArray(children) ? children : [children];
      arr.forEach(c =>
        e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c)
      );
    }
    return e;
  }

  function mkSel(arr, val) {
    const s = document.createElement('select');
    arr.forEach(x => {
      const o = document.createElement('option');
      o.value = x; o.textContent = x; if (x === val) o.selected = true;
      s.appendChild(o);
    });
    return s;
  }

  function mkNum(min, max, val, w) {
    return el('input', {type:'number', min, max, value: String(val ?? min), style:`width:${w||68}px;`});
  }

  function hint(txt) { return el('div',{class:'hint'},[txt]); }
  function frow(children) { return el('div',{class:'frow'},children); }

  // ── Key helpers ───────────────────────────────────────────
  function keyFromBinding(b) {
    const i = b.input;
    if (!i) return null;
    if (i.type === 'dpad')      return 'dpad_' + i.dir;
    if (i.type === 'button')    return 'button_' + i.code;
    if (i.type === 'axis_pair') {
      if (i.x === 'X'  && i.y === 'Y' ) return 'axis_LStick';
      if (i.x === 'RX' && i.y === 'RY') return 'axis_RStick';
      return 'axis_other';
    }
    return null;
  }

  function defaultInputForKey(key) {
    if (key.startsWith('dpad_'))   return {type:'dpad',   dir: key.split('_')[1],  edge:'hold'};
    if (key.startsWith('button_')) return {type:'button',  code: key.substring(7), edge:'press'};
    if (key === 'axis_LStick') return {type:'axis_pair', x:'X',  y:'Y',  deadband:16, invertX:false, invertY:false};
    if (key === 'axis_RStick') return {type:'axis_pair', x:'RX', y:'RY', deadband:16, invertX:false, invertY:false};
    return {type:'button', code:'BTN_A', edge:'press'};
  }

  function defaultActionForKey(key) {
    if (key === 'axis_LStick' || key === 'axis_RStick') return {type:'drive_pair', target:'gui'};
    return {type:'motor_direct', motor:0, pwm:200};
  }

  const KEY_LABELS = {
    dpad_UP:'DPAD ↑ Oben', dpad_DOWN:'DPAD ↓ Unten',
    dpad_LEFT:'DPAD ← Links', dpad_RIGHT:'DPAD → Rechts',
    'button_BTN_A':'Kreuz ×', 'button_BTN_B':'Kreis ○',
    'button_BTN_X':'Viereck □', 'button_BTN_Y':'Dreieck △',
    button_BUTTON_L1:'L1', button_BUTTON_L2:'L2',
    button_BUTTON_R1:'R1', button_BUTTON_R2:'R2',
    button_BUTTON_STICK_L:'L3 (Stick-Klick)', button_BUTTON_STICK_R:'R3 (Stick-Klick)',
    axis_LStick:'Linker Stick (X / Y)', axis_RStick:'Rechter Stick (RX / RY)',
  };
  function keyLabel(k) { return KEY_LABELS[k] || k; }

  function inputSummary(inp) {
    if (!inp) return '?';
    if (inp.type === 'dpad')      return `DPAD ${inp.dir} · ${inp.edge}`;
    if (inp.type === 'button')    return `${inp.code} · ${inp.edge}`;
    if (inp.type === 'axis_pair') return `axis ${inp.x}/${inp.y}`;
    return inp.type;
  }

  function actionSummary(act) {
    if (!act) return '?';
    if (act.type === 'motor_direct')  return `motor${act.motor} PWM ${act.pwm}`;
    if (act.type === 'drive_pair')    return `drive_pair ${act.target}`;
    if (act.type === 'led_set')       return `led ${act.color}`;
    if (act.type === 'servo_set')     return `servo${act.servo} ${act.angle}°`;
    if (act.type === 'servo_nudge')   return `servo${act.servo} ${act.delta > 0 ? '+' : ''}${act.delta}°`;
    if (act.type === 'speed_adjust')  return `speed ${act.delta > 0 ? '+' : ''}${act.delta}`;
    return act.type;
  }

  // ── Action editor ─────────────────────────────────────────
  function makeActionEditor(action) {
    const root      = el('div');
    const typeSel   = mkSel(ACTION_TYPES, action.type || 'motor_direct');
    const fieldsDiv = el('div');
    const refs      = {};

    function render() {
      fieldsDiv.innerHTML = '';
      const t = typeSel.value;

      if (t === 'motor_direct') {
        refs.motorEl = mkNum(0, 3,    action.motor ?? 0,   60);
        refs.pwmEl   = mkNum(-255, 255, action.pwm  ?? 200, 80);
        fieldsDiv.append(
          frow([el('label',{},'Motor (0–3):'), refs.motorEl, el('label',{},'PWM:'), refs.pwmEl]),
          hint('Motor 0=A · 1=B · 2=C · 3=D'),
          hint('PWM −255 bis 255 · positiv = vorwärts · negativ = rückwärts · 0 = stop')
        );
      } else if (t === 'drive_pair') {
        refs.targSel = mkSel(['gui','other'], action.target || 'gui');
        fieldsDiv.append(
          frow([el('label',{},'Ziel:'), refs.targSel]),
          hint('gui = Haupt-Fahrwerk (Motor A+B) · other = zweites Fahrwerk (Motor C+D)')
        );
      } else if (t === 'servo_set') {
        refs.idxEl = mkNum(0, 2,   action.servo ?? 0,  60);
        refs.angEl = mkNum(0, 180, action.angle ?? 90, 72);
        fieldsDiv.append(
          frow([el('label',{},'Servo (0–2):'), refs.idxEl, el('label',{},'Winkel °:'), refs.angEl]),
          hint('Setzt Servo direkt auf einen festen Winkel (0–180°)')
        );
      } else if (t === 'servo_toggle_band') {
        refs.idxEl   = mkNum(0, 2, action.servo ?? 0, 60);
        refs.bandsEl = el('input',{type:'text', value:(action.bands||[0,90]).join(','), style:'width:140px;'});
        fieldsDiv.append(
          frow([el('label',{},'Servo:'), refs.idxEl, el('label',{},'Positionen:'), refs.bandsEl]),
          hint('Komma-getrennte Winkel, z.B. 0,90,180 — wechselt bei jedem Tastendruck')
        );
      } else if (t === 'servo_nudge') {
        refs.idxEl   = mkNum(0, 2,   action.servo ?? 0,  60);
        refs.deltaEl = mkNum(-90, 90, action.delta ?? 10, 72);
        fieldsDiv.append(
          frow([el('label',{},'Servo:'), refs.idxEl, el('label',{},'Delta °:'), refs.deltaEl]),
          hint('+ = mehr Grad · − = weniger Grad · mit hold: kontinuierlich')
        );
      } else if (t === 'servo_axes') {
        refs.idxEl   = mkNum(0, 2, action.servo ?? 0, 60);
        refs.scaleEl = el('input',{type:'number', step:'0.1', value: String(action.scale ?? 1.0), style:'width:80px;'});
        fieldsDiv.append(
          frow([el('label',{},'Servo:'), refs.idxEl, el('label',{},'Scale:'), refs.scaleEl]),
          hint('Joystick-Achse → Servo-Winkel · Scale 1.0 = voller Bereich (0–180°)')
        );
      } else if (t === 'led_set') {
        refs.startEl = mkNum(0, 999, action.start ?? 0, 60);
        refs.countEl = mkNum(1, 999, action.count ?? 1, 60);
        refs.colorEl = el('input',{type:'color', value: action.color || '#00ff00'});
        fieldsDiv.append(
          frow([el('label',{},'Start:'), refs.startEl, el('label',{},'Anzahl:'), refs.countEl, el('label',{},'Farbe:'), refs.colorEl]),
          hint('LEDs ab Index Start einfärben · Anzahl = wie viele LEDs')
        );
      } else if (t === 'gpio_set') {
        refs.pinEl  = mkNum(0, 39, action.pin ?? 2, 72);
        refs.lvlSel = mkSel(['LOW','HIGH'], action.level ? 'HIGH' : 'LOW');
        fieldsDiv.append(
          frow([el('label',{},'Pin:'), refs.pinEl, el('label',{},'Level:'), refs.lvlSel])
        );
      } else if (t === 'speed_adjust') {
        refs.deltaEl = el('input',{type:'number', step:'0.1', value: String(action.delta ?? 0.1), style:'width:88px;'});
        fieldsDiv.append(
          frow([el('label',{},'Delta:'), refs.deltaEl]),
          hint('Globalen Geschwindigkeitsmultiplikator ändern · 0.1 = +10% · −0.1 = −10%')
        );
      }
    }

    typeSel.addEventListener('change', render);
    render();
    root.append(frow([el('label',{},'Action:'), typeSel]), fieldsDiv);

    return {
      root,
      collect() {
        const a = {type: typeSel.value};
        switch (a.type) {
          case 'motor_direct':
            a.motor = parseInt(refs.motorEl.value || '0');
            a.pwm   = parseInt(refs.pwmEl.value   || '0');
            break;
          case 'drive_pair':
            a.target = refs.targSel.value;
            break;
          case 'servo_set':
            a.servo = parseInt(refs.idxEl.value || '0');
            a.angle = parseInt(refs.angEl.value || '0');
            break;
          case 'servo_toggle_band':
            a.servo = parseInt(refs.idxEl.value || '0');
            a.bands = (refs.bandsEl.value || '0,90').split(',').map(s => parseInt(s.trim())).filter(n => !isNaN(n));
            break;
          case 'servo_nudge':
            a.servo = parseInt(refs.idxEl.value   || '0');
            a.delta = parseInt(refs.deltaEl.value || '0');
            break;
          case 'servo_axes':
            a.servo = parseInt(refs.idxEl.value   || '0');
            a.scale = parseFloat(refs.scaleEl.value || '1');
            break;
          case 'led_set':
            a.start = parseInt(refs.startEl.value || '0');
            a.count = parseInt(refs.countEl.value || '1');
            a.color = refs.colorEl.value || '#000000';
            break;
          case 'gpio_set':
            a.pin   = parseInt(refs.pinEl.value || '0');
            a.level = refs.lvlSel.value === 'HIGH' ? 1 : 0;
            break;
          case 'speed_adjust':
            a.delta = parseFloat(refs.deltaEl.value || '0');
            break;
        }
        return a;
      }
    };
  }

  // ── Input editor (type locked by controller key) ──────────
  function makeInputEditor(input, key) {
    const root = el('div');
    const refs = {};

    if (key && key.startsWith('dpad_')) {
      refs.edgeSel = mkSel(EDGES, input.edge || 'hold');
      root.append(
        frow([el('label',{},'Auslöser:'), refs.edgeSel]),
        hint('hold = läuft solange gehalten (für Motoren empfohlen) · press = einmalig beim Drücken')
      );
    } else if (key && key.startsWith('button_')) {
      refs.edgeSel = mkSel(EDGES, input.edge || 'press');
      root.append(
        frow([el('label',{},'Auslöser:'), refs.edgeSel]),
        hint('hold = läuft solange gehalten (für Motoren empfohlen) · press = einmalig beim Drücken')
      );
    } else if (key === 'axis_LStick' || key === 'axis_RStick') {
      refs.deadEl = mkNum(0, 512, input.deadband ?? 16, 72);
      refs.invX   = el('input',{type:'checkbox'}); refs.invX.checked = input.invertX || false;
      refs.invY   = el('input',{type:'checkbox'}); refs.invY.checked = input.invertY || false;
      root.append(
        frow([el('label',{},'Deadband:'), refs.deadEl]),
        hint('Totzone um Null (0–512) · verhindert Drift bei losgelassenem Stick'),
        frow([el('label',{},'Inv X:'), refs.invX, el('label',{},'Inv Y:'), refs.invY]),
        hint('Achse invertieren — nützlich wenn Richtung falsch ist')
      );
    } else {
      // Freie Bearbeitung (z.B. via "+ Neu" in der Toolbar)
      const typeSel = mkSel(INPUT_TYPES, input.type || 'button');
      const subDiv  = el('div');
      refs.typeSel  = typeSel;

      function renderSub() {
        subDiv.innerHTML = '';
        const t = typeSel.value;
        if (t === 'button') {
          refs.codeSel = mkSel(BUTTON_CODES, input.code || 'BTN_A');
          refs.edgeSel = mkSel(EDGES, input.edge || 'press');
          subDiv.append(
            frow([el('label',{},'Taste:'), refs.codeSel, el('label',{},'Auslöser:'), refs.edgeSel]),
            hint('hold = dauerhaft · press = einmalig')
          );
        } else if (t === 'dpad') {
          refs.dirSel  = mkSel(DPAD_DIRS, input.dir || 'UP');
          refs.edgeSel = mkSel(EDGES, input.edge || 'hold');
          subDiv.append(
            frow([el('label',{},'Richtung:'), refs.dirSel, el('label',{},'Auslöser:'), refs.edgeSel]),
            hint('hold = dauerhaft solange gedrückt (für Motor-Steuerung empfohlen)')
          );
        } else {
          refs.xSel   = mkSel(AXES, input.x || 'X');
          refs.ySel   = mkSel(AXES, input.y || 'Y');
          refs.deadEl = mkNum(0, 512, input.deadband ?? 16, 72);
          refs.invX   = el('input',{type:'checkbox'}); refs.invX.checked = input.invertX || false;
          refs.invY   = el('input',{type:'checkbox'}); refs.invY.checked = input.invertY || false;
          subDiv.append(
            frow([el('label',{},'X-Achse:'), refs.xSel, el('label',{},'Y-Achse:'), refs.ySel]),
            frow([el('label',{},'Deadband:'), refs.deadEl, el('label',{},'Inv X:'), refs.invX, el('label',{},'Inv Y:'), refs.invY]),
            hint('L-Stick = X/Y · R-Stick = RX/RY')
          );
        }
      }
      typeSel.addEventListener('change', renderSub);
      renderSub();
      root.append(frow([el('label',{},'Input-Typ:'), typeSel]), subDiv);
    }

    return {
      root,
      collect() {
        const i = {};
        if (key && key.startsWith('dpad_')) {
          i.type = 'dpad'; i.dir = key.split('_')[1]; i.edge = refs.edgeSel.value;
        } else if (key && key.startsWith('button_')) {
          i.type = 'button'; i.code = key.substring(7); i.edge = refs.edgeSel.value;
        } else if (key === 'axis_LStick') {
          i.type='axis_pair'; i.x='X';  i.y='Y';
          i.deadband=parseInt(refs.deadEl.value||'16'); i.invertX=refs.invX.checked; i.invertY=refs.invY.checked;
        } else if (key === 'axis_RStick') {
          i.type='axis_pair'; i.x='RX'; i.y='RY';
          i.deadband=parseInt(refs.deadEl.value||'16'); i.invertX=refs.invX.checked; i.invertY=refs.invY.checked;
        } else {
          i.type = refs.typeSel.value;
          if (i.type === 'button')    { i.code=refs.codeSel.value; i.edge=refs.edgeSel.value; }
          if (i.type === 'dpad')      { i.dir=refs.dirSel.value;   i.edge=refs.edgeSel.value; }
          if (i.type === 'axis_pair') {
            i.x=refs.xSel.value; i.y=refs.ySel.value;
            i.deadband=parseInt(refs.deadEl.value||'16'); i.invertX=refs.invX.checked; i.invertY=refs.invY.checked;
          }
        }
        return i;
      }
    };
  }

  // ── Panel ─────────────────────────────────────────────────
  const panelEl = document.getElementById('bindingPanel');

  function syncPanelToModel() {
    Object.entries(panelEditors).forEach(([idStr, eds]) => {
      const id = parseInt(idStr);
      const b  = bindingModel.find(x => x.id === id);
      if (b) { b.input = eds.collectInput(); b.action = eds.collectAction(); }
    });
    Object.keys(panelEditors).forEach(k => delete panelEditors[k]);
  }

  function renderPanel(key) {
    syncPanelToModel();
    selectedKey = key;
    updateControllerColors();
    panelEl.innerHTML = '';
    panelEl.appendChild(el('h3',{style:'margin-bottom:10px;'},[keyLabel(key)]));

    const filtered = bindingModel.filter(b => keyFromBinding(b) === key);

    if (filtered.length === 0) {
      panelEl.appendChild(el('p',{class:'panel-hint'},['Noch kein Binding für dieses Element. Unten hinzufügen.']));
    }

    filtered.forEach(b => {
      const entry = el('div',{class:'panel-entry'});
      const rmBtn = el('button',{class:'control-button rm-btn'},['× Entfernen']);

      const ie = makeInputEditor({...b.input}, key);
      const ae = makeActionEditor({...b.action});
      panelEditors[b.id] = {collectInput: ie.collect, collectAction: ae.collect};

      rmBtn.addEventListener('click', () => {
        syncPanelToModel();
        bindingModel = bindingModel.filter(x => x.id !== b.id);
        renderPanel(key);
        renderList();
        updateControllerColors();
      });

      entry.append(rmBtn, ie.root, el('hr',{class:'sep'}), ae.root);
      panelEl.appendChild(entry);
    });

    const addBtn  = el('button',{class:'control-button'},['+ Binding hinzufügen']);
    const saveBtn = el('button',{class:'control-button'},['💾 Speichern']);

    addBtn.addEventListener('click', () => {
      syncPanelToModel();
      bindingModel.push({id:nextId++, input:defaultInputForKey(key), action:defaultActionForKey(key)});
      renderPanel(key);
      renderList();
      updateControllerColors();
    });
    saveBtn.addEventListener('click', save);

    panelEl.appendChild(el('div',{class:'panel-actions'},[addBtn, saveBtn]));
  }

  // ── Controller colours ────────────────────────────────────
  function updateControllerColors() {
    document.querySelectorAll('.ctrl-btn[data-key]').forEach(btn => {
      const key   = btn.dataset.key;
      const count = bindingModel.filter(b => keyFromBinding(b) === key).length;
      btn.classList.remove('s-none','s-one','s-multi','s-sel');
      if (key === selectedKey) btn.classList.add('s-sel');
      btn.classList.add(count === 0 ? 's-none' : count === 1 ? 's-one' : 's-multi');
    });
  }

  // ── Bottom list ───────────────────────────────────────────
  const listEl = document.getElementById('bindingsList');

  function renderList() {
    listEl.innerHTML = '';
    if (bindingModel.length === 0) {
      listEl.appendChild(el('p',{style:'color:#999; font-style:italic;'},
        ['Keine Bindings. Oben ein Controller-Element auswählen oder "+ Neu" klicken.']));
      return;
    }
    bindingModel.forEach(b => {
      const row     = el('div',{class:'list-row'});
      const inpTag  = el('span',{class:'tag'},[inputSummary(b.input)]);
      const actTag  = el('span',{class:'tag'},[actionSummary(b.action)]);
      const editBtn = el('button',{class:'control-button'},['Bearbeiten']);
      const delBtn  = el('button',{class:'control-button'},['×']);

      editBtn.addEventListener('click', () => {
        syncPanelToModel();
        const key = keyFromBinding(b);
        if (key && key !== 'axis_other') {
          renderPanel(key);
          document.querySelector('.editor-main').scrollIntoView({behavior:'smooth'});
        }
      });
      delBtn.addEventListener('click', () => {
        syncPanelToModel();
        bindingModel = bindingModel.filter(x => x.id !== b.id);
        renderList();
        updateControllerColors();
        if (selectedKey) renderPanel(selectedKey);
      });

      row.append(inpTag, ' → ', actTag, editBtn, delBtn);
      listEl.appendChild(row);
    });
  }

  // ── Load / Save ───────────────────────────────────────────
  async function load() {
    st.textContent = 'Laden…';
    try {
      const r     = await fetch('/getBindings');
      const binds = await r.json();
      bindingModel = (binds || []).map(b => ({
        id: nextId++,
        input:  {...b.input},
        action: {...b.action}
      }));
      selectedKey = null;
      panelEl.innerHTML = '<p class="panel-hint">← Controller-Element auswählen um Bindings zu bearbeiten</p>';
      Object.keys(panelEditors).forEach(k => delete panelEditors[k]);
      renderList();
      updateControllerColors();
      st.textContent = 'Geladen';
    } catch (e) {
      st.textContent = 'Ladefehler';
    }
  }

  async function save() {
    syncPanelToModel();
    const payload = bindingModel.map(b => ({input: b.input, action: b.action}));
    st.textContent = 'Speichern…';
    try {
      const r = await fetch('/control_bindings', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
      });
      st.textContent = r.ok ? 'Gespeichert ✓' : 'Fehler beim Speichern';
    } catch (e) {
      st.textContent = 'Fehler beim Speichern';
    }
  }

  async function loadDefaults() {
    st.textContent = 'Laden…';
    try {
      const r   = await fetch('/getConfig');
      const cfg = await r.json();
      bindingModel = (cfg.control_bindings || []).map(b => ({
        id: nextId++,
        input:  {...b.input},
        action: {...b.action}
      }));
      selectedKey = null;
      panelEl.innerHTML = '<p class="panel-hint">← Controller-Element auswählen um Bindings zu bearbeiten</p>';
      Object.keys(panelEditors).forEach(k => delete panelEditors[k]);
      renderList();
      updateControllerColors();
      st.textContent = 'Standards geladen';
    } catch (e) {
      st.textContent = 'Fehler';
    }
  }

  // ── Examples ──────────────────────────────────────────────
  window.applyExample = function (n) {
    const ex = EXAMPLES[n];
    if (!ex) return;
    syncPanelToModel();
    const newB = {id:nextId++, input:{...ex.input}, action:{...ex.action}};
    bindingModel.push(newB);
    renderList();
    updateControllerColors();
    const key = keyFromBinding(newB);
    if (key && key !== 'axis_other') renderPanel(key);
    document.querySelector('.editor-main').scrollIntoView({behavior:'smooth'});
    st.textContent = 'Beispiel hinzugefügt';
  };

  // ── Toolbar ───────────────────────────────────────────────
  document.getElementById('addBinding').addEventListener('click', () => {
    syncPanelToModel();
    bindingModel.push({
      id: nextId++,
      input:  {type:'button', code:'BTN_A', edge:'press'},
      action: {type:'motor_direct', motor:0, pwm:200}
    });
    renderList();
    updateControllerColors();
    st.textContent = 'Binding hinzugefügt';
  });
  document.getElementById('load').addEventListener('click', load);
  document.getElementById('save').addEventListener('click', save);
  document.getElementById('defaults').addEventListener('click', loadDefaults);

  // ── Controller click handlers ─────────────────────────────
  document.querySelectorAll('.ctrl-btn[data-key]').forEach(btn => {
    btn.addEventListener('click', () => renderPanel(btn.dataset.key));
  });

  // ── Device name ───────────────────────────────────────────
  function applyDeviceName(cfg) {
    const name = (cfg.hotspot_ssid || cfg.wifi_ssid || 'TinkerThinker').trim();
    const e = document.getElementById('deviceName');
    if (e) e.textContent = name;
    document.title = name + ' Steuerungs-Editor';
  }

  fetch('/getConfig')
    .then(r => r.json())
    .then(applyDeviceName)
    .catch(() => {});

  // ── Init ──────────────────────────────────────────────────
  load();
})();
