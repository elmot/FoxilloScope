    const vltg = {};
    let uplot = null, paused = false, staleTimer = null;


    function resetTransport() { document.querySelectorAll('.transport-btn').forEach(b => b.classList.remove('active')); }
    function activateTransport(mode) { resetTransport(); const b = document.querySelector(`.transport-btn[data-mode="${mode}"]`); if (b) b.classList.add('active'); }

    const comm = {
    mode: 'wifi', _transport: null,
    async switchTo(mode) {
    this.close();
    if (mode === 'none') {
    this.mode = 'none'; this._transport = null;
    resetTransport(); setStatus("Disconnected"); return;
}
    this.mode = mode;
    const t = transports[mode]; if (!t) return;
    activateTransport(mode);
    this._transport = t; t._intentionalClose = false;
    const self = this;
    try {
    await t.connect(d => onFrame(d), () => {
    appendStatus("Disconnected.");
    resetTransport();
    self.mode = 'none';
    self._transport = null;
});
    setStatus("Connected"); param.sendAllCommands();
} catch (e) {
    setStatus("Error: " + e.message, true);
    self.mode = 'none'; self._transport = null; resetTransport();
}
},
    close() { if (this._transport) { this._transport._intentionalClose = true; this._transport.disconnect(); this._transport = null; } },
    send(data) { if (this._transport) this._transport.send(data); },
};
    let frame = { samplesA: [], samplesB: [] };
    let keyFrame = { samplesA: [], samplesB: [] };

    const param = {
    _values: {}, _controls: {}, _debounces: {},
    register(n, el, update, dbMs, m) { this._controls[n] = { el, update, methods: m }; if (dbMs) this._debounces[n] = this._mkDb(dbMs); },
    _mkDb(ms) { let t; return (n, v) => { clearTimeout(t); t = setTimeout(() => this._send(n, v), ms); }; },
    _setNow(n, v, o) { const f = parseFloat(v); this._values[n] = isFinite(f) ? f : v; const c = this._controls[n]; if (c) c.update(c.el, v); if (o?.send !== false) { const d = this._debounces[n]; d ? d(n, v) : this._send(n, v); } this._saveCookie(); },
    set(n, v, o) { this._setNow(n, v, o); },
    get(n, fb) { const v = this._values[n]; return v !== undefined ? v : (fb !== undefined ? fb : 0); },
    _fmt(n, v) { const c = this._controls[n]; return c?.methods?.send ? c.methods.send(v) : String(v); },
    _send(n, v) { comm.send(`${n}=${this._fmt(n, v)}\n`); },
    sendAllCommands() { let d = ""; for (const k in this._controls) d += `${k}=${this._fmt(k, this._values[k])}\n`; comm.send(d); },
    _saveCookie() { document.cookie = "scope_params=" + encodeURIComponent(JSON.stringify(this._values)) + ";path=/;SameSite=Lax"; },
    _loadCookie() { const s = _readCookie('scope_params'); if (s) for (const k in s) { if (this._controls[k]) this.set(k, s[k], {send: false}); } }
};

    function _readCookie(name) { const m = document.cookie.match(new RegExp('(^| )' + name + '=([^;]+)')); if (m) { try { return JSON.parse(decodeURIComponent(m[2])); } catch(e) {} } return null; }

    document.querySelectorAll('.param-slider').forEach(slider => {
    slider.oninput = () => { param.set(slider.id, slider.value); if (uplot) uplot.redraw(); };
    param._values[slider.id] = slider.value;
    param.register(slider.id, slider, (el, v) => el.value = String(v), 20);
});

    (function() { const c = param._controls['trg.level']; if (c) c.methods = { send(v) { const r = parseFloat(v) || 0, tc = param.get('trg.chan'), lc = tc === 0 ? 'a' : 'b'; return String(Math.max(-1e6, Math.min(1e6, Math.round(r / (swState[lc].zoom || 1) + (swState[lc].offsetPpm || 0))))); } }; })();

    document.querySelectorAll(".button-switch-block").forEach(block =>{
    const id = block.id;
    const buttons = block.querySelectorAll("button");
    buttons.forEach(button => {
    button.onclick = () => param.set(id, button.dataset.val);
    if (button.classList.contains("active")) param._values[id] = button.dataset.val;
});
    param.register(id, buttons, (els, v) => {
    els.forEach(b => b.classList.toggle('active', b.dataset.val === String(v)));
});
});

    const selectParam = { wire(id, tw, fw) {
    tw = tw || (v=>v); fw = fw || (v=>v);
    const el = document.getElementById(id);
    param.register(id, el, (el, v) => { const sv = fw(v); for (const o of el.options) { if (Number(o.value) === Number(sv)) { el.value = o.value; return; } } });
    const btns = el.parentElement.getElementsByTagName("button"), step = (d) => { const i = Math.max(0, Math.min(el.options.length - 1, el.selectedIndex + d)); el.value = el.options[i].value; param.set(id, tw(el.value)); };
    btns[0].onclick = () => step(-1); btns[1].onclick = () => step(1);
    el.onchange = () => param.set(id, tw(el.value));
    param._values[id] = el.options[Math.max(0, Math.min(el.options.length - 1, el.selectedIndex))].value;
}};
    selectParam.wire("sampling.ns", v => v / 4, v => v * 4);
    param._loadCookie();

    const HW_GAINS = [63, 31, 15, 7, 3, 1];
    const rawUvRange = { minA: -1650000, maxA: 1650000, minB: -1650000, maxB: 1650000 };
    const swState = { a: { zoom: 1, offset: 0 }, b: { zoom: 1, offset: 0 } };

    const _LOG_MAX = Math.log(504);

    function posToGain(pos) { return Math.round(Math.exp(pos / 1000 * _LOG_MAX) * 100) / 100; }
    function gainToPos(gain) { return Math.round(Math.log(Math.max(1, gain)) / _LOG_MAX * 1000); }
    function splitGain(total) { for (const hw of HW_GAINS) { if (hw <= total) { const sw = total / hw; if (sw <= 8) return { hw, sw: Math.round(sw * 100) / 100 }; } } return { hw: 63, sw: Math.round(total / 63 * 100) / 100 }; }

    function updateDisplayRange() { for (const ch of ['A','B']) { const lc = ch.toLowerCase(), z = swState[lc].zoom || 1, o = swState[lc].offset || 0, mn = rawUvRange['min'+ch], mx = rawUvRange['max'+ch], mid = (mn+mx)/2+o, hf = (mx-mn)/2/z; vltg['minUv'+ch] = mid-hf; vltg['maxUv'+ch] = mid+hf; } }

    const _track = { gainPos: {}, biasTotal: {} };

    function _updateTriggerIfActive(ch) { if ((ch === 'a' && param.get('trg.chan') === 0) || (ch === 'b' && param.get('trg.chan') === 1)) param.set('trg.level', param.get('trg.level')); }
    function _finalizeChannel() { _trackSaveCookie(); updateDisplayRange(); if (uplot) uplot.redraw(); }

    function setupGainSlider(ch) {
    const el = document.getElementById('gain.'+ch), valEl = document.getElementById('gain.'+ch+'.val'), detEl = document.getElementById('gain.'+ch+'.detail');
    function apply(p) { const g = posToGain(p), {hw, sw} = splitGain(g); swState[ch].zoom = sw; _track.gainPos[ch] = p; param.set('gain.'+ch, hw); valEl.textContent = '\u00d7'+g.toFixed(2); detEl.textContent = hw+'+'+sw.toFixed(1); _updateTriggerIfActive(ch); _finalizeChannel(); }
    el.oninput = function() { apply(parseFloat(this.value)); }; el.apply = apply;
}
    function setupBiasSlider(ch) {
    const el = document.getElementById('vbias.'+ch), uc = ch.toUpperCase();
    function apply(t) { const hw = Math.trunc(t), r = t-hw, rg = (rawUvRange['max'+uc]||1650000)-(rawUvRange['min'+uc]||-1650000); swState[ch].offset = r*rg/2e6; swState[ch].offsetPpm = r; _track.biasTotal[ch] = t; param.set('vbias.'+ch, hw); _updateTriggerIfActive(ch); _finalizeChannel(); }
    el.oninput = function() { apply(parseFloat(this.value)); }; el.apply = apply;
}

    ['gain.a','gain.b','vbias.a','vbias.b'].forEach(k => { param.register(k, null, ()=>{}, 20); param._values[k] = k.startsWith('vbias') ? 0 : 1; });
    ['a','b'].forEach(c => { setupGainSlider(c); setupBiasSlider(c); });

    function _trackSaveCookie() { document.cookie = "scope_track=" + encodeURIComponent(JSON.stringify(_track)) + ";path=/;SameSite=Lax"; }
    function _trackLoadCookie() { const s = _readCookie('scope_track'); if (s) { Object.assign(_track, s); if (!_track.gainPos) _track.gainPos = {}; if (!_track.biasTotal) _track.biasTotal = {}; } }

    (function() {
    _trackLoadCookie(); const old = _readCookie('scope_params') || {};
    function restore(type, ch, id, lo, hi, def, po) {
    const el = document.getElementById(id+ch);
    if (_track[type][ch] !== undefined) el.value = String(Math.min(hi, Math.max(lo, _track[type][ch])));
    else { const s = parseFloat(old[id+ch+'.total']); _track[type][ch] = !isNaN(s) ? po(s) : def; el.value = String(Math.min(hi, Math.max(lo, _track[type][ch]))); }
    el.apply(parseFloat(el.value));
}
    restore('gainPos','a','gain.',0,1000,gainToPos(param.get('gain.a',1)),v=>gainToPos(Math.min(504,Math.max(1,v))));
    restore('gainPos','b','gain.',0,1000,gainToPos(param.get('gain.b',1)),v=>gainToPos(Math.min(504,Math.max(1,v))));
    restore('biasTotal','a','vbias.',-1e6,1e6,param.get('vbias.a'),v=>v);
    restore('biasTotal','b','vbias.',-1e6,1e6,param.get('vbias.b'),v=>v);
    _trackSaveCookie();
})();

    function fmtSi(v, s) { if (v===undefined||isNaN(v)) return''; const a=Math.abs(v); for(const[t,m,u,d]of s)if(a>=t)return(v*m).toFixed(d)+u; const[m,u,d]=s.at(-1);return(v*m).toFixed(d)+u; }
    function fmtUv(v) { return fmtSi(v, [[10e6,1e-6,'V',1],[1e6,1e-6,'V',2],[10e3,1e-3,'mV',1],[1e3,1e-3,'mV',2],[10,1,'\u00B5V',1],[1,1,'\u00B5V',2],[1e3,'nV',0]]); }
    function fmtTime(t) { return fmtSi(t, [[10e3,1e-3,'s',0],[1e3,1e-3,'s',1],[10,1,'ms',0],[1,1,'ms',1],[10e-3,1e3,'\u00B5s',0],[1e-3,1e3,'\u00B5s',1],[1e6,'ns',0]]); }
    function adcToUv(adc, mn, mx) { if (adc==undefined) return null; const s=vltg.steps||4096; if(mn!=undefined&&mx!=undefined) return mn+(adc/s)*(mx-mn); return (adc/s)*3300000-1650000; }

    let _drag = false;
    function initUplot() {
    const el = document.getElementById("scope");
    const rect = el.getBoundingClientRect();
    const w = rect.width || 600, h = rect.height || 400;

    function seriesColor(u,i) { const c=i%2===0?COLORS.chB:COLORS.chA; return c+(param._values["trg.type"]==="0"?"A0":"50"); }
    function keySeriesColor(u,i) {
    const b=i%2===0?COLORS.chBKey:COLORS.chAKey, ms=param.get("sampling.ns")*param.frameWidth/1e6, td=isFinite(ms)?Math.max(1e3,ms):1e3;
    return b+Math.max(30,250-Math.ceil(50*(Date.now()-(keyFrame.timestamp||0))/td)).toString(16).padStart(2,"0");
}
    try {
    const grid={show:true,width:1,size:20,stroke:"#333",dash:[3,8]};
    uplot = new uPlot({
    width: w, height: h,
    cursor: { show: true, drag: { x: true, y: true, setScale: false } },
    legend: { show: false }, select: { show: true },
    scales: { chA:{range: (u,mn,mx)=>[vltg.minUvA??-1650000,vltg.maxUvA??1650000]}, chB:{range: (u,mn,mx)=>[vltg.minUvB??-1650000,vltg.maxUvB??1650000]} },
    axes: [
{ scale:"x", stroke:"#FFF", grid, values:(u,s)=>s.map(fmtTime) },
{ scale:"chA", side:3, values:(u,s)=>s.map(fmtUv), stroke:COLORS.chAKey, size:90, grid },
{ scale:"chB", side:1, values:(u,s)=>s.map(fmtUv), stroke:COLORS.chBKey, size:90 },
    ],
    series: [
{},
{ scale:"chA", stroke:seriesColor, width:2, points:{show:false} },
{ scale:"chB", stroke:seriesColor, width:2, points:{show:false} },
{ scale:"chA", stroke:keySeriesColor, width:2, points:{show:false} },
{ scale:"chB", stroke:keySeriesColor, width:2, points:{show:false} }
    ],
    hooks: {
    drawAxes: [(self) => {
    const ctx = self.ctx, l = self.bbox.left, r = l + self.bbox.width, t = self.bbox.top, b = t + self.bbox.height;
    ctx.save(); ctx.setLineDash([4,4]);
    [{a:'chA',c:COLORS.chA},{a:'chB',c:COLORS.chB}].forEach(({a,c})=>{const y=self.valToPos(0,a,true); if(y>-10&&y<self.height+10){ctx.strokeStyle=c;ctx.beginPath();ctx.moveTo(l,y);ctx.lineTo(r,y);ctx.stroke();}});
    ctx.setLineDash([]);
    try {
    const trgPpm = param.get('trg.level');
    if (!isNaN(trgPpm)) {
    const tc = param.get('trg.chan'), sc = tc === 0 ? 'chA':'chB', mn = tc === 0 ? (vltg.minUvA??-1650000):(vltg.minUvB??-1650000), mx = tc === 0 ? (vltg.maxUvA??1650000):(vltg.maxUvB??1650000);
    const y = self.valToPos(mn+((trgPpm+1e6)/2e6)*(mx-mn), sc, true);
    ctx.setLineDash([4,4]); ctx.strokeStyle='#ff69b480'; ctx.lineWidth=1.5; ctx.beginPath(); ctx.moveTo(l,y); ctx.lineTo(r,y); ctx.stroke(); ctx.setLineDash([]);
}
    const off = param.get('trg.time.offset'), xs = self.scales.x, xv = (xs.min??0)+(off/1e6)*((xs.max??1)-(xs.min??0)), xp = self.valToPos(xv,'x',true);
    let pl = l, pr = r;
    for (const a of self.axes) { if (a?.bbox) { if (a.side===1) pl=Math.max(pl,a.bbox.left+a.bbox.width); if (a.side===3) pr=Math.min(pr,a.bbox.left); } }
    if (xp >= pl && xp <= pr && pr - pl > 0) { ctx.save(); ctx.beginPath(); ctx.rect(pl,t,pr-pl,b-t); ctx.clip(); ctx.setLineDash([4,4]); ctx.strokeStyle='#ff69b480'; ctx.lineWidth=1.5; ctx.beginPath(); ctx.moveTo(xp,t); ctx.lineTo(xp,b); ctx.stroke(); ctx.setLineDash([]); ctx.restore(); }
} catch(e) {}
    ctx.restore();
}],
    setCursor: [self => {
    const cx = self.cursor.left, cy = self.cursor.top, ci = document.getElementById('cursor-info');
    if (cx == null || cy == null || !ci) return;
    try {
    const se = self.root.querySelector('.u-select');
    if (se) se.style.display = _drag ? '' : 'none';
    const s = self.select, bx = self.bbox.left, by = self.bbox.top;
    const cA = self.posToVal(cy,'chA'), cB = self.posToVal(cy,'chB');
    ci.textContent = [fmtTime(self.posToVal(cx,'x')), isNaN(cA)?'':'A '+fmtUv(cA), isNaN(cB)?'':'B '+fmtUv(cB)].filter(Boolean).join('  ');

    const sl = document.querySelector('.sel-label'), dt = _drag && s && s.width > 0 && s.height > 0 ? Math.abs(self.posToVal(s.left+bx,'x')-self.posToVal(s.left+s.width+bx,'x')) : null;
    if (sl) {
    sl.style.display = dt ? '' : 'none';
    if (dt) {
    const dA = Math.abs(self.posToVal(s.top+by,'chA')-self.posToVal(s.top+s.height+by,'chA')), dB = Math.abs(self.posToVal(s.top+by,'chB')-self.posToVal(s.top+s.height+by,'chB'));
    sl.textContent = ['\u0394t '+fmtTime(dt), isNaN(dA)?'':'\u0394A '+fmtUv(dA), isNaN(dB)?'':'\u0394B '+fmtUv(dB)].filter(Boolean).join('  ');
    sl.style.left = (s.left + bx + s.width/2) + 'px';
    sl.style.top = (s.top + by + 2) + 'px';
}
}
} catch(e) {}
}],
},
}, [[0], [0], [0], [0], [0]], el);
    const selLabel = document.createElement('div');
    selLabel.className = 'sel-label';
    uplot.root.append(selLabel);
    uplot.over.addEventListener('pointerdown', e => { if (e.button === 0) _drag = true; });
    uplot.over.addEventListener('pointerup', () => _drag = false);
    uplot.over.addEventListener('pointerleave', () => _drag = false);
    let _touchStart = null;
    uplot.over.addEventListener('touchstart', e => {
    e.preventDefault();
    const r = uplot.over.getBoundingClientRect(), t = e.changedTouches[0];
    _touchStart = {x: t.clientX - r.left, y: t.clientY - r.top};
    uplot.setSelect({left: _touchStart.x, top: _touchStart.y, width: 0, height: 0});
    uplot.setCursor({left: _touchStart.x, top: _touchStart.y}, true);
    _drag = true;
}, {passive: false});
    uplot.over.addEventListener('touchmove', e => {
    e.preventDefault();
    const r = uplot.over.getBoundingClientRect(), t = e.changedTouches[0];
    const cx = t.clientX - r.left, cy = t.clientY - r.top;
    if (_touchStart) {
    uplot.setSelect({left: Math.min(_touchStart.x, cx), top: Math.min(_touchStart.y, cy), width: Math.abs(cx - _touchStart.x), height: Math.abs(cy - _touchStart.y)});
}
    uplot.setCursor({left: cx, top: cy}, true);
}, {passive: false});
    uplot.over.addEventListener('touchend', () => {
    _drag = false;
    _touchStart = null;
    uplot.setSelect({left: 0, top: 0, width: 0, height: 0});
});
    const _yAxes = uplot.root.querySelectorAll('.u-axis');
    if (_yAxes.length >= 3) {
    _yAxes[0].id = 'axis-x';
    _yAxes[0].style.touchAction = 'none';
    _yAxes[1].id = 'axis-yA';
    _yAxes[1].style.touchAction = 'none';
    _yAxes[2].id = 'axis-yB';
    _yAxes[2].style.touchAction = 'none';
    const dpr = window.devicePixelRatio || 1;
    let _axisDrag = null;
    _yAxes[0].addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    _yAxes[0].setPointerCapture(e.pointerId);
    _axisDrag = { startX: e.clientX, startOffset: param.get('trg.time.offset', 0), uvPerPx: dpr / uplot.bbox.width };
});
    _yAxes[0].addEventListener('pointermove', (e) => {
    if (!_axisDrag) return;
    const newOffset = Math.max(-1e6, Math.min(1e6, _axisDrag.startOffset + (e.clientX - _axisDrag.startX) * _axisDrag.uvPerPx * 1e6));
    param.set('trg.time.offset', String(Math.round(newOffset)));
    updatePlot();
});
    _yAxes[0].addEventListener('pointerup', () => { _axisDrag = null; });
    _yAxes[0].addEventListener('pointerleave', () => { _axisDrag = null; });
    const _axisYSetup = (axis, lc) => {
    axis.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    axis.setPointerCapture(e.pointerId);
    const uc = lc.toUpperCase();
    const rng = (vltg['maxUv' + uc] || 1650000) - (vltg['minUv' + uc] || -1650000);
    _axisDrag = { startY: e.clientY, startOffset: swState[lc].offset || 0, lc, uvPerPx: rng * dpr / uplot.bbox.height };
});
    axis.addEventListener('pointermove', (e) => {
    if (!_axisDrag) return;
    const deltaUv = (e.clientY - _axisDrag.startY) * _axisDrag.uvPerPx;
    swState[_axisDrag.lc].offset = _axisDrag.startOffset + deltaUv;
    updateDisplayRange();
    uplot.redraw();
});
    axis.addEventListener('pointerup', () => { _axisDrag = null; });
    axis.addEventListener('pointerleave', () => { _axisDrag = null; });
};
    _axisYSetup(_yAxes[1], 'a');
    _axisYSetup(_yAxes[2], 'b');
}
} catch (e) { console.error("uPlot init fail:", e); }
    window.addEventListener("resize", () => {
    const r = el.getBoundingClientRect();
    if (uplot) uplot.setSize({ width: r.width, height: r.height });
});
}

    function updatePlot() {
    const fw = param.frameWidth;
    const pa = a => a.concat(...Array(Math.max(0,fw-a.length)).fill(null));
    const cl = (a,lo,hi) => lo!=undefined&&hi!=undefined ? a.map(v=>v===null?null:Math.max(lo,Math.min(hi,v))) : a;
    if (!uplot) return;
    try {
    const sp = param.get("sampling.ns",500000);
    if (Math.min(frame.samplesA.length,frame.samplesB.length) < 2) return;
    const tn = sp * fw, xs = new Array(fw);
    const [dA,dB,dKA,dKB] = [cl(pa(frame.samplesA),vltg.minUvA,vltg.maxUvA), cl(pa(frame.samplesB),vltg.minUvB,vltg.maxUvB), cl(pa(keyFrame.samplesA),vltg.minUvA,vltg.maxUvA), cl(pa(keyFrame.samplesB),vltg.minUvB,vltg.maxUvB)];
    const tr = (param.get('trg.time.offset')/1e6)*tn/1e6;
    for (let i=0;i<fw;i++) xs[i]=i*tn/fw/1e6-tr;
    uplot.setData([xs,dA,dB,dKA,dKB], true);
} catch(e) { console.error("updatePlot:",e); }
}

    function setStatus(t, e) { const el=document.getElementById("status"); if(el) { el.textContent=t; el.className=e?'error':''; } }
    function appendStatus(t) { const el=document.getElementById("status"); if(el) el.textContent+=" "+t; }

    function onFrame(text) {
    clearTimeout(staleTimer);
    if (text.startsWith("error:")) { setStatus(text.substring(7).trim(), true); return; }
    staleTimer = setTimeout(() => { if (comm._transport) setStatus("communication stalling", true); }, 1000);
    if (paused) return;
    const cmds = {};
    for (const l of text.split("\n")) { const t=l.trim(); if(!t) continue; const i=t.indexOf("="); if(i!==-1) cmds[t.substring(0,i).trim()]=t.substring(i+1).trim(); else if(t==='param?') param.sendAllCommands(); }
    const push = (a,b) => { for (let i=0;i<b.length;i++) a.push(b[i]); };
    let sA = decode(cmds["data.a"]||"").map(v=>adcToUv(v,rawUvRange.minA,rawUvRange.maxA));
    let sB = decode(cmds["data.b"]||"").map(v=>adcToUv(v,rawUvRange.minB,rawUvRange.maxB));
    if (cmds["head"]==="1") frame={samplesA:sA,samplesB:sB}; else { push(frame.samplesA,sA); push(frame.samplesB,sB); }
    if (cmds["keyframe"]==="1") { if (cmds["head"]==="1") { keyFrame.samplesA=sA; keyFrame.samplesB=sB; } else { push(keyFrame.samplesA,sA); push(keyFrame.samplesB,sB); } keyFrame.timestamp=Date.now(); sA=sB=null; }
    if (cmds["vltg.steps"]!==undefined) vltg.steps=parseFloat(cmds["vltg.steps"]);
    if (cmds["vltg.min.uv.a"]!==undefined) rawUvRange.minA=parseFloat(cmds["vltg.min.uv.a"]);
    if (cmds["vltg.max.uv.a"]!==undefined) rawUvRange.maxA=parseFloat(cmds["vltg.max.uv.a"]);
    if (cmds["vltg.min.uv.b"]!==undefined) rawUvRange.minB=parseFloat(cmds["vltg.min.uv.b"]);
    if (cmds["vltg.max.uv.b"]!==undefined) rawUvRange.maxB=parseFloat(cmds["vltg.max.uv.b"]);
    updateDisplayRange(); setStatus("streaming");
    param.frameWidth = parseInt(cmds["frame.size"]) || 200;
    updatePlot();
}

    if (!window.isSecureContext) document.querySelectorAll('.transport-btn[data-mode="ble"],.transport-btn[data-mode="serial"]').forEach(b=>b.classList.add('insecure'));
    document.querySelectorAll('.transport-btn[data-mode]').forEach(btn => {
    btn.addEventListener('click', () => {
        const m = btn.dataset.mode;
        if (!window.isSecureContext && m !== 'wifi') { window.open('https://elmot.xyz/oscilloscope','_blank'); return; }
        comm.switchTo(btn.classList.contains('active') ? 'none' : m);
    });
});
    const fsBtn = document.getElementById('btn-fullscreen');
    if (fsBtn) {
    fsBtn.onclick = () => { if (!document.fullscreenElement) document.documentElement.requestFullscreen?.(); else document.exitFullscreen?.(); };
    document.addEventListener('fullscreenchange', () => fsBtn.classList.toggle('active', !!document.fullscreenElement));
}

    initUplot();
    const host = location.hostname;
    if (host.endsWith('.local') || (host === '127.0.0.1') || !host.includes('.')) {
    comm.switchTo('wifi');}
    else {
    setStatus("Select transport");
}
    document.getElementById("trgShiftReset").onclick = (e) => { const el=document.getElementById("trg.time.offset"); el.value="0"; param.set("trg.time.offset","0"); e.target.blur(); };
    document.getElementById("pauseBtn").onclick = function() { paused=!paused; this.textContent=paused?"Resume":"Pause"; this.classList.toggle("paused",paused); };
