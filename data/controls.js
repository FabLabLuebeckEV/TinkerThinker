(() => {
  const list = document.getElementById('bindingsList');
  const st = document.getElementById('status');

  const INPUT_TYPES = ['button','dpad','axis_pair'];
  const BUTTON_CODES = ['BTN_A','BTN_B','BTN_X','BTN_Y','BUTTON_L1','BUTTON_R1','BUTTON_L2','BUTTON_R2','BUTTON_STICK_L','BUTTON_STICK_R'];
  const DPAD_DIRS = ['UP','DOWN','LEFT','RIGHT'];
  const EDGES = ['press','release','hold'];
  const AXES = ['X','Y','RX','RY'];
  const ACTION_TYPES = ['drive_pair','servo_set','servo_toggle_band','servo_nudge','led_set','gpio_set','speed_adjust','servo_axes'];

  function el(tag, attrs={}, children=[]) {
    const e = document.createElement(tag);
    Object.entries(attrs).forEach(([k,v]) => {
      if (k === 'class') e.className = v; else if (k==='style') e.setAttribute('style', v); else e[k]=v;
    });
    children.forEach(c => e.appendChild(c));
    return e;
  }

  function optionList(sel, arr, val) {
    sel.innerHTML='';
    arr.forEach(x => {
      const o = document.createElement('option'); o.value = x; o.textContent = x; if (x===val) o.selected = true; sel.appendChild(o);
    });
  }

  function makeInputEditor(input) {
    const row = el('div', {class:'row', style:'display:flex; gap:8px; flex-wrap:wrap;'});
    const typeSel = el('select'); optionList(typeSel, INPUT_TYPES, input.type||'button');
    const holder = el('div', {style:'display:flex; gap:8px; flex-wrap:wrap;'});

    function renderFields() {
      holder.innerHTML='';
      const t = typeSel.value;
      if (t==='button') {
        const codeSel = el('select'); optionList(codeSel, BUTTON_CODES, input.code||'BTN_A');
        const edgeSel = el('select'); optionList(edgeSel, EDGES, input.edge||'press');
        holder.append('Code:', codeSel, 'Edge:', edgeSel);
        input.codeSel = codeSel; input.edgeSel = edgeSel;
      } else if (t==='dpad') {
        const dirSel = el('select'); optionList(dirSel, DPAD_DIRS, input.dir||'RIGHT');
        const edgeSel = el('select'); optionList(edgeSel, EDGES, input.edge||'press');
        holder.append('Richtung:', dirSel, 'Edge:', edgeSel);
        input.dirSel = dirSel; input.edgeSel = edgeSel;
      } else if (t==='axis_pair') {
        const xSel = el('select'); optionList(xSel, AXES, input.x||'RX');
        const ySel = el('select'); optionList(ySel, AXES, input.y||'RY');
        const dead = el('input', {type:'number', value: input.deadband??16, min:0, max:512, style:'width:100px;'});
        holder.append('X:', xSel, 'Y:', ySel, 'Deadband:', dead);
        input.xSel=xSel; input.ySel=ySel; input.deadEl=dead;
      }
    }
    typeSel.addEventListener('change', renderFields);
    renderFields();
    row.append('Input:', typeSel, holder);
    return {root: row, typeSel, input};
  }

  function makeActionEditor(action) {
    const row = el('div', {class:'row', style:'display:flex; gap:8px; flex-wrap:wrap;'});
    const typeSel = el('select'); optionList(typeSel, ACTION_TYPES, action.type||'drive_pair');
    const holder = el('div', {style:'display:flex; gap:8px; flex-wrap:wrap;'});

    function render() {
      holder.innerHTML='';
      const t = typeSel.value;
      if (t==='drive_pair') {
        const targ = el('select'); optionList(targ, ['gui','other'], action.target||'gui');
        holder.append('Ziel:', targ); action.targetSel=targ;
      } else if (t==='servo_set') {
        const idx = el('input',{type:'number', min:0, max:2, value: action.servo??0, style:'width:80px;'});
        const ang = el('input',{type:'number', min:0, max:180, value: action.angle??90, style:'width:80px;'});
        holder.append('Servo:', idx, 'Winkel:', ang); action.idxEl=idx; action.angEl=ang;
      } else if (t==='servo_toggle_band') {
        const idx = el('input',{type:'number', min:0, max:2, value: action.servo??0, style:'width:80px;'});
        const bands = el('input',{type:'text', value: (action.bands||[0,90]).join(','), style:'width:160px;'});
        holder.append('Servo:', idx, 'Bänder (z.B. 0,90,180):', bands); action.idxEl=idx; action.bandsEl=bands;
      } else if (t==='servo_nudge') {
        const idx = el('input',{type:'number', min:0, max:2, value: action.servo??0, style:'width:80px;'});
        const delta = el('input',{type:'number', min:-90, max:90, value: action.delta??10, style:'width:80px;'});
        holder.append('Servo:', idx, 'Delta:', delta); action.idxEl=idx; action.deltaEl=delta;
      } else if (t==='led_set') {
        const start = el('input',{type:'number', min:0, value: action.start??0, style:'width:80px;'});
        const count = el('input',{type:'number', min:1, value: action.count??1, style:'width:80px;'});
        const color = el('input',{type:'color', value: action.color||'#00ff00'});
        holder.append('Start:', start, 'Anzahl:', count, 'Farbe:', color); action.startEl=start; action.countEl=count; action.colorEl=color;
      } else if (t==='gpio_set') {
        const pin = el('input',{type:'number', min:0, value: action.pin??2, style:'width:100px;'});
        const level = el('select'); optionList(level, ['LOW','HIGH'], (action.level? 'HIGH':'LOW'));
        holder.append('Pin:', pin, 'Level:', level); action.pinEl=pin; action.levelSel=level;
      } else if (t==='speed_adjust') {
        const d = el('input',{type:'number', step:'0.1', value: action.delta??0.1, style:'width:120px;'});
        holder.append('Delta:', d); action.deltaEl=d;
      } else if (t==='servo_axes') {
        const idx = el('input',{type:'number', min:0, max:2, value: action.servo??0, style:'width:80px;'});
        const sc = el('input',{type:'number', step:'0.1', value: action.scale??1.0, style:'width:100px;'});
        holder.append('Servo:', idx, 'Scale:', sc); action.idxEl=idx; action.scaleEl=sc;
      }
    }
    typeSel.addEventListener('change', render);
    render();
    row.append('Action:', typeSel, holder);
    return {root: row, typeSel, action};
  }

  function createBindingView(binding) {
    const card = el('div', {class:'status', style:'padding:12px;'});
    const del = el('button', {class:'control-button', style:'padding:4px 8px; float:right;'}, [document.createTextNode('Entfernen')]);
    const header = el('h3', {}, [document.createTextNode('Binding')]);
    const IE = makeInputEditor(binding.input||{type:'button'});
    const AE = makeActionEditor(binding.action||{type:'drive_pair'});
    del.addEventListener('click', () => card.remove());
    card.append(del, header, IE.root, AE.root);
    card._collect = () => {
      const input = { type: IE.typeSel.value };
      if (input.type==='button') { input.code = IE.input.codeSel.value; input.edge = IE.input.edgeSel.value; }
      if (input.type==='dpad')   { input.dir  = IE.input.dirSel.value;  input.edge = IE.input.edgeSel.value; }
      if (input.type==='axis_pair') { input.x = IE.input.xSel.value; input.y = IE.input.ySel.value; input.deadband = parseInt(IE.input.deadEl.value||'16'); }

      const action = { type: AE.typeSel.value };
      switch (action.type) {
        case 'drive_pair': action.target = AE.action.targetSel.value; break;
        case 'servo_set': action.servo = parseInt(AE.action.idxEl.value||'0'); action.angle = parseInt(AE.action.angEl.value||'0'); break;
        case 'servo_toggle_band': action.servo = parseInt(AE.action.idxEl.value||'0'); action.bands = (AE.action.bandsEl.value||'0,90').split(',').map(s=>parseInt(s.trim())).filter(n=>!isNaN(n)); break;
        case 'servo_nudge': action.servo = parseInt(AE.action.idxEl.value||'0'); action.delta = parseInt(AE.action.deltaEl.value||'0'); break;
        case 'led_set': action.start = parseInt(AE.action.startEl.value||'0'); action.count = parseInt(AE.action.countEl.value||'1'); action.color = AE.action.colorEl.value || '#000000'; break;
        case 'gpio_set': action.pin = parseInt(AE.action.pinEl.value||'-1'); action.level = (AE.action.levelSel.value==='HIGH') ? 1 : 0; break;
        case 'speed_adjust': action.delta = parseFloat(AE.action.deltaEl.value||'0'); break;
        case 'servo_axes': action.servo = parseInt(AE.action.idxEl.value||'0'); action.scale = parseFloat(AE.action.scaleEl.value||'1.0'); break;
      }
      return { input, action };
    };
    return card;
  }

  async function load() {
    st.textContent = 'Laden...';
    list.innerHTML='';
    const r = await fetch('/getBindings');
    const binds = await r.json();
    (binds||[]).forEach(b => list.appendChild(createBindingView(b)));
    st.textContent = 'Geladen';
  }

  async function save() {
    const payload = [];
    [...list.children].forEach(card => payload.push(card._collect()));
    st.textContent = 'Speichern...';
    const r = await fetch('/control_bindings', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload)});
    st.textContent = r.ok ? 'Gespeichert' : 'Fehler beim Speichern';
  }

  async function defaults() {
    st.textContent = 'Laden...';
    const r = await fetch('/getConfig');
    const cfg = await r.json();
    list.innerHTML='';
    (cfg.control_bindings||[]).forEach(b => list.appendChild(createBindingView(b)));
    st.textContent = 'Standards geladen';
  }

  document.getElementById('addBinding').addEventListener('click', () => list.appendChild(createBindingView({input:{type:'button',code:'BTN_A',edge:'press'}, action:{type:'led_set',start:0,count:1,color:'#00ff00'}})) );
  document.getElementById('load').addEventListener('click', load);
  document.getElementById('save').addEventListener('click', save);
  document.getElementById('defaults').addEventListener('click', defaults);

  load();
})();
