(() => {
  const ta = document.getElementById('bindings');
  const st = document.getElementById('status');

  async function load() {
    st.textContent = 'Loading...';
    try {
      const r = await fetch('/getBindings');
      const t = await r.text();
      // pretty format
      try {
        const obj = JSON.parse(t);
        ta.value = JSON.stringify(obj, null, 2);
      } catch { ta.value = t; }
      st.textContent = 'Loaded';
    } catch (e) {
      st.textContent = 'Load failed';
    }
  }

  async function save() {
    st.textContent = 'Saving...';
    let body = ta.value;
    try { JSON.parse(body); } catch (e) { alert('Invalid JSON'); st.textContent = 'Invalid JSON'; return; }
    const r = await fetch('/control_bindings', { method:'POST', headers:{'Content-Type':'application/json'}, body});
    st.textContent = r.ok ? 'Saved' : 'Save failed';
  }

  async function defaults() {
    st.textContent = 'Loading defaults...';
    try {
      const r = await fetch('/getConfig');
      const cfg = await r.json();
      ta.value = JSON.stringify(cfg.control_bindings || [], null, 2);
      st.textContent = 'Defaults loaded';
    } catch { st.textContent = 'Defaults failed'; }
  }

  document.getElementById('load').addEventListener('click', load);
  document.getElementById('save').addEventListener('click', save);
  document.getElementById('defaults').addEventListener('click', defaults);

  load();
})();

