// node xiao_m2b.js PLAYER_HTML: --xiao against the firmware M2b (fw/integ-2): the sound flag, the outputs, `home`'s meter numbers,
// the Bluetooth window from `status`, and the source / hold words in `status` and `get`
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
const konst = name => html.match(new RegExp('const ' + name + ' = ([0-9.-]+);'))[0];
const consts = ['XIAO_STATUS_FRESH_MS', 'XIAO_PLAY_WAIT_MS', 'XIAO_HOME_FRESH_MS', 'XIAO_FLOOR_DB', 'BLE_WINDOW_S', 'XIAO_REPORT_FRESH_MS'].map(konst).join('\n');
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const fns = ['xiaoOutputs', 'homeMode', 'parseHome', 'foldNow', 'foldNext', 'autoBarFrom', 'xiaoMeters', 'meterNumbers', 'bleNow', 'beatStats'];
const AUTOBAR = 'const AUTOBAR_WORDS = ["off", "start", "always"];\n';
const P = new Function(AUTOBAR + consts + '\n' + fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

// the clock outputs: status outputs= / hold=, with a play / stop the page sent and no status has answered yet
const st = (o = {}) => Object.assign({ outputs: 'stopped', hold: 0 }, o);
eq(P.xiaoOutputs(null, 0, 1000, null), null);
eq(P.xiaoOutputs(st(), 1000, 6001, null), null);                                   // older than 5 s
eq(P.xiaoOutputs({ link_peers: 1 }, 1000, 1500, null), null);                      // an older firmware: no outputs=
eq(P.xiaoOutputs(st({ outputs: 'running' }), 1000, 1500, null), { running: true, armed: false });
eq(P.xiaoOutputs(st({ outputs: 'armed' }), 1000, 1500, null), { running: false, armed: true });
eq(P.xiaoOutputs(st(), 1000, 1500, null), { running: false, armed: false });
eq(P.xiaoOutputs(st({ outputs: 'running', hold: 1 }), 1000, 1500, null), { running: false, armed: false });   // held: stopped
eq(P.xiaoOutputs(st(), 1000, 1500, { cmd: 'play', at: 1200 }), { running: false, armed: true });              // sent, not answered: armed
eq(P.xiaoOutputs(st(), 3000, 3500, { cmd: 'play', at: 1200 }), { running: false, armed: false });             // a newer status answered
eq(P.xiaoOutputs(st(), 1000, 1200 + 2600, { cmd: 'play', at: 1200 }), { running: false, armed: false });      // 2.5 s: never stuck
eq(P.xiaoOutputs(st({ outputs: 'running' }), 1000, 1500, { cmd: 'stop', at: 1200 }), { running: false, armed: false });   // stop: at once
eq(P.xiaoOutputs(st({ outputs: 'running' }), 1000, 1500, { cmd: 'play', at: 1200 }), { running: true, armed: false });    // play while running: unchanged

// `home`
const H = 'home=h 12400 0 1 2 4 1 0 87 -23 64 -30 -12';
eq(P.parseHome(H), { bpm: 124, net: '0', transport: 1, bar: 2, quantum: 4, linkPeers: 1, airPeers: 0, batt: 87, level: -23, conf: 64, confHold: false, min: -30, max: -12, mode: null, fold: null });     // (an older firmware: no mode token)
const none = P.parseHome('home=h 0 0 0 0 4 0 0 - - - - -');
eq([none.batt, none.level, none.conf, none.min, none.max, none.bpm], [null, null, null, null, null, 0]);
eq(P.parseHome('ok'), null);
eq(P.parseHome('home=h 1 2 3'), null);                                              // cut short
eq(P.parseHome('bpm=120'), null);

// the meter's numbers: home (fresh) > status's rms_db > nothing; the floor when the XIAO says nothing
const home = P.parseHome(H);
eq(P.xiaoMeters(home, 1000, null, 0, 1500), { level: -23, min: -30, max: -12, conf: 0.64, hold: false });
eq(P.xiaoMeters(home, 1000, null, 0, 1000 + 1501), null);                           // home too old, no status
const stm = { rms_db: -40, rms_min_db: -50, rms_max_db: -35, conf: 25 };
eq(P.xiaoMeters(home, 1000, stm, 2400, 2600), { level: -40, min: -50, max: -35, conf: 0.25, hold: false });   // home old: the status's numbers
eq(P.xiaoMeters(null, 0, stm, 1000, 1500).level, -40);
eq(P.xiaoMeters(null, 0, stm, 1000, 6500), null);
eq(P.xiaoMeters(P.parseHome('home=h 0 0 0 0 4 0 0 - - - - -'), 1000, null, 0, 1200), { level: null, min: null, max: null, conf: 0, hold: false });
const local = { bar: -20, lo: -25, hi: -15, conf: 0.9 };
eq(P.meterNumbers(false, { level: -23, min: -30, max: -12, conf: 0.64 }, local), local);          // not --xiao: the page's own meter
eq(P.meterNumbers(true, { level: -23, min: -30, max: -12, conf: 0.64 }, local), { bar: -23, lo: -30, hi: -12, conf: 0.64, hold: false });
eq(P.meterNumbers(true, null, local), { bar: -99, lo: -99, hi: -99, conf: 0, hold: false });                    // nothing from the XIAO: no local numbers
eq(P.meterNumbers(true, { level: null, min: null, max: null, conf: 0 }, local), { bar: -99, lo: -99, hi: -99, conf: 0, hold: false });

// the status's words: source, Bluetooth, outputs; hold / audio / the meter are numbers; mode stays out
const b = P.beatStats(['audio=1', 'audio_live=0', 'hold=1', 'outputs=armed', 'audio_source=usb', 'ble_state=window', 'ble_left=97', 'rms_db=-41', 'conf=12', 'bar=locked', 'mode=steady', 'source=link', 'peers=2']);
eq([b.audio, b.audio_live, b.hold, b.outputs, b.audio_source, b.ble_state, b.ble_left, b.rms_db, b.conf, b.bar, b.mode, b.source, b.peers], [1, 0, 1, 'armed', 'usb', 'window', 97, -41, 12, 'locked', undefined, 'link', 2]);

// the pair window and the phone
const L = (end, phone = false) => [false, null, 0, 0, end, phone];
eq(P.bleNow(...L(0), 5000), { left: null, phone: false });                          // local, no window
eq(P.bleNow(...L(125000), 5000), { left: 120, phone: false });
eq(P.bleNow(...L(125000), 124500), { left: 0.5, phone: false });
eq(P.bleNow(...L(125000), 125000), { left: null, phone: false });                   // over
eq(P.bleNow(...L(0, true), 5000), { left: null, phone: true });
const X = (s, at, pairAt, now) => P.bleNow(true, s, at, pairAt, 0, true, now);      // (the panel's phone box does not count with --xiao)
eq(X({ ble_state: 'window', ble_left: 90 }, 1000, 0, 1000), { left: 90, phone: false });
eq(X({ ble_state: 'window', ble_left: 90 }, 1000, 0, 3000), { left: 88, phone: false });    // counts down between statuses
eq(X({ ble_state: 'connected' }, 1000, 0, 1500), { left: null, phone: true });
eq(X({ ble_state: 'off' }, 1000, 0, 1500), { left: null, phone: false });
eq(X(null, 0, 0, 1500), { left: null, phone: false });
eq(X({ ble_state: 'off' }, 1000, 1200, 1700), { left: 119.5, phone: false });        // `ble pair` sent, no status answered yet: open
eq(X({ ble_state: 'off' }, 1000, 1200, 1200 + 2600), { left: null, phone: false });  // 2.5 s: never stuck
eq(X({ ble_state: 'window', ble_left: 100 }, 2000, 1200, 2100), { left: 99.9, phone: false });   // answered: the status's own

// the control line: a `home` answer is kept (dated by its first frame), apart from `status` and `get`
{
  const XIAO = { text: '', lines: [], answerAt: 0, stats: null, status: null, statusAt: 0, cfg: null, err: '', errAt: 0, home: null, homeAt: 0 };
  const env = { XIAO, api: () => {}, beatStats: P.beatStats, parseHome: P.parseHome, performance: { now: () => 99 } };
  const onXiaoLineText = new Function(...Object.keys(env), grab('onXiaoLineText') + '; return onXiaoLineText;')(...Object.values(env));
  onXiaoLineText('home=h 12400 0 1 2 4 1 0 87 -23', 700);
  onXiaoLineText(' 64 -30 -12\nok\n', 730);
  eq([XIAO.home.level, XIAO.home.conf, XIAO.homeAt, XIAO.status, XIAO.cfg], [-23, 64, 700, null, null]);
  onXiaoLineText('link_peers=1\nbpm=120\nbeat=3.5\noutputs=running\nhold=0\nble_state=off\nok\n', 800);
  eq([XIAO.status.outputs, XIAO.status.hold, XIAO.status.ble_state, XIAO.statusAt], ['running', 0, 'off', 800]);
  onXiaoLineText('quantum=4\nsource=link\nok\n', 900);
  eq(XIAO.cfg.source, 'link');
  onXiaoLineText('ok\n', 950);                                                      // `ble pair`'s answer: nothing to keep
  eq([XIAO.home.level, XIAO.status.bpm], [-23, 120]);
}

// --xiao's mode follows the sound flag (live), not an open stream
{
  const boxes = { usbaudio: { checked: false }, linein: { checked: true } };
  const dev = { link: false };
  const set = [];
  const XIAO = { rep: null, repAt: 0 };
  const env = { $: id => boxes[id], XIAO, dev, XIAO_REPORT_FRESH_MS: 3000, setField: (f, v) => { set.push([f, v]); dev.link = v === 1; } };
  const syncXiaoMode = new Function(...Object.keys(env), grab('syncXiaoMode') + '; return syncXiaoMode;')(...Object.values(env));
  syncXiaoMode(1000);
  eq([boxes.linein.checked, boxes.usbaudio.disabled, set], [false, true, []]);       // no report: nothing decided
  XIAO.rep = { audio: true, live: false }; XIAO.repAt = 900;                         // the stream is open, no sound: LINK
  syncXiaoMode(1000);
  eq([set, boxes.usbaudio.checked], [[['mode', 1]], false]);
  XIAO.rep = { audio: true, live: true };                                            // sound: AUDIO
  syncXiaoMode(1100);
  eq([set, boxes.usbaudio.checked], [[['mode', 1], ['mode', 0]], true]);
  syncXiaoMode(1200);                                                                // unchanged: nothing more sent
  eq(set.length, 2);
  XIAO.rep = { audio: false, live: false };
  syncXiaoMode(1300);
  eq(set[2], ['mode', 1]);
  syncXiaoMode(1300 + 3001);                                                         // a stale report: kept
  eq(set.length, 3);
}
console.log('xiao M2b page helpers: ' + checks + ' checks passed');
