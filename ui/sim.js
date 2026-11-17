/* A local device model for the control page's Demo mode: no network, no stick, no audio. It holds what the OLED module
   (ui/oled.js) reads as its ctx, applies the ctx actions to it, and turns button presses and releases into OLED.press
   calls the way the browser simulator (beat/live/player.html) does for a plain press: UP / DOWN act at once and repeat
   while held, NO / YES on HOME act on release, everywhere else at once. Not modelled: the HOME combos (NO or YES held with
   UP / DOWN), NO's triple click, the SETUP hotspot. The page has no beat tracker: in AUDIO mode there is no BPM, no beat and no
   confidence and no level (the screen shows the silent state the device shows with no beat yet). Time is always passed in (ms), so the model has no timers of its own. */
const Sim = (() => {
  const KEY_BTN = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'no', Escape: 'no', Backspace: 'no', ArrowRight: 'yes', Enter: 'yes' };
  const KEY_HINT = 'Keys: Up / Down = UP / DOWN, Left / Esc / Backspace = NO, Right / Enter = YES';
  const REPEAT = { firstMs: 400, everyMs: 120 };
  const TEMPO_RANGE = [90, 180], LINK_START_BPM = 120, SETUP_MODE = 3;
  const FLOOR_DB = -90, IDLE_MS = 500;
  const LED = { beatMs: 80, barMs: 150, flashCapDiv: 3, scale: 40, beat: [0, 30, 0], one: [40, 18, 0] };
  const SCAN = [{ name: 'HomeNet', bars: 3 }, { name: 'jank-studio', bars: 2 }, { name: 'Cafe Guest', bars: 1 }, { name: 'FritzBox 7590', bars: 2 }];
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const isUpDown = b => b === 'up' || b === 'down';

  const initialDev = () => ({
    link: false, hold: false, foldK: 1, linkBpm: LINK_START_BPM, th: 80, gain: 0, confStop: true, inOff: 0, offs: [0, 0, 0], bright: 100,
    sleep: 0, rate: 2, ltMode: 2, inSrc: 0, usbRole: 0, usbMidi: 0, autoBar: 1, bleMode: 1,
    net: { mode: 0, prev: 0, group: 1, ssid: 'HomeNet', pass: 'letmein42', appass: 'jankjank', apName: 'jank-a3f9' },
  });

  const FIELDS = {
    threshold: v => ({ th: v }), lowconf: v => ({ confStop: v === 1 }), ltmode: v => ({ ltMode: v }), gain: v => ({ gain: v }),
    bright: v => ({ bright: v }), sleep: v => ({ sleep: v }), inoff: v => ({ inOff: v }), rate: v => ({ rate: v }),
    insrc: v => ({ inSrc: v }), usbrole: v => ({ usbRole: v }), usbmidi: v => ({ usbMidi: v }), autobar: v => ({ autoBar: v }),
    ble: v => ({ bleMode: v }), mode: v => ({ link: v === 1, foldK: 1 }),
  };
  const OFFSET_FIELDS = { out0: 0, out1: 1, out2: 2 };
  const NET_FIELDS = { ssid: 'ssid', pass: 'pass', appass: 'appass', apname: 'apName' };

  // the value a setField(name, value) of the OLED module writes; SETUP (netmode 3) needs the hotspot, not modelled here
  function applyField(dev, name, value) {
    if (name in FIELDS) return { ...dev, ...FIELDS[name](value) };
    if (name in OFFSET_FIELDS) return { ...dev, offs: dev.offs.map((o, i) => (i === OFFSET_FIELDS[name] ? value : o)) };
    if (name in NET_FIELDS) return { ...dev, net: { ...dev.net, [NET_FIELDS[name]]: value } };
    if (name === 'group') return { ...dev, net: { ...dev.net, group: value + 1 } };
    if (name === 'netmode') return value === SETUP_MODE ? dev : { ...dev, net: { ...dev.net, prev: dev.net.mode, mode: value } };
    throw new Error('Sim: unknown field ' + name);
  }

  const beatClock = (bpm, now) => {
    const ms = 60000 / bpm, n = Math.floor(now / ms);
    return { n, ms: now - n * ms, periodMs: ms };
  };

  // LINK mode: the beat comes from Link's tempo (a setting). AUDIO mode: the page has no beat tracker, so no beat: the LEDs stay dark
  // (the device flashes them only inside a Link session)
  function ledColor(m, now) {
    if (!m.dev.link || !m.running()) return { index: -1, rgb: [0, 0, 0] };
    const { n, ms, periodMs } = beatClock(m.bpm(), now);
    const one = n % 4 === 0;
    const flash = Math.min(one ? LED.barMs : LED.beatMs, periodMs / LED.flashCapDiv);
    return { index: n % 4, rgb: ms < flash ? (one ? LED.one : LED.beat) : [0, 0, 0] };
  }
  const ledPaint = rgb => rgb.map(v => Math.round(255 * Math.pow(Math.min(1, v / LED.scale), 0.6)));

  function create() {
    const m = {
      ui: OLED.create(), dev: initialDev(), held: { up: false, down: false, no: false, yes: false },
      deferred: {}, combo: false, repeatAt: {},
    };
    m.bpm = () => (m.dev.link ? m.dev.linkBpm : null);
    m.running = () => m.dev.link && !m.dev.hold;                  // AUDIO mode has no tracker here: the outputs never run (no COAST label on a silent bar)
    m.onHome = () => m.ui.screen === 'home' && m.dev.net.mode !== SETUP_MODE;
    m.setField = (name, value) => { m.dev = applyField(m.dev, name, value); };
    m.tempoStep = d => {
      if (m.dev.link) { m.dev = { ...m.dev, linkBpm: clamp(Math.round(m.dev.linkBpm) + d, TEMPO_RANGE[0], TEMPO_RANGE[1]) }; return m.dev.linkBpm; }
      m.dev = { ...m.dev, foldK: clamp(m.dev.foldK * (d > 0 ? 2 : 0.5), 0.25, 4) };
      return null;                                                  // no tracker, no tempo to show
    };
    m.setHold = hold => { m.dev = { ...m.dev, hold }; };
    m.linkStartStop = run => m.setHold(!run);
    m.ctx = now => {
      const d = m.dev, link = d.link;
      const clock = link ? beatClock(m.bpm(), now) : null;
      return {
        link, bpm: null, linkBpm: d.linkBpm, mode: 'steady', conf: 0, th: d.th, gain: d.gain, confStop: d.confStop,
        levelDb: FLOOR_DB, levelMin: FLOOR_DB, levelMax: FLOOR_DB, silent: true, gated: false, hold: d.hold, playing: m.running(), running: m.running(),
        armed: false, ltMode: d.ltMode, xiao: false, fold: d.foldK, foldAuto: false, beatN: link && m.running() ? clock.n : null, beatMs: clock ? clock.ms : 0,
        peers: 0, usb: true, fitted: false, batt: 100, lineIn: !link, bright: d.bright, sleep: d.sleep, inOff: d.inOff, offs: d.offs,
        rate: d.rate, boot: false, splash: false, wifi: 'good', wifiQ: null, groupPeers: 3, setup: false, saved: false,
        net: { ...d.net, status: 'CONNECTED', ip: '192.168.1.42', apClients: 1, setupName: 'jank-setup', scan: SCAN },
        ble: { mode: d.bleMode, left: null, phone: false }, inSrc: d.inSrc, usbRole: d.usbRole, usbMidi: d.usbMidi, autoBar: d.autoBar,
        bar: null, tempoStep: m.tempoStep, setField: m.setField, setHold: m.setHold, linkStartStop: m.linkStartStop,
      };
    };
    return m;
  }

  const press = (m, b, now) => OLED.press(m.ui, b, now, m.ctx(now));

  function down(m, b, now) {
    if (m.held[b]) return;
    m.held[b] = true;
    if (!m.onHome()) { press(m, b, now); if (isUpDown(b)) m.repeatAt[b] = now + REPEAT.firstMs; return; }
    if (isUpDown(b)) { if (m.held.no || m.held.yes) m.combo = true; else { press(m, b, now); m.repeatAt[b] = now + REPEAT.firstMs; } return; }
    m.deferred[b] = true;
    m.combo = false;
  }

  function up(m, b, now) {
    if (!m.held[b]) return;
    m.held[b] = false;
    delete m.repeatAt[b];
    if (!m.deferred[b]) return;
    m.deferred[b] = false;
    if (m.onHome() && !m.combo) press(m, b, now);
  }

  // the auto-repeat of a held UP / DOWN: every repeat that came due by `now`
  function tick(m, now) {
    for (const b of Object.keys(m.repeatAt)) {
      while (m.repeatAt[b] <= now) { press(m, b, m.repeatAt[b]); m.repeatAt[b] += REPEAT.everyMs; }
    }
  }

  const releaseAll = (m, now) => { for (const b of Object.keys(m.held)) up(m, b, now); };
  const busy = (m, now) => Object.keys(m.repeatAt).length > 0 || m.ui.screen !== 'home' || !!(m.ui.overlay && now < m.ui.overlay.until);
  // ms to the next redraw that is not driven by a press: a beat edge in LINK mode, else a slow refresh
  const nextBeatInMs = (m, now) => { if (!m.dev.link) return IDLE_MS; const { ms, periodMs } = beatClock(m.bpm(), now); return periodMs - ms; };
  const framebuffer = (m, now) => { const fb = new Uint8Array(OLED.W * OLED.H); OLED.draw(fb, m.ui, m.ctx(now), now); return fb; };
  // the 4 beat LEDs as css colours (null = dark)
  const ledCss = (m, now) => {
    const { index, rgb } = ledColor(m, now), lit = rgb.some(v => v > 0);
    return [0, 1, 2, 3].map(i => { if (!lit || i !== index) return null; const c = ledPaint(rgb); return `rgb(${c[0]},${c[1]},${c[2]})`; });
  };

  return { KEY_BTN, KEY_HINT, REPEAT, create, down, up, tick, releaseAll, busy, nextBeatInMs, framebuffer, ledCss };
})();
