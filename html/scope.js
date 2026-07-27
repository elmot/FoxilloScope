const _defVslParameters = {
    "trigger.lvl.ppm": 100000,
    "trg.type": 1,
    "trg.chan": 0,
    "trg.time.offset": -200000, //ppm //todo 0
    "sampling.ns": 250,//todo 5000
    "channels": {
        "a": {
            "range.uv": 33000000,//todo 3300000
            "base.lvl.uv": -1000000,//todo 0
        },
        "b": {
            "range.uv": 3300000,
            "base.lvl.uv": 500000,//todo 0
        },
    }
};

let vslParameters = null;

const ParametersStorage = {
    save() {
        localStorage.setItem("vslParameters", JSON.stringify(vslParameters));
    },
    load() {
        vslParameters = JSON.parse(JSON.stringify(_defVslParameters));
        try {
            const saved = JSON.parse(localStorage.getItem("vslParameters"));
            if (saved && typeof saved === 'object') {
                Object.assign(vslParameters, saved);
                if (saved.channels) {
                    vslParameters.channels.a = Object.assign({}, _defVslParameters.channels.a, saved.channels.a);
                    vslParameters.channels.b = Object.assign({}, _defVslParameters.channels.b, saved.channels.b);
                }
            }
        } catch (e) {
            console.log("Error loading parameters from local storage:", e);
        }
    }
}
ParametersStorage.load();

const Hardware = {
    sentParameters: {},
    sendAllParameters() {
        this.sendTimingParameters()
        this.sendChannelParameters("a")
        this.sendChannelParameters("b")
        this.sendTriggerParameters()
    },
    sendChannelParameters(channel) {
        const range = vslParameters.channels[channel]["range.uv"]
        const {hw} = Gain.splitGain(Gain.BASE_VOLTAGE_uV / range)
        const baseLevelUv = vslParameters.channels[channel]["base.lvl.uv"]
        comm.send(`gain.${channel}=${hw}\nbase.lvl.${channel}.uv=${baseLevelUv.toFixed(0)}`)
    },
    _sendParameters(...names) {
        let cmd = ""
        names.forEach(name => cmd +=`${name}=${vslParameters[name]}\n` )
        comm.send(cmd)
    },
    triggerLevelUv() {
        const ch = vslParameters["trg.chan"] === 0 ? "a" : "b";
        const range = vslParameters.channels[ch]["range.uv"]
        const base = vslParameters.channels[ch]["base.lvl.uv"]
        return base + range * vslParameters["trigger.lvl.ppm"] / 2000000
    },
    sendTriggerParameters() {
        this._sendParameters("trg.type", "trg.chan", "trg.time.offset", "trigger.lvl.ppm")
    },
    sendTimingParameters() {
        this._sendParameters("sampling.ns")
    }
}

function onFrame(text) {
    clearTimeout(staleTimer);
    if (text.startsWith("error:")) {
        setStatus(text.substring(7).trim(), true);
        return;
    }
    staleTimer = setTimeout(() => {
        if (comm._transport) setStatus("communication stalling", true);
    }, 1000);
    if (paused) return;
    const cmds = {samples: {}, parameters: {}};
    for (let l of text.split("\n")) {
        if (l.indexOf('param?') >= 0) {
            Hardware.sendAllParameters();
            continue;
        }
        let [key, value] = l.split("=").map(s => s.trim());
        if (value === undefined) continue;
        if (key.startsWith('data.')) {
            cmds.samples[key.substring(5)] = decode(value);
        } else {
            try {
                cmds.parameters[key] = parseFloat(value);
            } catch (e) {
                console.error(e);
            }
        }
    }
    let currentFrame;
    // noinspection JSUnresolvedReference
    if (cmds.parameters.keyframe === 1) {
        keyFrame.timestamp = Date.now();
        currentFrame = keyFrame;
    } else {
        currentFrame = frame;
    }
    currentFrame.parameters = cmds.parameters;
    if (cmds.parameters.head === 1) {
        currentFrame.samples = cmds.samples;
    } else {
        for (const [channel, srcSamples] of Object.entries(cmds.samples)) {
            let dst = currentFrame.samples[channel] || (currentFrame.samples[channel] = []);
            dst.push(...srcSamples);
        }
    }
    setStatus("streaming");
    updatePlot();
}

let uplot = null, paused = false, staleTimer = null;

let frame;
let keyFrame ;

function clearFrames() {
    frame = {samples: {}, parameters: {}};
    keyFrame = {samples: {}, parameters: {}};
}

clearFrames();

document.querySelectorAll(".button-switch-block").forEach(block => {
    const id = block.id;
    const val = String(vslParameters[id]);
    const buttons = block.querySelectorAll("button");
    buttons.forEach(button => {
        button.classList.toggle('active', String(button.dataset.val) === val);
        button.onclick = () => {
            vslParameters[id] = parseFloat(button.dataset.val);
            Hardware.sendTriggerParameters();
            ParametersStorage.save();
            buttons.forEach(otherButton => otherButton.classList.toggle('active', button === otherButton));
        }
    });
});

{
    const trgSlider = document.getElementById('trg.level');
    trgSlider.value = vslParameters["trigger.lvl.ppm"] ?? 0;
    trgSlider.oninput = () => {
        vslParameters["trigger.lvl.ppm"] = parseFloat(trgSlider.value);
        Hardware.sendTriggerParameters();
        ParametersStorage.save();
        updatePlot();
    };
}

const Gain  = {
    HW_GAINS: [64, 32, 16, 8, 4, 2, 1],
    MAX: 504,
    LOG_MAX: Math.ceil(Math.log(504) * 100) / 100,
    BASE_VOLTAGE_uV: 3300000,//todo replace with calibrated value
    splitGain: (total) => {
        for (const hw of Gain.HW_GAINS) {
            if (hw <= total) {
                const sw = total / hw;
                if (sw <= 8) return {hw, sw: Math.round(sw * 100) / 100};
            }
        }
        return {hw: 63, sw: Math.round(total / 63 * 100) / 100};
    },
    readGain(slider) {
        return clampValue(Math.exp(parseFloat(slider.value)), 1, Gain.MAX)
    }
}

for(const chName of ["a","b"]) {
    const slider = document.getElementById('gain.' + chName);
    slider.setAttribute("max", "" + Gain.LOG_MAX);
    slider.setAttribute("min", "0");
    const currentGain = Gain.BASE_VOLTAGE_uV / vslParameters.channels[chName]["range.uv"];
    slider.value = Math.log(clampValue(currentGain, 1, Gain.MAX));
    const updateDetails = () =>{
        // noinspection JSCheckFunctionSignatures
        const gain = Gain.readGain(slider)
        const {hw, sw} = Gain.splitGain(gain);
        document.getElementById('gain.' + chName + '.val').textContent = `x${gain.toFixed(1)}`;
        document.getElementById('gain.' + chName + '.detail').textContent = `${hw}+${sw.toFixed(1)}`;
    }
    updateDetails();
    slider.oninput = () => {
        // noinspection JSCheckFunctionSignatures
        vslParameters.channels[chName]["range.uv"] = Gain.BASE_VOLTAGE_uV / Gain.readGain(slider);
        updateDetails()
        updatePlot()
        Hardware.sendChannelParameters(chName) //todo debouncing
        ParametersStorage.save();
    }
}

function fmtSi(v, s) {
    if (v === undefined || isNaN(v)) return '';
    const a = Math.abs(v);
    for (const [t, m, u, d] of s) if (a >= t) return (v * m).toFixed(d) + u;
    const [m, u, d] = s.at(-1);
    return (v * m).toFixed(d) + u;
}

function fmtUv(v) {
    return fmtSi(v, [[10e6, 1e-6, 'V', 1], [1e6, 1e-6, 'V', 2], [10e3, 1e-3, 'mV', 1], [1e3, 1e-3, 'mV', 2], [10, 1, '\u00B5V', 1], [1, 1, '\u00B5V', 2], [1e3, 'nV', 0]]);
}

function fmtTime(t) { return fmtSi(t, [[10e3,1e-3,'s',0],[1e3,1e-3,'s',1],[10,1,'ms',0],[1,1,'ms',1],[10e-3,1e3,'\u00B5s',0],[1e-3,1e3,'\u00B5s',1],[1e6,'ns',0]]); }

let _drag = false;

function initUplot() {
    const el = document.getElementById("scope");
    const rect = el.getBoundingClientRect();
    const w = rect.width || 600, h = rect.height || 400;

    function seriesColor(u, i) {
        const c = i % 2 === 0 ? COLORS.chB : COLORS.chA;
        return c + (vslParameters["triggerg.type"] === 0 ? "A0" : "50");
    }

    function keySeriesColor(u, i) {
        const b = i % 2 === 0 ? COLORS.chBKey : COLORS.chAKey, ms = vslParameters["sampling.ns"] * keyFrame.parameters["frame.size"] / 1e6,
            td = isFinite(ms) ? Math.max(1e3, ms) : 1e3;
        return b + Math.max(30, 250 - Math.ceil(50 * (Date.now() - (keyFrame.timestamp || 0)) / td)).toString(16).padStart(2, "0");
    }

    try {
        const grid = {show: true, width: 1, size: 20, stroke: "#333", dash: [3, 8]};
        const axisRange = chParams => () => {const [r,b] = [chParams["range.uv"],chParams["base.lvl.uv"]]; return [b - r / 2, b + r / 2]}
        // noinspection JSPotentiallyInvalidConstructorUsage
        uplot = new uPlot({
            width: w, height: h,
            cursor: {show: true, drag: {x: true, y: true, setScale: false}},
            legend: {show: false}, select: {show: true},
            scales: {
                chA: {range: axisRange(vslParameters.channels.a)},
                chB: {range: axisRange(vslParameters.channels.b)}
            },
            axes: [
                {scale: "x", stroke: "#FFF", grid, values: (u, s) => s.map(fmtTime)},
                {scale: "chA", side: 3, values: (u, s) => s.map(fmtUv), stroke: COLORS.chAKey, size: 90, grid},
                {scale: "chB", side: 1, values: (u, s) => s.map(fmtUv), stroke: COLORS.chBKey, size: 90},
            ],
            series: [
                {},
                {scale: "chA", stroke: seriesColor, width: 2, points: {show: false}},
                {scale: "chB", stroke: seriesColor, width: 2, points: {show: false}},
                {scale: "chA", stroke: keySeriesColor, width: 2, points: {show: false}},
                {scale: "chB", stroke: keySeriesColor, width: 2, points: {show: false}}
            ],
            hooks: {
                drawAxes: [(self) => {
                    const ctx = self.ctx, l = self.bbox.left, r = l + self.bbox.width, t = self.bbox.top,
                        b = t + self.bbox.height;
                    ctx.save();
                    ctx.setLineDash([4, 4]);
                    [{a: 'chA', c: COLORS.chA}, {a: 'chB', c: COLORS.chB}].forEach(({a, c}) => {
                        const y = self.valToPos(0, a, true);
                        if (y >= t && y <= b) {
                            ctx.strokeStyle = c;
                            ctx.beginPath();
                            ctx.moveTo(l, y);
                            ctx.lineTo(r, y);
                            ctx.stroke();
                        }
                    });
                    ctx.setLineDash([]);
                    try {
                        if (vslParameters["trg.type"] !== 0) {
                            const trgCh = vslParameters["trg.chan"] === 0 ? "a" : "b";
                            const trgUv = Hardware.triggerLevelUv();
                            const y = self.valToPos(trgUv, trgCh === "a" ? "chA" : "chB", true);
                            ctx.setLineDash([4, 4]);
                            ctx.strokeStyle = '#ff69b480';
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            ctx.moveTo(l, y);
                            ctx.lineTo(r, y);
                            ctx.stroke();
                            ctx.setLineDash([]);
                        const xp = self.valToPos(0, 'x', true);
                        let pl = l, pr = r;
                        for (const a of self.axes) {
                            if (a?.bbox) {
                                if (a.side === 1) pl = Math.max(pl, a.bbox.left + a.bbox.width);
                                if (a.side === 3) pr = Math.min(pr, a.bbox.left);
                            }
                        }
                        if (xp >= pl && xp <= pr && pr - pl > 0) {
                            ctx.save();
                            ctx.beginPath();
                            ctx.rect(pl, t, pr - pl, b - t);
                            ctx.clip();
                            ctx.setLineDash([4, 4]);
                            ctx.strokeStyle = '#ff69b480';
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            ctx.moveTo(xp, t);
                            ctx.lineTo(xp, b);
                            ctx.stroke();
                            ctx.setLineDash([]);
                            ctx.restore();
                        }
                        }
                    } catch (e) {
                    }
                    ctx.restore();
                }],
                setCursor: [self => {
                    const cx = self.cursor.left, cy = self.cursor.top, ci = document.getElementById('cursor-info');
                    if (cx == null || cy == null || !ci) return;
                    try {
                        const se = self.root.querySelector('.u-select');
                        if (se) se.style.display = _drag ? '' : 'none';
                        const s = self.select, bx = self.bbox.left, by = self.bbox.top;
                        const cA = self.posToVal(cy, 'chA'), cB = self.posToVal(cy, 'chB');
                        ci.textContent = [fmtTime(self.posToVal(cx, 'x')), isNaN(cA) ? '' : 'A ' + fmtUv(cA), isNaN(cB) ? '' : 'B ' + fmtUv(cB)].filter(Boolean).join('  ');

                        const sl = document.querySelector('.sel-label'),
                            dt = _drag && s && s.width > 0 && s.height > 0 ? Math.abs(self.posToVal(s.left + bx, 'x') - self.posToVal(s.left + s.width + bx, 'x')) : null;
                        if (sl) {
                            sl.style.display = dt ? '' : 'none';
                            if (dt) {
                                const dA = Math.abs(self.posToVal(s.top + by, 'chA') - self.posToVal(s.top + s.height + by, 'chA')),
                                    dB = Math.abs(self.posToVal(s.top + by, 'chB') - self.posToVal(s.top + s.height + by, 'chB'));
                                sl.textContent = ['\u0394t ' + fmtTime(dt), isNaN(dA) ? '' : '\u0394A ' + fmtUv(dA), isNaN(dB) ? '' : '\u0394B ' + fmtUv(dB)].filter(Boolean).join('  ');
                                sl.style.left = (s.left + bx + s.width / 2) + 'px';
                                sl.style.top = (s.top + by + 2) + 'px';
                            }
                        }
                    } catch (e) {
                    }
                }],
            },
        }, [[0], [0], [0], [0], [0]], el);
        const selLabel = document.createElement('div');
        selLabel.className = 'sel-label';
        uplot.root.append(selLabel);
        uplot.over.addEventListener('pointerdown', e => {
            if (e.button === 0) _drag = true;
        });
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
                uplot.setSelect({
                    left: Math.min(_touchStart.x, cx),
                    top: Math.min(_touchStart.y, cy),
                    width: Math.abs(cx - _touchStart.x),
                    height: Math.abs(cy - _touchStart.y)
                });
            }
            uplot.setCursor({left: cx, top: cy}, true);
        }, {passive: false});
        uplot.over.addEventListener('touchend', () => {
            _drag = false;
            _touchStart = null;
            uplot.setSelect({left: 0, top: 0, width: 0, height: 0});
        });
        const axes = uplot.root.querySelectorAll('.u-axis');
        if (axes.length >= 3) {
            axes[0].id = 'axis-x';
            axes[0].style.touchAction = 'none';
            axes[1].id = 'axis-yA';
            axes[1].style.touchAction = 'none';
            axes[2].id = 'axis-yB';
            axes[2].style.touchAction = 'none';
            const dpr = window.devicePixelRatio || 1;
            let _axisDrag = null;
            axes[0].addEventListener('pointerdown', (e) => {
                if (e.button !== 0) return;
                axes[0].setPointerCapture(e.pointerId);
                _axisDrag = {
                    startX: e.clientX,
                    startOffset: vslParameters["trg.time.offset"] || 0,
                    uvPerPx: dpr / uplot.bbox.width
                };
            });
            axes[0].addEventListener('pointermove', (e) => {
                if (!_axisDrag) return;
                const newOffset = Math.max(-1e6, Math.min(1e6, _axisDrag.startOffset - (e.clientX - _axisDrag.startX) * _axisDrag.uvPerPx * 1e6));
                vslParameters["trg.time.offset"] = Math.round(newOffset);
                Hardware.sendTriggerParameters();//todo debouncing
                ParametersStorage.save();
                updatePlot();
            });
            const dragStop =() =>{_axisDrag = null;};
            axes[0].addEventListener('pointerup', dragStop);
            axes[0].addEventListener('pointerleave', dragStop);
            const _axisYSetup = (axis, chName) => {
                axis.addEventListener('pointerdown', (e) => {
                    if (e.button !== 0) return;
                    axis.setPointerCapture(e.pointerId);
                    const rng = vslParameters.channels[chName]["range.uv"];
                    _axisDrag = {
                        startY: e.clientY,
                        startOffset: vslParameters.channels[chName]["base.lvl.uv"],
                        lc: chName,
                        uvPerPx: rng * dpr / uplot.bbox.height
                    };
                });
                axis.addEventListener('pointermove', (e) => {
                    if (!_axisDrag) return;
                    const deltaUv = (e.clientY - _axisDrag.startY) * _axisDrag.uvPerPx;
                    vslParameters.channels[chName]["base.lvl.uv"] = _axisDrag.startOffset + deltaUv;
                    Hardware.sendChannelParameters(chName)//todo debounce
                    ParametersStorage.save();
                    updatePlot();
                });
                axis.addEventListener('pointerup', dragStop);
                axis.addEventListener('pointerleave', dragStop);
            };
            _axisYSetup(axes[1], 'a');
            _axisYSetup(axes[2], 'b');
        }
    } catch (e) {
        console.error("uPlot init fail:", e);
    }
    window.addEventListener("resize", () => {
        const r = el.getBoundingClientRect();
        if (uplot) uplot.setSize({width: r.width, height: r.height});
    });
}

function updatePlot() {
    const frameSize = frame.parameters["frame.size"] || keyFrame.parameters["frame.size"] || 200;
    try {
        const sp = vslParameters["sampling.ns"] || 500000;
        const xAxisMs = new Array(frameSize);
        xAxisMs[0] = frameSize * sp * vslParameters["trg.time.offset"] / 1000000 / 1000000
        for(let i = 1; i < frameSize; i++) {
            xAxisMs[i] = xAxisMs[i-1] + sp / 1000000;
        }
        const data = [xAxisMs]
        for (const f of [frame, keyFrame]) {
            const samplingTimeOk = f.parameters["sampling.ns"] === sp
            for(const ch of ["a", "b"]) {
                const [displayRange, displayBase] = [vslParameters.channels[ch]["range.uv"], vslParameters.channels[ch]["base.lvl.uv"]];
                const [displayMin, displayMax] = [displayBase - displayRange/2, displayBase + displayRange/2];
                const [min, max, steps] = [f.parameters[`vltg.min.uv.${ch}`] || -1650000, f.parameters[`vltg.max.uv.${ch}`] || 1650000, f.parameters[`vltg.steps`] || 4096];
                const srcArr = f.samples[ch]
                const arr = new Array(frameSize);
                if(samplingTimeOk && srcArr) {
                    const d = (max - min) / steps;
                    for (let i = 0; i < srcArr.length; i++) {
                        const v = min + srcArr[i] * d;
                        arr[i] = clampValue(v, displayMin, displayMax);
                    }
                }
                data.push(arr)
            }
        }
        uplot.setData(data, true);
    } catch (e) {
        console.error("updatePlot:", e);
    }
}

function setStatus(t, e) {
    const el = document.getElementById("status");
    if (el) {
        el.textContent = t;
        el.className = e ? 'error' : '';
    }
}

function appendStatus(t) {
    const el = document.getElementById("status");
    if (el) el.textContent += " " + t;
}


if (!window.isSecureContext) document.querySelectorAll('.transport-btn[data-mode="ble"],.transport-btn[data-mode="serial"]').forEach(b => b.classList.add('insecure'));
document.querySelectorAll('.transport-btn[data-mode]').forEach(btn => {
    btn.addEventListener('click', () => {
        const m = btn.dataset.mode;
        if (!window.isSecureContext && m !== 'wifi') {
            window.open('https://elmot.xyz/oscilloscope', '_blank');
            return;
        }
        // noinspection JSIgnoredPromiseFromCall
        comm.switchTo(btn.classList.contains('active') ? 'none' : m);
    });
});
const fsBtn = document.getElementById('btn-fullscreen');
if (fsBtn) {
    fsBtn.onclick = () => {
        if (!document.fullscreenElement) document.documentElement.requestFullscreen?.(); else document.exitFullscreen?.();
    };
    document.addEventListener('fullscreenchange', () => fsBtn.classList.toggle('active', !!document.fullscreenElement));
}

const host = location.hostname;
if (host.endsWith('.local') || (host === '127.0.0.1') || !host.includes('.')) {
    // noinspection JSIgnoredPromiseFromCall
    comm.switchTo('wifi');
} else {
    setStatus("Select transport");
}
document.getElementById("trgShiftReset").onclick = () => {
    vslParameters["trg.time.offset"] = 0;
    Hardware.sendTriggerParameters();
    ParametersStorage.save();
};

document.getElementById("pauseBtn").onclick = function () {
    paused = !paused;
    this.textContent = paused ? "Resume" : "Pause";
    this.classList.toggle("paused", paused);
};

{
    const samplingInput = document.getElementById("sampling.ns");
    if (vslParameters["sampling.ns"]) {
        samplingInput.value = vslParameters["sampling.ns"];
    }
    const changeSampling = function () {
        vslParameters["sampling.ns"] = parseFloat(samplingInput.value);
        Hardware.sendTimingParameters();
        ParametersStorage.save();
        clearFrames();
        updatePlot();
    };
    samplingInput.onchange = changeSampling
    for(const btn of samplingInput.parentElement.getElementsByTagName("button")) {
        btn.onclick = () =>{
            const n = samplingInput.selectedIndex + parseInt(btn.dataset.val);
            samplingInput.selectedIndex = clampValue(n, 0, samplingInput.options.length - 1);
            changeSampling();
        };
    }
}
initUplot();
