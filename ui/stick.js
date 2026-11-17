/* The face of a connected stick: the OLED module's ctx built from what the page's control line has read (the HOME line, `get`,
   `status`, the beat reports), the control-line commands the face's buttons and menu send, and the button logic of the D-pad.
   No DOM, no timers: time is always passed in (ms). The plain XIAO has no screen, so this is a virtual screen of the stick's state:
   what the stick does not report is left empty ('---', a level at the floor, no beat), never made up.
   The commands are the ones beat/live/player.html's --xiao mode sends: `fold x2` / `fold /2` (AUDIO) or `tempo <bpm>` (LINK) on
   UP / DOWN, `play` / `stop` on YES, `set <key> <value>` for the menu's edits, `nudge beat` / `nudge ms` for NO / YES held with
   UP / DOWN (core/jank_ctl.c, docs/firmware.md). */
const Stick = (() => {
  const KEY_BTN = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'no', Escape: 'no', Backspace: 'no', ArrowRight: 'yes', Enter: 'yes' };
  const KEY_HINT = 'Keys: Up / Down = UP / DOWN, Left / Esc / Backspace = NO, Right / Enter = YES';
  const REPEAT = { firstMs: 400, everyMs: 120 };
  const FLOOR_DB = -99, SILENCE_DB = -60, TEMPO_RANGE = [20, 999], BAR_CONF_FULL = 2;
  const FRESH_USB_MS = 1500, FRESH_BLE_MS = 5000, STATUS_FRESH_MS = 5000;
  const SRC = ['auto', 'line', 'usb', 'link'], BLE = ['pair', 'auto', 'off'], AUTOBAR = ['off', 'start', 'always'], FOLDS = { x2: 2, x4: 4, '/2': 0.5, '/4': 0.25 };
  const LED = { beatMs: 80, barMs: 150, flashCapDiv: 3, scale: 40, beat: [0, 30, 0], one: [40, 18, 0], armed: [30, 20, 0], lowConf: [40, 0, 0] };
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const num = v => (v === undefined || v === null || v === '' || !Number.isFinite(Number(v)) ? null : Number(v));
  const isUpDown = b => b === 'up' || b === 'down';

  // ---- the beat reports: `b <n> <bpm x100> <mode> <audio> <lag us>` on each tracked beat, `t ...` once a second (docs/beat.md) ----
  function parseBeat(text) {
    const f = String(text).trim().split(/\s+/);
    if (f[0] !== 'b' || f.length < 6) return null;
    const [n, bpm100, , audio, lagUs] = f.slice(1).map(Number);
    return [n, bpm100, audio, lagUs].every(Number.isFinite) ? { n, bpm: bpm100 / 100, mode: f[3], audio, lagUs } : null;
  }

  // ---- what the stick says, as the OLED reads it ----
  // state: { home (the page's parseHome), homeAt, get ({key: string}), status ({key: string}), statusAt, beat (parseBeat + at), overBle }
  const fresh = (at, now, maxMs) => at !== undefined && at !== null && now - at <= maxMs;
  const phase = (h, homeAt, now) => {                               // the session's place in the bar (beats), moving at the HOME line's tempo
    if (!h || !(h.bpm > 0) || h.bar === null || h.bar === undefined || !h.quantum) return null;
    return h.bar / 100 + (h.transport === 'stopped' ? 0 : Math.max(0, now - homeAt) / 60000 * h.bpm);
  };
  const foldOf = h => (h && FOLDS[h.fold] ? FOLDS[h.fold] : 1);

  function ctx(state, now) {
    const g = state.get || {}, st = state.status && fresh(state.statusAt, now, STATUS_FRESH_MS) ? state.status : null;
    const h = state.home && fresh(state.homeAt, now, state.overBle ? FRESH_BLE_MS : FRESH_USB_MS) ? state.home : null;
    const audio = !!(h && h.audio), pos = phase(h, state.homeAt, now);
    const playing = !!h && h.transport === 'playing', armed = !!h && h.transport === 'armed';
    const level = h && h.level !== null ? h.level : FLOOR_DB;
    const barLead = st ? num(st.bar_lead) : null, barState = st && st.bar ? st.bar : null;
    const net = g.net === 'wifi' ? 1 : 0;
    const wifiBars = h && h.net && h.net.kind === 'wifi' && !h.net.up ? 'none' : st && num(st.rssi) !== null && num(st.rssi) !== 0 ? (num(st.rssi) > -70 ? 'good' : 'weak') : 'unknown';
    return {
      link: !audio, bpm: audio && h.bpm > 0 ? h.bpm : null, linkBpm: !audio && h && h.bpm > 0 ? h.bpm : null, mode: 'steady',
      conf: h && h.conf !== null ? h.conf / 100 : 0, confHold: !!(h && h.confHold), th: num(g.threshold) === null ? 80 : num(g.threshold), gain: num(g.line_gain) || 0,
      confStop: g.lowconf !== 'run', levelDb: level, levelMin: h && h.levelMin !== null ? h.levelMin : FLOOR_DB, levelMax: h && h.levelMax !== null ? h.levelMax : FLOOR_DB,
      silent: level < SILENCE_DB, gated: !!st && st.gated === '1', hold: !!h && h.transport === 'stopped', playing, running: playing, armed,
      ltMode: g.transport === 'auto' ? 0 : 2, xiao: true, fold: foldOf(h), foldAuto: !!st && st.fold_manual === '0' && foldOf(h) !== 1,
      beatN: h && pos !== null && playing ? Math.floor(pos) : null, beatMs: 0,
      peers: h ? h.link : 0, usb: true, fitted: !!h && h.battery !== null, batt: h && h.battery !== null ? h.battery : 0,
      lineIn: g.source === 'line' || (!!st && st.audio_source === 'line'), bright: 100, sleep: 0, inOff: num(g.lat_trim) || 0, offs: [0, 0, 0], rate: 2,
      boot: false, splash: false, wifi: wifiBars, wifiQ: null, groupPeers: h ? h.air : 0, setup: false, saved: false,
      net: { mode: net, prev: net, group: num(g.group) || 1, ssid: g.ssid || '', pass: g.pass === 'set' ? '********' : '', appass: '', apName: '',
             status: h && h.net && h.net.kind === 'wifi' ? (h.net.up ? 'CONNECTED' : 'LOOKING') : 'NO NETWORK', ip: st && st.link_ip && st.link_ip !== '0.0.0.0' ? st.link_ip : '-',
             apClients: 0, setupName: '', scan: [] },
      ble: { mode: Math.max(0, BLE.indexOf(g.ble)), left: st && st.ble_state === 'window' ? num(st.ble_left) : null, phone: !!st && st.ble_state === 'connected' },
      inSrc: Math.max(0, SRC.indexOf(g.source)), usbRole: g.usb === 'host' ? 1 : 0, usbMidi: g.usbmidi === 'off' ? 1 : 0,
      autoBar: Math.max(0, AUTOBAR.indexOf(g.autobar)),
      bar: barState ? { state: barState, conf: barLead === null ? 0 : clamp(barLead / BAR_CONF_FULL, 0, 1) } : null,
    };
  }

  // ---- the beat LEDs: a flash at each beat the stick reports, the colours of the device's LED language (core/jank_led.h) ----
  const ledPaint = rgb => rgb.map(v => Math.round(255 * Math.pow(Math.min(1, v / LED.scale), 0.6)));
  const css = rgb => { const c = ledPaint(rgb); return `rgb(${c[0]},${c[1]},${c[2]})`; };
  // The flash is anchored on a beat report (b ...) when one came lately, else on the HOME line's phase (Link mode, Bluetooth: no reports).
  function leds(state, now) {
    const dark = [null, null, null, null], h = state.home && fresh(state.homeAt, now, state.overBle ? FRESH_BLE_MS : FRESH_USB_MS) ? state.home : null;
    if (!h) return dark;
    if (h.transport === 'armed') return Array(4).fill(css(LED.armed));
    if (h.transport !== 'playing' || !(h.bpm > 0)) return dark;
    const period = 60000 / h.bpm, pos = phase(h, state.homeAt, now);
    if (pos === null) return dark;
    const b = state.beat, reported = b && now - b.at - b.lagUs / 1000 < 2 * period && now - b.at >= 0;
    const sinceEdge = reported ? ((now - (b.at - b.lagUs / 1000)) % period + period) % period : (pos - Math.floor(pos)) * period;
    const edgePos = pos - sinceEdge / period, index = ((Math.floor(edgePos + 1e-6) % h.quantum) + h.quantum) % h.quantum;
    const one = index === 0, flash = Math.min(one ? LED.barMs : LED.beatMs, period / LED.flashCapDiv);
    if (sinceEdge >= flash) return dark;
    const gated = state.status && state.status.gated === '1';
    const rgb = gated ? LED.lowConf : one ? LED.one : LED.beat;
    return [0, 1, 2, 3].map(i => (i === index % 4 ? css(rgb) : null));
  }

  // ---- the control line: what a menu edit, a tempo step, Start / Stop and the nudges send ----
  const refuse = why => ({ refuse: why });
  const cmds = (...c) => ({ commands: c });
  const NO_STICK_SETTING = {
    out0: 'the stick has one clock offset (Advanced), not one per output', out1: 'the stick has one clock offset (Advanced), not one per output',
    out2: 'the stick has one clock offset (Advanced), not one per output', rate: 'the stick has no clock rate setting',
    bright: 'the stick has no screen to dim', sleep: 'the stick has no screen to switch off',
    ssid: 'set the Wi-Fi name under Settings (Save and restart)', pass: 'set the Wi-Fi password under Settings (Save and restart)',
    apname: 'the stick has no own-network mode', appass: 'the stick has no own-network mode',
    netmode: 'change the network under Settings (Join, Save and restart): it restarts the stick',
    usbrole: 'the USB role restarts the stick: not changed from here',
  };
  const WORDS = { insrc: SRC, ble: BLE, autobar: AUTOBAR, lowconf: ['run', 'stop'], usbmidi: ['on', 'off'] };
  const KEYS = { insrc: 'source', ble: 'ble', autobar: 'autobar', lowconf: 'lowconf', usbmidi: 'usbmidi' };
  // an OLED edit (field, value as the OLED holds it) -> { commands: [...] } or { refuse: 'why' }
  function fieldCommands(field, val) {
    if (NO_STICK_SETTING[field]) return refuse(NO_STICK_SETTING[field]);
    if (field === 'threshold') return cmds('set threshold ' + clamp(Math.round(val), 0, 100));
    if (field === 'group') return cmds('set group ' + (val + 1));
    if (field === 'gain') return cmds('set line_gain ' + clamp(Math.round(val), -12, 12));
    if (field === 'inoff') return cmds('set lat_trim ' + clamp(Math.round(val), -100, 100));
    if (field === 'ltmode') return val === 1 ? refuse('FOLLOW is not on the control line: pick IGNORE or FOLLOW+LEAD') : cmds('set transport ' + (val === 0 ? 'auto' : 'link'));
    if (field === 'lowconf') return cmds('set lowconf ' + (val === 1 ? 'stop' : 'run'));
    if (field === 'ble') return val === 0 ? cmds('set ble pair', 'ble pair') : cmds('set ble ' + BLE[val]);
    if (field === 'usbmidi') return cmds('set usbmidi ' + (val === 0 ? 'on' : 'off'));
    if (WORDS[field] && WORDS[field][val] !== undefined) return cmds('set ' + KEYS[field] + ' ' + WORDS[field][val]);
    return refuse('the stick does not take ' + field + ' from here');
  }
  // UP / DOWN on HOME: AUDIO folds the tracked tempo (x2 / /2), LINK steps the session tempo by 1 BPM. `expected` is what the
  // stick should report next (for the overlay only; the screen then shows what the stick reports), null when unknown
  function tempoCommand(c, d) {
    if (!c.link) {
      const next = clamp(c.fold * (d > 0 ? 2 : 0.5), 0.25, 4);
      return { commands: [d > 0 ? 'fold x2' : 'fold /2'], expected: c.bpm === null ? null : c.bpm * next / c.fold };
    }
    if (c.linkBpm === null) return refuse('no Link session yet: no tempo to change');
    const bpm = clamp(Math.round(c.linkBpm) + d, TEMPO_RANGE[0], TEMPO_RANGE[1]);
    return { commands: ['tempo ' + bpm], expected: bpm };
  }
  const playCommand = on => (on ? 'play' : 'stop');
  // NO held + UP / DOWN: PHASE (half a beat), YES held + UP / DOWN: AUDIO SHIFT (1 ms); AUDIO mode only
  function nudgeCommand(c, mod, d) {
    if (c.link) return refuse('the nudges are for AUDIO mode (music heard)');
    return cmds(mod === 'no' ? 'nudge beat ' + (d > 0 ? '+0.5' : '-0.5') : 'nudge ms ' + (d > 0 ? '+1' : '-1'));
  }

  // ---- the D-pad: UP / DOWN act at once and repeat while held, NO / YES on HOME act on release, elsewhere at once; NO or YES held
  // with UP / DOWN on HOME is a combo (the nudges), which cancels the release action ----
  function create(getCtx, onCombo) {
    return { ui: OLED.create(), held: { up: false, down: false, no: false, yes: false }, deferred: {}, combo: false, repeatAt: {}, getCtx, onCombo };
  }
  const press = (m, b, now) => OLED.press(m.ui, b, now, m.getCtx(now));
  const onHome = (m, now) => m.ui.screen === 'home' && !m.getCtx(now).setup;
  function down(m, b, now) {
    if (m.held[b]) return;
    m.held[b] = true;
    if (!onHome(m, now)) { press(m, b, now); if (isUpDown(b)) m.repeatAt[b] = now + REPEAT.firstMs; return; }
    if (isUpDown(b)) {
      const mod = m.held.no ? 'no' : m.held.yes ? 'yes' : null;
      if (mod && m.deferred[mod]) { m.combo = true; m.onCombo(mod, b === 'up' ? 1 : -1); return; }
      press(m, b, now);
      m.repeatAt[b] = now + REPEAT.firstMs;
      return;
    }
    m.deferred[b] = true;
    m.combo = false;
  }
  function up(m, b, now) {
    if (!m.held[b]) return;
    m.held[b] = false;
    delete m.repeatAt[b];
    if (!m.deferred[b]) return;
    m.deferred[b] = false;
    if (onHome(m, now) && !m.combo) press(m, b, now);
  }
  function tick(m, now) {
    for (const b of Object.keys(m.repeatAt)) {
      while (m.repeatAt[b] <= now) { press(m, b, m.repeatAt[b]); m.repeatAt[b] += REPEAT.everyMs; }
    }
  }
  const releaseAll = (m, now) => { for (const b of Object.keys(m.held)) up(m, b, now); };
  const framebuffer = (m, now) => { const fb = new Uint8Array(OLED.W * OLED.H); OLED.draw(fb, m.ui, m.getCtx(now), now); return fb; };

  return { KEY_BTN, KEY_HINT, REPEAT, parseBeat, ctx, leds, fieldCommands, tempoCommand, playCommand, nudgeCommand, create, down, up, tick, releaseAll, framebuffer };
})();
