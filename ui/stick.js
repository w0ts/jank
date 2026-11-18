/* The face of a connected stick: the OLED module's ctx built from what the page's control line has read (the HOME line, `get`,
   `status`), the control-line commands the face's buttons and menu send, and the button logic of the D-pad.
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
  const FRESH_HOME_MS = 1500, STATUS_FRESH_MS = 5000, ABOUT_FRESH_MS = 5000;
  const SRC = ['auto', 'line', 'usb', 'link'], AUTOBAR = ['off', 'start', 'always'], FOLDS = { x2: 2, x4: 4, '/2': 0.5, '/4': 0.25 };
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const num = v => (v === undefined || v === null || v === '' || !Number.isFinite(Number(v)) ? null : Number(v));
  const isUpDown = b => b === 'up' || b === 'down';

  // ---- what the stick says, as the OLED reads it ----
  // state: { home (the page's parseHome), homeAt, get ({key: string}), status ({key: string}), statusAt, about ({key: string}: the `about` reply), aboutAt, phase (phaseFeed, optional), delayMs (the display delay) }
  const fresh = (at, now, maxMs) => at !== undefined && at !== null && now - at <= maxMs;
  const phase = (h, homeAt, now) => {                               // the session's place in the bar (beats), moving at the HOME line's tempo
    if (!h || !(h.bpm > 0) || h.bar === null || h.bar === undefined || !h.quantum) return null;
    return h.bar / 100 + (h.transport === 'stopped' ? 0 : Math.max(0, now - homeAt) / 60000 * h.bpm);
  };
  // ---- the smooth local phase: the bar position the boxes show, advanced with the page's own clock between the stick's reports ----
  // A report is the HOME line's bar position (beats) and tempo, taken at the page's time `t`. Between reports the position runs on at the last
  // tempo; a new report corrects it softly: a quarter of the error, and backwards by 0.02 beat at most, unless it is further than half a
  // beat out (then it snaps). A stopped transport holds the position; no report for 1.5 s, or a report with no session / no tempo (a lost
  // lock), and there is no position at all: the boxes go empty instead of running on stale data.
  const PHASE = { staleMs: 1500, snapBeats: 0.5, gain: 0.25, backBeats: 0.02 };
  const createPhase = () => ({ pos: null, at: 0, bpm: 0, running: false, quantum: 4, reportAt: -Infinity });
  const wrapDiff = (d, q) => ((((d + q / 2) % q) + q) % q) - q / 2;
  const phaseReport = h => (h && h.bpm > 0 && h.bar !== null && h.bar !== undefined && h.quantum > 0 ? { bar: h.bar / 100, bpm: h.bpm, running: h.transport === 'playing', quantum: h.quantum } : null);
  function phaseFeed(ph, r, t) {
    if (!r) return createPhase();
    if (ph.pos === null) return { pos: r.bar, at: t, bpm: r.bpm, running: r.running, quantum: r.quantum, reportAt: t };
    const predicted = ph.running ? ph.pos + (t - ph.at) * ph.bpm / 60000 : ph.pos, e = wrapDiff(r.bar - predicted, r.quantum);
    const pos = !r.running ? r.bar : Math.abs(e) > PHASE.snapBeats ? r.bar : predicted + (e >= 0 ? e * PHASE.gain : Math.max(e * PHASE.gain, -PHASE.backBeats));
    return { pos, at: t, bpm: r.bpm, running: r.running, quantum: r.quantum, reportAt: t };
  }
  // the position (beats, not wrapped) the boxes show at `now`, shifted by the display delay (+: as it was that long ago); null: none to show
  function phaseAt(ph, now, delayMs) {
    if (ph.pos === null || now - ph.reportAt > PHASE.staleMs) return null;
    return ph.running ? ph.pos + (now - (delayMs || 0) - ph.at) * ph.bpm / 60000 : ph.pos;
  }
  // The display delay the page can estimate from what it knows: the monitor reaches the ear `monitorMs` after the file position, the stick
  // path `stickMs` (the part the browser can read), and the stick aligns its session beat by `beatLatencyMs` (the value it uses now). The
  // boxes are drawn when the stick reports, so they are off by (stick path + beat_latency - monitor): the delay that undoes it is
  // monitor - stick path - beat_latency. With Auto latency the stick's beat_latency becomes monitor - stick path and this is 0: the boxes
  // are then not corrected twice. Without a known beat_latency: monitor - stick path, as before. To 5 ms.
  const BEAT_DELAY_MAX = 400, BEAT_DELAY_KEY = 'jank.beat.delay';
  const clampDelay = v => clamp(Math.round(v / 5) * 5, -BEAT_DELAY_MAX, BEAT_DELAY_MAX);
  const beatDelayEstimate = l => (Number.isFinite(l.monitorMs) && Number.isFinite(l.stickMs) ? clampDelay(l.monitorMs - l.stickMs - (Number.isFinite(l.beatLatencyMs) ? l.beatLatencyMs : 0)) : 0);
  const parseDelay = text => (text === null || text === undefined || text === '' || !Number.isFinite(Number(text)) ? null : clampDelay(Number(text)));
  const foldOf = h => (h && FOLDS[h.fold] ? FOLDS[h.fold] : 1);

  function ctx(state, now) {
    const g = state.get || {}, st = state.status && fresh(state.statusAt, now, STATUS_FRESH_MS) ? state.status : null;
    const h = state.home && fresh(state.homeAt, now, FRESH_HOME_MS) ? state.home : null;
    // the ABOUT screen's numbers (the `about` reply, only read while that screen is open): the firmware's unknowns (-1, temp_c -999, reset empty) are null here
    const ab = state.about && fresh(state.aboutAt, now, ABOUT_FRESH_MS) ? state.about : {}, abNum = k => (num(ab[k]) === null || num(ab[k]) < 0 ? null : num(ab[k]));
    const audio = !!(h && h.audio), delay = state.delayMs || 0, pos = state.phase ? phaseAt(state.phase, now, delay) : phase(h, state.homeAt, now - delay);
    const playing = !!h && h.transport === 'playing', armed = !!h && h.transport === 'armed';
    const level = h && h.level !== null ? h.level : FLOOR_DB;
    const barLead = st ? num(st.bar_lead) : null, barState = st && st.bar ? st.bar : null;
    const net = g.net === 'wifi' ? 1 : 0;
    // the signal for the lcd2's bars: Wi-Fi client = the router's RSSI (status wifi_rssi), jank = the best peer's (status rssi); 0 = none
    // heard; null = the stick has not said (no fresh status). Nothing is made up.
    const wifiMode = !!(h && h.net && h.net.kind === 'wifi');
    const rssiRaw = st ? num(wifiMode ? st.wifi_rssi : st.rssi) : null, rssi = wifiMode && !h.net.up ? 0 : rssiRaw;
    // the peers' air over the last 10 s window (status air_*, rssi_min / rssi_max): null = not known (no fresh status, or the firmware's -1); a level of 0 = none
    const pctOf = v => (num(v) === null || num(v) < 0 ? null : num(v));
    const wifiBars = h && h.net && h.net.kind === 'wifi' && !h.net.up ? 'none' : st && num(st.rssi) !== null && num(st.rssi) !== 0 ? (num(st.rssi) > -70 ? 'good' : 'weak') : 'unknown';
    return {
      link: !audio, bpm: audio && h.bpm > 0 ? h.bpm : null, linkBpm: !audio && h && h.bpm > 0 ? h.bpm : null, mode: 'steady',
      conf: h && h.conf !== null ? h.conf / 100 : 0, confKnown: !!h && h.conf !== null, rssi, airLost: st ? pctOf(st.air_lost_pct) : null, airRetry: st ? pctOf(st.air_retry_pct) : null,
      rssiMin: st ? num(st.rssi_min) : null, rssiMax: st ? num(st.rssi_max) : null, peakDb: st ? num(st.peak_db) : null, confHold: !!(h && h.confHold), th: num(g.threshold) === null ? 80 : num(g.threshold), gain: num(g.line_gain) || 0,
      confStop: g.lowconf !== 'run', levelDb: level, levelMin: h && h.levelMin !== null ? h.levelMin : FLOOR_DB, levelMax: h && h.levelMax !== null ? h.levelMax : FLOOR_DB,
      silent: level < SILENCE_DB || (!!st && st.silent === '1'), gated: !!st && st.gated === '1', hold: !!h && h.transport === 'stopped' && !(st && st.hold === '0'), playing, running: playing, armed,
      ltMode: g.transport === 'auto' ? 0 : 2, xiao: true, fold: foldOf(h), foldAuto: !!st && st.fold_manual === '0' && foldOf(h) !== 1,
      beatN: h && pos !== null && playing ? Math.floor(pos) : null, beatMs: 0,
      peers: h ? h.link : 0, usb: true, fitted: !!h && h.battery !== null, batt: h && h.battery !== null ? h.battery : 0, tempC: num(ab.temp_c) === null || num(ab.temp_c) === -999 ? null : num(ab.temp_c), cpuPct: abNum('cpu_pct'), heapKb: abNum('heap_kb'), psramKb: abNum('psram_kb'), heapMinKb: abNum('heap_min_kb'),
      uptimeS: abNum('uptime_s'), flashKb: abNum('flash_kb'), appUsedKb: abNum('app_used_kb'), appKb: abNum('app_part_kb'), reset: ab.reset ? ab.reset : null, version: g.version ? g.version : null,
      clkLateUs: abNum('clock_late_us'), uiLoopUs: abNum('ui_loop_us'), beatLoopUs: abNum('beat_loop_us'),
      lineIn: g.source === 'line' || (!!st && st.audio_source === 'line'), bright: 100, sleep: 0, inOff: num(g.lat_trim) || 0, offs: [0, 0, 0], rate: 2,
      boot: false, splash: false, wifi: wifiBars, wifiQ: null, groupPeers: h ? h.air : 0, setup: false, saved: false,
      net: { mode: net, prev: net, group: num(g.group) || 1, ssid: g.ssid || '', pass: g.pass === 'set' ? '********' : '', appass: '', apName: '',
             status: h && h.net && h.net.kind === 'wifi' ? (h.net.up ? 'CONNECTED' : 'LOOKING') : 'NO NETWORK', ip: st && st.link_ip && st.link_ip !== '0.0.0.0' ? st.link_ip : '-',
             apClients: 0, setupName: '', scan: [] },
      inSrc: Math.max(0, SRC.indexOf(g.source)), usbRole: g.usb === 'host' ? 1 : 0, usbMidi: g.usbmidi === 'off' ? 1 : 0,
      autoBar: Math.max(0, AUTOBAR.indexOf(g.autobar)),
      bar: barState ? { state: barState, conf: barLead === null ? 0 : clamp(barLead / BAR_CONF_FULL, 0, 1) } : null,
    };
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
  const WORDS = { insrc: SRC, autobar: AUTOBAR, lowconf: ['run', 'stop'], usbmidi: ['on', 'off'] };
  const KEYS = { insrc: 'source', autobar: 'autobar', lowconf: 'lowconf', usbmidi: 'usbmidi' };
  // an OLED edit (field, value as the OLED holds it) -> { commands: [...] } or { refuse: 'why' }
  function fieldCommands(field, val) {
    if (NO_STICK_SETTING[field]) return refuse(NO_STICK_SETTING[field]);
    if (field === 'threshold') return cmds('set threshold ' + clamp(Math.round(val), 0, 100));
    if (field === 'group') return cmds('set group ' + (val + 1));
    if (field === 'gain') return cmds('set line_gain ' + clamp(Math.round(val), -12, 12));
    if (field === 'inoff') return cmds('set lat_trim ' + clamp(Math.round(val), -100, 100));
    if (field === 'ltmode') return val === 1 ? refuse('FOLLOW is not on the control line: pick IGNORE or FOLLOW+LEAD') : cmds('set transport ' + (val === 0 ? 'auto' : 'link'));
    if (field === 'lowconf') return cmds('set lowconf ' + (val === 1 ? 'stop' : 'run'));
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
  // the ABOUT screen is shown: the page then asks the jank for `about` (and only then)
  const aboutOpen = m => !!m && m.ui.screen === 'info' && m.ui.page === 'about';
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
  // the screen of a layout: { fb, ink, w, h }. 'oled' (default): the 128 x 64 frame buffer, no ink. 'lcd2': the colour screen's own grid
  // (OLED.size('lcd2'), 320 x 240) with its ink plane (ui/lcd2.js paints them). Buffers are made here, so a caller never guesses a size.
  function screen(m, now, layout) {
    const lcd = layout === 'lcd2', size = OLED.size(layout), fb = new Uint8Array(size.w * size.h), ink = lcd ? new Uint8Array(size.w * size.h) : null;
    const c = m.getCtx(now);
    OLED.draw(fb, m.ui, lcd ? { ...c, layout } : c, now, ink);
    return { fb, ink, w: size.w, h: size.h };
  }
  const framebuffer = (m, now) => screen(m, now, 'oled').fb;                 // the OLED's 128 x 64

  return { KEY_BTN, KEY_HINT, REPEAT, PHASE, BEAT_DELAY_KEY, BEAT_DELAY_MAX, createPhase, phaseReport, phaseFeed, phaseAt, beatDelayEstimate, parseDelay, ctx, fieldCommands, tempoCommand, playCommand, nudgeCommand, create, aboutOpen, down, up, tick, releaseAll, screen, framebuffer };
})();
