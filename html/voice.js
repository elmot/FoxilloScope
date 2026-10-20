// Optional voice control. Every command "presses" an existing UI control, so parameter
// handling, hardware sends and ParametersStorage stay in scope.js.
(() => {
    const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
    const btn = document.getElementById('btn-voice');
    if (!SR || !btn) return;
    btn.hidden = false;

    const STORAGE_KEY = 'vslVoice';
    const remember = on => { try { localStorage.setItem(STORAGE_KEY, on ? '1' : '0'); } catch (e) {} };
    const remembered = () => { try { return localStorage.getItem(STORAGE_KEY) === '1'; } catch (e) { return false; } };

    if (!window.isSecureContext) {
        btn.classList.add('insecure');
        btn.onclick = () => window.open('https://elmot.xyz/f-scope', '_blank');
        return;
    }

    // Commands are English, so only follow the browser locale when it is an English one
    const lang = /^en\b/i.test(navigator.language) ? navigator.language : 'en-US';
    const FATAL = ['not-allowed', 'service-not-allowed', 'audio-capture', 'network', 'language-not-supported'];
    let rec = null, enabled = false, local = false;

    async function checkLocal() {
        if (!SR.available) return false;
        try {
            const opts = {langs: [lang], processLocally: true};
            const state = await SR.available(opts);
            if (state === 'available') return true;
            if (state === 'downloadable' && SR.install) {
                // Use the cloud recognizer meanwhile; the next restart picks up the local model
                SR.install(opts).then(ok => { if (ok) local = true; }).catch(() => {});
            }
        } catch (e) {}
        return false;
    }

    function start() {
        rec = new SR();
        rec.lang = lang;
        rec.continuous = true;
        rec.interimResults = false;
        rec.maxAlternatives = 3;
        if (local) rec.processLocally = true;
        rec.onresult = ev => {
            for (let i = ev.resultIndex; i < ev.results.length; i++) {
                if (ev.results[i].isFinal) handle(Array.from(ev.results[i], alt => alt.transcript));
            }
        };
        rec.onerror = ev => {
            if (local && (ev.error === 'language-not-supported' || ev.error === 'service-not-allowed')) {
                local = false; // on-device refused, retry in the cloud
                return;
            }
            if (FATAL.includes(ev.error)) {
                setStatus(`Voice: ${ev.error}`, true);
                setEnabled(false);
            }
        };
        // Chrome ends the session by itself after silence; keep listening while enabled
        rec.onend = () => { if (enabled) setTimeout(() => enabled && start(), 250); };
        try { rec.start(); } catch (e) { setStatus(`Voice: ${e.message}`, true); setEnabled(false); }
    }

    async function setEnabled(on) {
        if (on === enabled) return;
        enabled = on;
        remember(on);
        btn.classList.toggle('active', on);
        if (on) {
            local = await checkLocal();
            if (!enabled) return;
            setStatus(`Voice: listening${local ? ' (on-device)' : ''}`);
            start();
        } else if (rec) {
            rec.abort();
            rec = null;
        }
    }

    btn.onclick = () => setEnabled(!enabled);
    // Browsers want a user gesture before opening the mic, so resume on the first interaction
    if (remembered()) {
        document.addEventListener('pointerdown', ev => { if (!btn.contains(ev.target)) setEnabled(true); }, {once: true});
    }

    const flash = cls => {
        btn.classList.add(cls);
        setTimeout(() => btn.classList.remove(cls), 400);
    };

    const NUMBERS = {
        one: 1, two: 2, three: 3, four: 4, five: 5, six: 6, seven: 7, eight: 8, nine: 9, ten: 10,
        twelve: 12, sixteen: 16, twenty: 20, fifty: 50, hundred: 100
    };
    function normalise(text) {
        return text.toLowerCase()
            .replace(/[.,!?]/g, '')
            .replace(/×/g, ' times ')
            .replace(/\b(\d+)\s*x\b|\bx\s*(\d+)\b/g, (m, a, b) => `times ${a || b}`)
            .replace(/\btimes (to|too)\b/g, 'times 2')
            .replace(/\btimes for\b/g, 'times 4')
            .replace(/\bthirty[ -]two\b/g, '32')
            .replace(/\bsixty[ -]four\b/g, '64')
            .replace(/\b[a-z]+\b/g, w => NUMBERS[w] ?? w)
            .replace(/\s+/g, ' ')
            .trim();
    }

    const $ = id => document.getElementById(id);
    const click = el => el && el.click();
    const nudge = (slider, delta) => {
        slider.value = clampValue(parseFloat(slider.value) + delta, parseFloat(slider.min), parseFloat(slider.max));
        slider.dispatchEvent(new Event('input'));
    };
    const setPaused = p => { if ($('pauseBtn').classList.contains('paused') !== p) click($('pauseBtn')); };
    const trgType = v => click($('trg.type').querySelector(`button[data-val="${v}"]`));
    const timebase = v => click(document.querySelector(`.step-control button[data-val="${v}"]`));
    const chan = c => /^b/.test(c) ? 'b' : 'a';
    const CH = '(?:channel )?(a|eh|ay|b|be|bee)';
    // Without a named channel, gain commands follow the trigger source
    const gainSlider = c => $('gain.' + (c ? chan(c) : $('trg.chan').querySelector('.active')?.dataset.val === '1' ? 'b' : 'a'));

    // First match wins, so more specific phrases come first
    const COMMANDS = [
        [/\b(stop listening|voice off|mic off)\b/, () => setEnabled(false)],
        [/\btrigger (reset|cent(?:er|re)|start)\b/, () => click($('trgShiftReset'))],
        [/\btrigger (?:level )?(up|higher|raise|down|lower)\b(?:.*\b(bit|little|lot)\b)?/, m => {
            const slider = $('trg.level');
            const step = (slider.max - slider.min) * ({bit: 0.01, little: 0.01, lot: 0.2}[m[2]] ?? 0.05);
            nudge(slider, /up|higher|raise/.test(m[1]) ? step : -step);
        }],
        [/\b(?:trigger )?(rising|rise|positive)\b/, () => trgType(1)],
        [/\b(?:trigger )?(falling|fall|negative)\b/, () => trgType(-1)],
        [/\btrigger (off|none|no|disabled?)\b/, () => trgType(0)],
        [new RegExp(`\\btrigger (?:on )?${CH}\\b`), m => click($('trg.chan').querySelector(`button[data-val="${chan(m[1]) === 'a' ? 0 : 1}"]`))],
        [new RegExp(`\\bgain (?:${CH} )?(up|higher|more|down|lower|less)\\b`), m =>
            nudge(gainSlider(m[1]), /up|higher|more/.test(m[2]) ? Math.LN2 : -Math.LN2)],
        [new RegExp(`\\bgain (?:${CH} )?(?:times |to )?(\\d+(?:\\.\\d+)?)\\b`), m => {
            const slider = gainSlider(m[1]);
            slider.value = Math.log(clampValue(parseFloat(m[2]), 1, Gain.MAX));
            slider.dispatchEvent(new Event('input'));
        }],
        [/\b(faster|zoom in|shorter)\b/, () => timebase(1)],
        [/\b(slower|zoom out|longer)\b/, () => timebase(-1)],
        [/\b(pause|stop|freeze|hold)\b/, () => setPaused(true)],
        [/\b(resume|run|go|continue|start)\b/, () => setPaused(false)],
    ];

    function match(text) {
        for (const [re, action] of COMMANDS) {
            const m = text.match(re);
            if (m) return () => action(m);
        }
        return null;
    }

    function handle(alternatives) {
        for (const alt of alternatives) {
            const text = normalise(alt);
            const actions = text.split(/\b(?:and|then)\b/).map(s => s.trim()).filter(Boolean).map(match);
            if (actions.length && actions.every(Boolean)) {
                actions.forEach(run => run());
                setStatus(`🎤 "${text}"`);
                flash('heard');
                return;
            }
        }
        setStatus(`🎤 "${alternatives[0]?.trim() ?? ''}"`);
        appendStatus('?');
        flash('missed');
    }
})();
