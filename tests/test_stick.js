// SPDX-License-Identifier: MIT
// The face of a connected stick (ui/stick.js) on transcripts of the firmware's own control line (tests/stick_ref/transcript.txt, made
// by running core/jank_ctl.c; home lines of a playing session from tests/test_phone.c): the OLED ctx from the stick's state, the
// smooth beat phase, the commands the face sends, and the D-pad. Exit status: non-zero on a failure.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const root = path.join(__dirname, '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const { OLED, Stick } = new Function(read('ui/oled.js') + read('ui/stick.js') + '; return { OLED, Stick };')();
const page = read('page/jank.html');
const logic = {};
vm.createContext(logic);
vm.runInContext(page.match(/<script id="logic">([\s\S]*?)<\/script>/)[1] + '; this.parseHome = parseHome; this.parseAnswer = parseAnswer;', logic);

let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };

// ---- the firmware's replies ----
const sections = {};
for (const block of read('tests/stick_ref/transcript.txt').split('### ').slice(1)) {
  const [cmd, ...rest] = block.split('\n');
  sections[cmd] = rest.filter(Boolean);
}
const answer = cmd => logic.parseAnswer(sections[cmd]).values;
const get = answer('get'), status = answer('status');
eq([get.group, get.transport, get.lowconf, get.threshold, get.source, get.ble, get.usbmidi, get.autobar, get.pass, get.net], ['1', 'link', 'stop', '80', 'auto', 'pair', 'on', 'start', 'none', 'jank'], 'the get transcript has the settings the face reads');
eq([status.hold, status.outputs, status.bar, status.gated, status.fold], ['0', 'stopped', 'off', '0', '1'], 'the status transcript has the numbers the face reads');
eq(sections['fold x3'], ['err fold x2, /2 or 1'], 'a refused command answers err <why>');
const idleHome = logic.parseHome(sections.home[0].replace('home=', ''));
ok(idleHome && idleHome.bpm === 0 && idleHome.audio === false, 'the idle home line of the firmware parses');

const home = line => logic.parseHome(line);
const LINK = home('h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 0');
const AUDIO = home('h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 1');
const state = (over = {}) => ({ home: AUDIO, homeAt: 1000, get, status: null, statusAt: 0, beat: null, overBle: false, ...over });

// ---- no data yet: the silent screen, nothing made up ----
const none = Stick.ctx({ home: null, get: {}, status: null }, 5000);
eq([none.bpm, none.linkBpm, none.conf, none.levelDb, none.levelMin, none.levelMax, none.silent, none.beatN, none.bar, none.running, none.armed, none.peers, none.fitted],
   [null, null, 0, -99, -99, -99, true, null, null, false, false, 0, false], 'no HOME line yet: no tempo, no level, no beat, nothing running');
const idle = Stick.ctx(state({ home: idleHome }), 1100);
eq([idle.bpm, idle.linkBpm, idle.levelDb, idle.beatN, idle.running, idle.fitted], [null, null, -99, null, false, true], 'the stick says nothing playing: the same');
const stale = Stick.ctx(state(), 1000 + 1501);
eq([stale.bpm, stale.levelDb, stale.running], [null, -99, false], 'a HOME line older than 1.5 s over USB is not shown');
eq(Stick.ctx(state({ overBle: true }), 1000 + 4000).bpm, 120, 'over Bluetooth a line is good for 5 s (the push comes every 2 s)');

// ---- AUDIO and LINK mode from the HOME line ----
const a = Stick.ctx(state(), 1000);
eq([a.link, a.bpm, a.linkBpm, a.conf, a.levelDb, a.levelMin, a.levelMax, a.running, a.armed, a.hold, a.peers, a.groupPeers, a.fitted, a.batt, a.fold, a.xiao],
   [false, 120, null, 0.87, -24, -31, -18, true, false, false, 2, 0, true, 64, 1, true], 'AUDIO: the tracker tempo, the meter, the transport, the peers, the battery');
const l = Stick.ctx(state({ home: LINK }), 1000);
eq([l.link, l.bpm, l.linkBpm, l.running], [true, null, 120, true], "LINK: the session's tempo, no tracker tempo");
eq(Stick.ctx(state({ home: home('h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 x2') }), 1000).fold, 2, 'the fold from the mode token');
eq(Stick.ctx(state({ home: home('h 6400 j3 1 275 4 2 0 64 -24 87 -31 -18 /2') }), 1000).fold, 0.5, '/2');
eq(Stick.ctx(state({ home: home('h 12000 j3 0 275 4 2 0 64 -24 87 -31 -18 1') }), 1000).hold, true, 'transport stopped in AUDIO mode: held');
const armed = Stick.ctx(state({ home: home('h 12000 j3 2 275 4 2 0 64 -24 87 -31 -18 1') }), 1000);
eq([armed.armed, armed.running], [true, false], 'transport armed: the blinking triangle, not running');
const hold = Stick.ctx(state({ home: home('h 12000 j3 1 275 4 2 0 - -24 87h -31 -18 1') }), 1000);
eq([hold.confHold, hold.conf, hold.fitted], [true, 0.87, false], 'conf "87h": holding; battery - : no gauge');
eq([Stick.ctx(state({ home: home('h 12000 w0 1 275 4 2 0 64 -24 87 -31 -18 1') }), 1000).wifi, Stick.ctx(state({ home: home('h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 1') }), 1000).wifi], ['none', 'unknown'], 'Wi-Fi looking: no signal; nothing measured: unknown, not made up');

// ---- the beat boxes follow the session's bar position ----
eq(Stick.ctx(state(), 1000).beatN, 2, 'bar 2.75: beat box 2');
eq(Stick.ctx(state(), 1000 + 250).beatN, 3, '+250 ms at 120 BPM: 3.25');
eq(Stick.ctx(state(), 1000 + 1250).beatN, 5, '+1250 ms: 5.25 (the OLED takes it modulo 4)');
eq(Stick.ctx(state({ home: home('h 12000 j3 0 275 4 2 0 64 -24 87 -31 -18 1') }), 5000).beatN, null, 'held: no beat box lit');

// ---- the settings from `get`, the gate and the bar from `status` ----
eq([a.th, a.confStop, a.ltMode, a.inSrc, a.ble.mode, a.usbMidi, a.autoBar, a.net.group, a.net.mode, a.gain, a.inOff], [80, true, 2, 0, 0, 0, 1, 1, 0, 0, 0], 'get: threshold, lowconf, transport, source, ble, usbmidi, autobar, group, net');
const g2 = { ...get, lowconf: 'run', transport: 'auto', source: 'usb', ble: 'off', usbmidi: 'off', autobar: 'always', net: 'wifi', ssid: 'HomeNet', pass: 'set', line_gain: '3', lat_trim: '-4', usb: 'host', threshold: '55' };
const b = Stick.ctx(state({ get: g2 }), 1000);
eq([b.th, b.confStop, b.ltMode, b.inSrc, b.ble.mode, b.usbMidi, b.autoBar, b.net.mode, b.net.ssid, b.net.pass, b.gain, b.inOff, b.usbRole], [55, false, 0, 2, 2, 1, 2, 1, 'HomeNet', '********', 3, -4, 1], 'get: the other values');
const st = Stick.ctx(state({ status: { ...status, gated: '1', bar: 'locked', bar_lead: '1.00', ble_state: 'window', ble_left: '95', link_ip: '10.0.0.5', fold_manual: '0' }, statusAt: 1000 }), 1500);
eq([st.gated, st.bar, st.ble.left, st.ble.phone, st.net.ip], [true, { state: 'locked', conf: 0.5 }, 95, false, '10.0.0.5'], 'status: the gate, the bar estimator, the Bluetooth window, the address');
eq(Stick.ctx(state({ status: { ...status, ble_state: 'connected' }, statusAt: 1000 }), 1500).ble.phone, true, 'status: a phone connected');
eq(Stick.ctx(state({ status: { ...status, gated: '1' }, statusAt: 0 }), 9000).gated, false, 'a status older than 5 s is not used');


// ---- the commands ----
const F = Stick.fieldCommands;
eq([F('threshold', 55), F('threshold', 200), F('group', 1), F('gain', 3), F('gain', 50), F('inoff', -4)].map(r => r.commands[0]),
   ['set threshold 55', 'set threshold 100', 'set group 2', 'set line_gain 3', 'set line_gain 12', 'set lat_trim -4'], 'numbers: clamped, the group index is 0-based');
eq([0, 1, 2, 3].map(v => F('insrc', v).commands[0]), ['set source auto', 'set source line', 'set source usb', 'set source link'], 'SOURCE');
eq([F('lowconf', 0).commands[0], F('lowconf', 1).commands[0]], ['set lowconf run', 'set lowconf stop'], 'LOW CONFIDENCE: KEEP RUNNING / STOP START');
eq([0, 1, 2].map(v => F('autobar', v).commands[0]), ['set autobar off', 'set autobar start', 'set autobar always'], 'AUTO BAR');
eq([F('usbmidi', 0).commands, F('usbmidi', 1).commands], [['set usbmidi on'], ['set usbmidi off']], 'USB MIDI CLOCK');
eq([F('ble', 0).commands, F('ble', 1).commands, F('ble', 2).commands], [['set ble pair', 'ble pair'], ['set ble auto'], ['set ble off']], 'BLUETOOTH: PAIR NOW also opens the window');
eq([F('ltmode', 0).commands, F('ltmode', 2).commands], [['set transport auto'], ['set transport link']], 'LINK TRANSPORT: IGNORE / FOLLOW+LEAD');
ok(/FOLLOW is not on the control line/.test(F('ltmode', 1).refuse), 'FOLLOW has no control-line value: refused with the reason');
for (const f of ['out0', 'out1', 'out2', 'rate', 'bright', 'sleep', 'ssid', 'pass', 'apname', 'appass', 'netmode', 'usbrole']) ok(typeof F(f, 0).refuse === 'string' && F(f, 0).refuse.length > 10 && !F(f, 0).commands, f + ' is refused with a reason, nothing sent');
ok(!!F('nonsense', 1).refuse, 'an unknown field is refused, not ignored');
const aud = Stick.ctx(state(), 1000), lnk = Stick.ctx(state({ home: LINK }), 1000);
eq(Stick.tempoCommand(aud, 1), { commands: ['fold x2'], expected: 240 }, 'AUDIO UP: fold x2, the tempo expected doubled');
eq(Stick.tempoCommand(aud, -1), { commands: ['fold /2'], expected: 60 }, 'AUDIO DOWN: fold /2');
eq(Stick.tempoCommand(lnk, 1), { commands: ['tempo 121'], expected: 121 }, 'LINK UP: tempo +1');
eq(Stick.tempoCommand(lnk, -1).commands, ['tempo 119'], 'LINK DOWN');
eq(Stick.tempoCommand({ ...lnk, linkBpm: 999 }, 1).commands, ['tempo 999'], 'LINK: stops at 999');
eq(Stick.tempoCommand({ ...lnk, linkBpm: null }, 1).refuse, 'no Link session yet: no tempo to change', 'LINK without a session: refused');
eq(Stick.tempoCommand({ ...aud, bpm: null }, 1), { commands: ['fold x2'], expected: null }, 'AUDIO with no tempo known: the fold is sent, no value promised');
eq([Stick.playCommand(true), Stick.playCommand(false)], ['play', 'stop'], 'YES: play / stop');
eq([Stick.nudgeCommand(aud, 'no', 1).commands[0], Stick.nudgeCommand(aud, 'no', -1).commands[0], Stick.nudgeCommand(aud, 'yes', 1).commands[0], Stick.nudgeCommand(aud, 'yes', -1).commands[0]],
   ['nudge beat +0.5', 'nudge beat -0.5', 'nudge ms +1', 'nudge ms -1'], 'NO / YES held + UP / DOWN: the nudges');
ok(/AUDIO mode/.test(Stick.nudgeCommand(lnk, 'no', 1).refuse), 'the nudges are refused in LINK mode');

// ---- the D-pad on a face whose actions are recorded ----
function face(over = {}) {
  const sent = [], combos = [];
  const m = Stick.create(now => ({ ...Stick.ctx(state(over), 1000 + now % 1), tempoStep: d => { sent.push(['tempo', d]); return null; }, setField: (f, v) => sent.push(['field', f, v]),
                                  setHold: () => {}, linkStartStop: on => sent.push(['play', on]) }), (mod, d) => combos.push([mod, d]));
  return { m, sent, combos };
}
const click = (m, b, t) => { Stick.down(m, b, t); Stick.up(m, b, t + 50); };
let f = face();
Stick.down(f.m, 'no', 0);
eq(f.m.ui.screen, 'home', 'NO on HOME acts on release, not on press');
Stick.up(f.m, 'no', 50);
eq(f.m.ui.screen, 'menu', 'NO released: the MENU');
ok(Stick.framebuffer(f.m, 100).some(v => v === 1), 'the screen draws');
const c0 = f.m.ui.cursor;
click(f.m, 'down', 100);
eq(f.m.ui.cursor, c0 + 1, 'DOWN moves the cursor (locally)');
click(f.m, 'up', 150);
eq(f.m.ui.cursor, c0, 'UP moves it back');
click(f.m, 'yes', 200);
eq(f.m.ui.screen, 'sub', 'YES enters AUDIO');
click(f.m, 'yes', 300);
eq(f.m.ui.screen, 'edit', 'YES enters the first row (SOURCE)');
click(f.m, 'up', 400);
eq(f.sent.at(-1), ['field', 'insrc', 1], 'a live edit step goes to the stick through setField');
click(f.m, 'no', 500);
eq(f.sent.at(-1), ['field', 'insrc', 0], 'NO puts the old value back (also a command)');
f = face();
click(f.m, 'up', 0);
eq(f.sent, [['tempo', 1]], 'UP on HOME: the tempo step (the fold in AUDIO)');
click(f.m, 'yes', 200);
eq(f.sent.at(-1), ['play', false], 'YES on HOME, running: stop');
f = face({ home: home('h 12000 j3 0 275 4 2 0 64 -24 87 -31 -18 1') });
click(f.m, 'yes', 0);
eq(f.sent.at(-1), ['play', true], 'YES on HOME, held: play');
f = face();
Stick.down(f.m, 'up', 0);
Stick.tick(f.m, Stick.REPEAT.firstMs - 1);
eq(f.sent.length, 1, 'no repeat before the first delay');
Stick.tick(f.m, Stick.REPEAT.firstMs + Stick.REPEAT.everyMs);
eq(f.sent.length, 3, 'UP held on HOME repeats');
Stick.up(f.m, 'up', 2000);
Stick.tick(f.m, 9000);
eq(f.sent.length, 3, 'released: it stops');
f = face();
Stick.down(f.m, 'no', 0);
Stick.down(f.m, 'up', 10);
Stick.up(f.m, 'up', 20);
Stick.up(f.m, 'no', 30);
eq([f.combos, f.m.ui.screen, f.sent], [[['no', 1]], 'home', []], 'NO held + UP: the nudge, not a tempo step, and the release does not open the MENU');
Stick.down(f.m, 'yes', 100);
Stick.down(f.m, 'down', 110);
Stick.up(f.m, 'down', 120);
Stick.up(f.m, 'yes', 130);
eq([f.combos.at(-1), f.sent], [['yes', -1], []], 'YES held + DOWN: the audio shift, no play / stop');
Stick.down(f.m, 'up', 200);
Stick.releaseAll(f.m, 210);
eq(Object.keys(f.m.repeatAt).length, 0, 'releaseAll stops the repeats');
eq(Stick.KEY_BTN, { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'no', Escape: 'no', Backspace: 'no', ArrowRight: 'yes', Enter: 'yes' }, 'the keyboard mapping is the Mac simulator\'s');


// ---- the smooth local phase, on a fake clock ----
{
  const rep = (bar, bpm = 120, running = true) => ({ bar, bpm, running, quantum: 4 });
  const beatsAt = (bpm, ms) => ms * bpm / 60000;
  let ph = Stick.phaseFeed(Stick.createPhase(), rep(2.75), 1000);
  eq(Stick.phaseAt(ph, 1000, 0), 2.75, 'the first report is taken as it is');
  eq(Stick.phaseAt(ph, 1250, 0), 3.25, 'between reports the position runs on at the tempo (0.5 beat in 250 ms at 120)');
  // a steady tempo: exact reports every 300 ms keep the position exact and never go backwards
  ph = Stick.phaseFeed(Stick.createPhase(), rep(0), 0);
  let worst = 0, last = 0;
  for (let t = 300; t <= 6000; t += 300) {
    ph = Stick.phaseFeed(ph, rep(beatsAt(120, t) % 4), t);
    worst = Math.max(worst, Math.abs(Stick.phaseAt(ph, t, 0) - beatsAt(120, t)));
    ok(Stick.phaseAt(ph, t, 0) >= last, 'never backwards at ' + t);
    last = Stick.phaseAt(ph, t, 0);
  }
  ok(worst < 1e-9, 'steady tempo: the shown position follows the truth to 1e-9 beat (' + worst + ')');
  // a tempo change: the new tempo is taken at once, the position carries on from where it was
  ph = Stick.phaseFeed(Stick.createPhase(), rep(0), 0);
  ph = Stick.phaseFeed(ph, rep(0.6), 300);
  const before = Stick.phaseAt(ph, 300, 0);
  ph = Stick.phaseFeed(ph, rep(0.6 + beatsAt(120, 300) + 0.01, 128), 600);
  ok(Math.abs(Stick.phaseAt(ph, 600, 0) - (0.6 + beatsAt(120, 300))) < 0.01, 'a tempo change: the position stays close (a quarter of a 0.01 beat error taken)');
  eq(Math.round((Stick.phaseAt(ph, 1600, 0) - Stick.phaseAt(ph, 600, 0)) * 1e6) / 1e6, Math.round(beatsAt(128, 1000) * 1e6) / 1e6, 'and from then on runs at 128 BPM');
  ok(before < Stick.phaseAt(ph, 600, 0), 'forward all the way');
  // a late report: it says we are 0.24 beat behind the truth... reported late means its position is OLD: it asks for a step back. At most 0.02.
  ph = Stick.phaseFeed(Stick.createPhase(), rep(1), 0);
  const predicted = Stick.phaseAt(ph, 600, 0);
  ph = Stick.phaseFeed(ph, rep(1 + beatsAt(120, 480)), 600);                  // the same line, 120 ms late: its position is 0.24 beat behind
  ok(Stick.phaseAt(ph, 600, 0) >= predicted - Stick.PHASE.backBeats - 1e-9, 'a late report moves the boxes back by 0.02 beat at most (' + (predicted - Stick.phaseAt(ph, 600, 0)).toFixed(4) + ')');
  ok(Stick.phaseAt(ph, 600, 0) < predicted, 'but it does move toward the report');
  // an early report is taken forward by a quarter of the error
  ph = Stick.phaseFeed(Stick.createPhase(), rep(1), 0);
  ph = Stick.phaseFeed(ph, rep(1 + beatsAt(120, 300) + 0.2), 300);
  ok(Math.abs(Stick.phaseAt(ph, 300, 0) - (1 + beatsAt(120, 300) + 0.05)) < 1e-9, 'a report ahead of us pulls forward by a quarter of the error');
  // a jump of more than half a beat snaps (a new lock, a bar moved)
  ph = Stick.phaseFeed(ph, rep(3.9), 600);
  eq(Stick.phaseAt(ph, 600, 0), 3.9, 'a jump over half a beat snaps');
  ph = Stick.phaseFeed(Stick.phaseFeed(Stick.createPhase(), rep(3.9), 0), rep(0.1), 100);
  ok(Math.abs(Stick.phaseAt(ph, 100, 0) - 3.9 - beatsAt(120, 100) - 0.25 * 0.2 + 0.0) < 0.3, 'the bar wrap (3.9 to 0.1) is a small step, not a jump');
  // stopped holds the position
  ph = Stick.phaseFeed(Stick.phaseFeed(Stick.createPhase(), rep(1), 0), rep(1.5, 120, false), 300);
  eq([Stick.phaseAt(ph, 300, 0), Stick.phaseAt(ph, 1300, 0)], [1.5, 1.5], 'stopped: the position holds');
  // stale: no running on old data past 1.5 s; a lost lock clears at once
  ph = Stick.phaseFeed(Stick.createPhase(), rep(1), 0);
  eq([Stick.phaseAt(ph, 1500, 0) !== null, Stick.phaseAt(ph, 1501, 0)], [true, null], 'no report for 1.5 s: the boxes go empty, not on running on stale data');
  eq(Stick.phaseAt(Stick.phaseFeed(ph, null, 200), 200, 0), null, 'a report with no session or tempo (lost lock): empty at once');
  eq(Stick.phaseReport({ bpm: 0, bar: null, quantum: 4, transport: 'stopped' }), null, 'no tempo, no bar: no report');
  eq(Stick.phaseReport({ bpm: 120, bar: 275, quantum: 4, transport: 'playing' }), { bar: 2.75, bpm: 120, running: true, quantum: 4 }, 'the HOME line as a report');
  // the display delay: + shows the position of that long ago, - runs ahead
  ph = Stick.phaseFeed(Stick.createPhase(), rep(2), 1000);
  eq([Stick.phaseAt(ph, 1200, 0), Stick.phaseAt(ph, 1200, 100), Stick.phaseAt(ph, 1200, -100)], [2.4, 2.2, 2.6], 'delay +100 ms: 0.2 beat behind; -100 ms: 0.2 ahead');
  // in the ctx
  const base = { home: AUDIO, homeAt: 1000, get, status: null, statusAt: 0, overBle: false };
  const withPhase = (delayMs, now) => { const st2 = { ...base, phase: Stick.phaseFeed(Stick.createPhase(), { bar: 2.75, bpm: 120, running: true, quantum: 4 }, 1000), delayMs }; return Stick.ctx(st2, now).beatN; };
  eq(withPhase(0, 1250), 3, 'ctx: the box from the smooth phase');
  eq(withPhase(250, 1250), 2, 'ctx: delayed 250 ms: still the previous box');
  eq(withPhase(-250, 1000), 3, 'ctx: advanced 250 ms: the next box already');
  eq(withPhase(0, 1000 + 1501), null, 'the stale phase draws no box');
  // the estimate of the display delay and its storage
  eq([Stick.beatDelayEstimate({ monitorMs: 30, stickMs: 50 }), Stick.beatDelayEstimate({ monitorMs: 96, stickMs: 20 }), Stick.beatDelayEstimate({ monitorMs: null, stickMs: 50 }), Stick.beatDelayEstimate({ monitorMs: 900, stickMs: 0 }), Stick.beatDelayEstimate({ monitorMs: 0, stickMs: 900 })],
     [-20, 75, 0, 400, -400], 'the estimate: monitor minus stick latency to 5 ms, 0 when one is unknown, within +-400');
  eq([Stick.beatDelayEstimate({ monitorMs: 30, stickMs: 50, beatLatencyMs: -20 }), Stick.beatDelayEstimate({ monitorMs: 30, stickMs: 50, beatLatencyMs: 25 }), Stick.beatDelayEstimate({ monitorMs: 30, stickMs: 50, beatLatencyMs: null })],
     [0, -45, -20], 'the estimate leaves out what the stick already corrects (its beat_latency): 0 once Auto latency has synced');
  eq([Stick.parseDelay('120'), Stick.parseDelay('-7'), Stick.parseDelay('900'), Stick.parseDelay(''), Stick.parseDelay(null), Stick.parseDelay('x')], [120, -5, 400, null, null, null], 'a stored delay: rounded to 5, limited, or none');
}

console.log('stick: ' + checks + ' checks passed');
