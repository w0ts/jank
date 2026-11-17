// SPDX-License-Identifier: MIT
// The control page's connection, with a fake DOM and a fake MIDI transport that answers with the firmware's own replies
// (tests/stick_ref/transcript.txt): before a connection no button is pressed and the face, the player and the Advanced extra are
// hidden and the keys do nothing; a failed connect says why and leaves it so; connecting shows the face, which draws the stick's live
// state and sends its commands through the page's one command function (UP in AUDIO sends `fold x2`, an error reply is shown); Advanced
// (USB only) shows the player and the extra info with the diag polled every 2 s; every way out hides it all again with the player
// stopped and released, no timer and no frame left, and no error thrown.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const root = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'page', 'jank.html'), 'utf8');
const lib = ['ui/oled.js', 'ui/face.js', 'ui/stick.js', 'ui/player.js'].map(f => fs.readFileSync(path.join(root, f), 'utf8')).join('\n') + '\nthis.OLED = OLED; this.Face = Face; this.Stick = Stick; this.Player = Player;';
const script = id => html.match(new RegExp('<script id="' + id + '">([\\s\\S]*?)</script>'))[1];
const transcript = {};
for (const block of fs.readFileSync(path.join(root, 'tests', 'stick_ref', 'transcript.txt'), 'utf8').split('### ').slice(1)) {
  const [cmd, ...rest] = block.split('\n');
  transcript[cmd] = rest.filter(Boolean).join('\n');
}

function makeElement() {
  const listeners = {}, attrs = {}, classes = new Set(), el = {};
  Object.assign(el, {
    textContent: '', value: '', disabled: false, checked: false, children: [], style: {}, dataset: {}, options: [], tagName: 'DIV', className: '',
    classList: { add: c => classes.add(c), remove: c => classes.delete(c), contains: c => classes.has(c), toggle: (c, on) => { (on === undefined ? !classes.has(c) : on) ? classes.add(c) : classes.delete(c); return classes.has(c); } },
    setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null),
    addEventListener: (type, fn) => { (listeners[type] = listeners[type] || []).push(fn); },
    click: () => (listeners.click || []).forEach(fn => fn({ target: el })),
    fire: (type, ev) => (listeners[type] || []).forEach(fn => fn(ev)), contains: () => false,
    append: () => {}, add: () => {}, focus: () => {}, remove: () => {}, replaceChildren: (...kids) => { el.children = kids; },
    closest: () => null, cloneNode: () => makeElement(), setPointerCapture: () => {}, selectedOptions: [{ text: '' }],
    getContext: () => ({ createImageData: () => ({ data: new Uint8ClampedArray(128 * 64 * 4) }), putImageData(img) { el.lastImage = img; } }),
  });
  const kids = new Map();
  el.querySelector = sel => { if (!kids.has(sel)) kids.set(sel, makeElement()); return kids.get(sel); };
  el.firstElementChild = makeElement.leaf ? null : null;
  Object.defineProperty(el, 'firstElementChild', { get: () => el.querySelector('first') });
  return el;
}

function makeAudio(media) {
  const audio = makeElement();
  Object.assign(audio, { tagName: 'AUDIO', paused: true, ended: false, currentTime: 0, duration: NaN, volume: 1, src: '', pause() { audio.paused = true; media.paused++; }, load() {}, removeAttribute() { audio.src = ''; }, play() { audio.paused = false; return Promise.resolve(); } });
  return audio;
}

function makePage(opts = {}) {
  const elements = new Map(), timers = new Set(), frames = new Set();
  let nextTimer = 1, nextFrame = 1;
  const bus = {}, winBus = {}, media = { paused: 0, revoked: [] }, clock = { t: 1000 };
  const document = {
    getElementById: id => { if (!elements.has(id)) elements.set(id, id === 'pl-audio' ? makeAudio(media) : makeElement()); return elements.get(id); },
    addEventListener: (type, fn) => { (bus[type] = bus[type] || []).push(fn); },
    dispatchEvent: ev => { (bus[ev.type] || []).forEach(fn => fn(ev)); return true; },
    createElement: () => makeElement(), createElementNS: () => makeElement(), activeElement: null,
  };
  const setTimer = () => { const id = nextTimer++; timers.add(id); return id; };
  const box = {
    document, console, performance: { now: () => clock.t }, Option: class { constructor(text, value) { this.text = text; this.value = value; } },
    setInterval: (fn, ms) => { const id = setTimer(); box.__intervals = box.__intervals || new Map(); box.__intervals.set(id, fn); return id; }, setTimeout: setTimer,
    clearInterval: id => timers.delete(id), clearTimeout: id => timers.delete(id),
    requestAnimationFrame: fn => { const id = nextFrame++; frames.add({ id, fn }); return id; },
    cancelAnimationFrame: id => { for (const f of frames) if (f.id === id) frames.delete(f); },
    CustomEvent: class { constructor(type, init) { this.type = type; this.detail = init && init.detail; } }, navigator: { mediaDevices: opts.mediaDevices },
    localStorage: opts.storage || { getItem: () => null, setItem: () => {} },
    URL: { createObjectURL: f => 'blob:' + f.name, revokeObjectURL: u => media.revoked.push(u) },
    window: { addEventListener: (type, fn) => { (winBus[type] = winBus[type] || []).push(fn); } }, location: { search: '' }, URLSearchParams, TextDecoder, Uint8Array, Float32Array,
  };
  vm.createContext(box);
  vm.runInContext(script('logic'), box);
  vm.runInContext(script('ui'), box);
  vm.runInContext(lib, box);
  vm.runInContext(script('face-ui'), box);
  vm.runInContext(script('advanced-ui'), box);
  vm.runInContext(script('player-ui'), box);
  vm.runInContext('this.S = S; this.disconnected = disconnected;', box);
  const key = (type, k) => (winBus[type] || []).forEach(fn => fn({ key: k, target: { tagName: 'BODY' }, repeat: false, preventDefault() {} }));
  const runFrame = () => { const [f] = [...frames]; if (f) { frames.delete(f); f.fn(clock.t); } };
  return { box, el: id => document.getElementById(id), timers, frames, media, clock, key, runFrame, intervals: () => [...(box.__intervals || [])] };
}

// the fake stick: one MIDI port pair that answers each command line with the firmware's reply
function fakeMidi(replies = {}) {
  const sent = [], input = { id: 'in', name: 'jank', state: 'connected', onmidimessage: null };
  let assembling = '', page = null;
  const text = {
    ...transcript,
    home: 'home=h 12000 j3 1 275 4 2 0 64 -24 87 -31 -18 1\nok', 'fold x2': 'fold=2\nok', 'fold /2': 'fold=0.5\nok', play: 'ok', stop: 'ok',
    diag: 'boots=3\nreset=poweron\nsave_pending=0\nlink_settled=1\nlink_drift_ppb=12\nlink_last_err_us=3\nlink_far=0\nlink_partner_rtt_us=800\ndrive=ok\nok',
    ...replies,
  };
  const output = {
    id: 'out', name: 'jank', state: 'connected',
    send: frame => {
      assembling += page.box.sysexToText(frame);
      if (!assembling.endsWith('\n')) return;
      const command = assembling.trim();
      assembling = '';
      sent.push(command);
      const answer = (text[command] !== undefined ? text[command] : 'ok').replace(/^### .*\n/, '') + '\n';
      queueMicrotask(() => { for (const f of page.box.textToSysex(answer)) input.onmidimessage({ data: Uint8Array.from(f) }); });
    },
  };
  const access = { inputs: new Map([['in', input]]), outputs: new Map([['out', output]]), onstatechange: null };
  return { input, output, access, sent, text, attach: p => { page = p; } };
}
const shown = (page, id) => !page.el(id).classList.contains('hide');
const pressed = page => ['c-midi', 'c-bluetooth'].map(id => page.el(id).getAttribute('aria-pressed'));
const settle = async (n = 12) => { for (let i = 0; i < n; i++) await new Promise(resolve => setImmediate(resolve)); };
let checks = 0;
const errors = [];
process.on('uncaughtException', e => errors.push(e));
process.on('unhandledRejection', e => errors.push(e));
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };
const hidden = page => ['live', 'c-advanced-box', 'player', 'advanced-extra'].map(id => shown(page, id));

async function connected(replies, opts) {
  const page = makePage(opts), midi = fakeMidi(replies);
  midi.attach(page);
  page.box.navigator.requestMIDIAccess = () => Promise.resolve(midi.access);
  page.el('c-midi').click();
  await settle();
  return { page, midi };
}
const tap = (page, k) => { page.clock.t += 60; page.key('keydown', k); page.clock.t += 40; page.key('keyup', k); };

(async () => {
  // ---- before any connection ----
  let page = makePage();
  eq(pressed(page), ['false', 'false'], 'no button is pressed before a connection');
  eq(hidden(page), [false, false, false, false], 'the face, the Advanced box, the player and the Advanced extra are hidden');
  page.key('keydown', 'ArrowUp');
  eq([page.frames.size, page.timers.size], [0, 0], 'no face, no frame, no timer, and the keys do nothing');

  // ---- a failed connect: no MIDI at all / no port ----
  page = makePage();
  page.box.navigator.requestMIDIAccess = () => Promise.reject(new Error('nope'));
  page.el('c-midi').click();
  await settle();
  eq([pressed(page), hidden(page), page.el('msg').textContent, page.el('msg').classList.contains('bad')], [['false', 'false'], [false, false, false, false], 'nope', true], 'a failed connect: nothing shown, the reason shown');
  page = makePage();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve({ inputs: new Map(), outputs: new Map() });
  page.el('c-midi').click();
  await settle();
  assert.match(page.el('msg').textContent, /No MIDI port/); checks++;
  eq(pressed(page), ['false', 'false'], 'no port: nothing pressed');

  // ---- connected over USB: the face shows the stick's live state ----
  let { page: p, midi } = await connected();
  eq([pressed(p), shown(p, 'live'), shown(p, 'c-advanced-box'), shown(p, 'player'), shown(p, 'advanced-extra')], [['true', 'false'], true, true, false, false], 'USB: the face and the Advanced box appear, the player does not');
  eq(p.el('msg').textContent, '', 'no "Connected." text');
  ok(midi.sent.includes('get') && midi.sent.includes('home'), 'the page read get and home from the stick');
  eq([p.box.S.lastHome.bpm, p.box.S.lastHome.audio, p.box.S.lastGet.group], [120, true, '1'], "the HOME line and get of the stick are held");
  ok(p.frames.size === 1, 'the face draws frames while connected');
  p.runFrame();
  const lit = [...p.el('oled').lastImage.data].filter((v, i) => i % 4 === 0 && v > 100).length;
  ok(lit > 100, 'the screen shows the stick\'s HOME (' + lit + ' lit pixels)');
  for (const [, tick] of p.intervals()) tick();
  await settle();
  ok(midi.sent.includes('status'), 'status is polled (every 1.5 s) for the gate, the bar and the Bluetooth window, with no details open');

  // ---- the face's buttons send the stick's commands ----
  const before = midi.sent.length;
  tap(p, 'ArrowUp');
  await settle();
  eq(midi.sent.slice(before, before + 1), ['fold x2'], 'UP in AUDIO mode: `fold x2`, through the page\'s command function');
  ok(midi.sent.slice(before).includes('home'), 'and the home line is read again');
  eq(p.el('msg').textContent, 'Tempo doubled (x2).', 'the page says what it did');
  tap(p, 'Enter');
  await settle();
  ok(midi.sent.slice(before).includes('stop'), 'YES on HOME while running: `stop`');
  tap(p, 'ArrowLeft');                                               // NO: the menu, locally
  tap(p, 'Enter');                                                   // AUDIO
  tap(p, 'Enter');                                                   // SOURCE
  const mark = midi.sent.length;
  tap(p, 'ArrowUp');                                                 // AUTO -> LINE IN: a live edit
  await settle();
  eq(midi.sent.slice(mark, mark + 1), ['set source line'], 'a menu edit: `set source line`');
  ok(midi.sent.slice(mark).includes('get'), 'and get is read back to show what the stick has');
  tap(p, 'ArrowLeft');
  tap(p, 'ArrowLeft');
  tap(p, 'ArrowLeft');
  await settle();

  // a refused command shows the stick's own error
  ({ page: p, midi } = await connected({ 'fold x2': 'err link mode' }));
  tap(p, 'ArrowUp');
  await settle();
  eq([p.el('msg').textContent, p.el('msg').classList.contains('bad')], ['x2 and ÷2 are for AUDIO mode (music heard).', true], 'err link mode: shown as the page shows errors');
  ({ page: p, midi } = await connected({ 'fold x2': 'err fold x2, /2 or 1' }));
  tap(p, 'ArrowUp');
  await settle();
  eq([p.el('msg').textContent, p.el('msg').classList.contains('bad')], ['err fold x2, /2 or 1', true], 'another err reply: its text, as an error');

  // an action the stick cannot take does nothing, visibly, with a one-line reason
  ({ page: p, midi } = await connected());
  const n0 = midi.sent.length;
  for (const k of ['ArrowLeft', 'ArrowDown', 'ArrowDown', 'ArrowDown', 'ArrowDown', 'Enter', 'ArrowUp']) tap(p, k);      // MENU > USB > a step of the role
  await settle();
  eq([p.el('msg').textContent, p.el('msg').classList.contains('bad')], ['Not from here: the USB role restarts the stick: not changed from here.', true], 'USB role from the menu: refused with the reason, shown as an error');
  eq(midi.sent.slice(n0).filter(c => c.startsWith('set ')), [], 'and nothing was sent');

  // ---- the port goes away: back to nothing shown ----
  ({ page: p, midi } = await connected());
  ok(p.timers.size > 0 && p.frames.size === 1, 'timers and a frame run while connected');
  midi.access.inputs.delete('in');
  midi.access.onstatechange();
  eq([pressed(p), hidden(p), p.timers.size, p.frames.size, midi.input.onmidimessage, p.box.S.line], [['false', 'false'], [false, false, false, false], 0, 0, null, null], 'the port went away: face gone, nothing pressed, no timer, no frame, the input released');
  eq(p.el('msg').textContent, "The jank's MIDI port went away.", 'and it says so');
  const afterGone = midi.sent.length;
  tap(p, 'ArrowUp');
  eq(midi.sent.length, afterGone, 'the keys do nothing any more');

  // ---- pressing the active button again leaves it ----
  ({ page: p, midi } = await connected());
  p.el('c-midi').click();
  eq([pressed(p), hidden(p), p.timers.size, p.frames.size], [['false', 'false'], [false, false, false, false], 0, 0], 'the active button pressed again: disconnected, all gone');

  // ---- Advanced over USB: the player and the extra info, diag polled every 2 s ----
  ({ page: p, midi } = await connected());
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  eq([shown(p, 'player'), shown(p, 'advanced-extra')], [true, true], 'Advanced ticked: the player and the extra show');
  ok(midi.sent.includes('diag'), 'diag is read at once');
  eq(p.el('adv-boots').textContent, 'boots 3 · reset: poweron', 'boots and the reset reason, first');
  eq([p.el('adv-settled').textContent, p.el('adv-drift').textContent, p.el('adv-rtt').textContent, p.el('adv-drive').textContent, p.el('adv-save').textContent].map(String), ['yes', '12', '800', 'ok', 'no'], 'the diag fields');
  eq([p.el('adv-link').textContent, p.el('adv-air').textContent, p.el('adv-batt').textContent].map(String), ['2', '0', '64 %'], 'the HOME information moved here');
  const polls = p.intervals();
  ok(polls.length >= 1, 'the diag poll is an interval');
  // a restart of the stick: boots changes between two polls
  midi.text.diag = 'boots=4\nreset=sw\nsave_pending=1\nlink_settled=0\nlink_drift_ppb=0\nlink_last_err_us=0\nlink_far=0\nlink_partner_rtt_us=0\ndrive=ok\nok';
  eq(p.el('adv-restart').classList.contains('hide'), true, 'no restart notice while the boots stay the same');
  for (const [, tick] of p.intervals()) tick();
  await settle();
  eq([p.el('adv-boots').textContent, p.el('adv-boots').classList.contains('restarted'), p.el('adv-restart').classList.contains('hide'), p.el('adv-save').textContent],
     ['boots 4 \u00b7 reset: sw', true, false, 'yes (unplugging loses it)'], 'a restart shows: boots and the reset reason change, a notice appears');
  assert.match(p.el('adv-restart').textContent, /boots 3 \u2192 4, reset: sw/); checks++;
  // untick: the poll stops
  p.el('c-advanced').checked = false;
  p.el('c-advanced').fire('change', {});
  eq([shown(p, 'player'), shown(p, 'advanced-extra'), p.timers.size > 0], [false, false, true], 'Advanced unticked: both hide (the connection\'s own timers go on)');
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  p.box.disconnected();
  eq([hidden(p), p.timers.size, p.frames.size], [[false, false, false, false], 0, 0], 'a disconnect with Advanced ticked: everything hidden, nothing left running');

  // ---- the player: the folder fallback, the "then" choice, stop on leaving ----
  ({ page: p, midi } = await connected());
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  const file = (name, rel, data = Buffer.from('')) => ({ name, webkitRelativePath: rel, slice: (a, b) => ({ arrayBuffer: async () => Uint8Array.from(data.subarray(a, b)).buffer }) });
  const tagged = Buffer.concat([Buffer.from('ID3'), Buffer.from([3, 0, 0, 0, 0, 0, 17]), Buffer.from('TBPM'), Buffer.from([0, 0, 0, 7, 0, 0, 0]), Buffer.from('124.00')]);
  p.el('pl-dir').fire('change', { target: { files: [file('a.mp3', 'Music/a.mp3', tagged), file('b2.mp3', 'Music/b2.mp3'), file('b10.mp3', 'Music/b10.mp3'), file('cover.jpg', 'Music/cover.jpg')] } });
  await settle();
  const rows = () => p.el('pl-list').children;
  eq(rows().map(r => r.textContent), ['a.mp3', 'b2.mp3', 'b10.mp3'], 'the folder lists the audio files, natural-sorted, nothing else');
  eq(shown(p, 'pl-browser'), true, 'the browser shows once a folder is chosen');
  rows()[0].click();
  await settle();
  eq([p.el('pl-name').textContent, p.el('pl-bpm').textContent], ['a.mp3', 'tag BPM 124.00 × pitch → 124.00'], 'a click plays the file; its tag BPM shows');
  p.el('pl-pitch').value = '8';
  p.el('pl-pitch').fire('input', {});
  eq([p.el('pl-bpm').textContent, p.el('pl-pitch-num').value], ['tag BPM 124.00 × pitch → 133.92', 8], 'the tag BPM follows the pitch');
  p.el('pl-pitch-num').value = '-2.5';
  p.el('pl-pitch-num').fire('input', {});
  eq([p.el('pl-pitch').value, p.el('pl-pitchval').textContent], [-2.5, '-2.50 %'], 'the number box sets the pitch too');
  // "then": next
  const audio = p.el('pl-audio');
  audio.fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'b2.mp3', 'a track that ends: the next one in the list');
  audio.fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'b10.mp3', 'and the next');
  audio.fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'b10.mp3', 'the last one with "then next": it stops');
  p.el('pl-next').click();
  await settle();
  eq(p.el('pl-name').textContent, 'a.mp3', 'the Next button wraps to the first');
  p.el('pl-then').value = 'stop';
  p.el('pl-then').fire('change', {});
  audio.fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'a.mp3', '"then stop": nothing follows');
  p.el('pl-then').value = 'random';
  p.el('pl-then').fire('change', {});
  p.box.Math = Object.create(Math);
  audio.fire('ended', {});
  await settle();
  ok(['b2.mp3', 'b10.mp3'].includes(p.el('pl-name').textContent), '"then random": another file of the folder, never the one that ended');
  audio.paused = false;
  p.el('c-advanced').checked = false;
  p.el('c-advanced').fire('change', {});
  eq([shown(p, 'player'), p.media.paused > 0, p.media.revoked.length > 0, p.el('pl-play').disabled], [false, true, true, true], 'Advanced unticked: playback stops and the file is released');


  // ---- the stick's output without the microphone ----
  const fakeDevices = (withChooser, saved) => {
    const calls = { getUserMedia: 0, selectAudioOutput: 0, enumerate: 0, stopped: 0 }, store = new Map(saved ? [['jank.player.output', JSON.stringify(saved)]] : []);
    const mediaDevices = {
      enumerateDevices: async () => { calls.enumerate++; return [{ kind: 'audiooutput', label: '', deviceId: 'spk' }, { kind: 'audiooutput', label: '', deviceId: 'jk' }]; },
      getUserMedia: async () => { calls.getUserMedia++; return { getTracks: () => [{ stop: () => { calls.stopped++; } }] }; },
      addEventListener: () => {},
    };
    if (withChooser) mediaDevices.selectAudioOutput = async () => { calls.selectAudioOutput++; return { deviceId: 'jk', label: 'JANK USB Audio' }; };
    return { calls, store, opts: { mediaDevices, storage: { getItem: k => (store.has(k) ? store.get(k) : null), setItem: (k, v) => store.set(k, v) } } };
  };
  let dev = fakeDevices(true);
  ({ page: p, midi } = await connected(undefined, dev.opts));
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  eq([shown(p, 'pl-pick'), p.el('pl-pick').textContent, p.el('pl-pick-help').textContent], [true, "Choose the stick's output", "Choose the stick's output (jank) once."], 'no stick output chosen yet: the chooser button and its one line');
  eq(dev.calls, { getUserMedia: 0, selectAudioOutput: 0, enumerate: 1, stopped: 0 }, 'the card read the device list (ids, no permission) and asked for nothing');
  p.el('pl-dir').fire('change', { target: { files: [{ name: 'a.mp3', webkitRelativePath: 'Music/a.mp3', slice: () => ({ arrayBuffer: async () => new ArrayBuffer(0) }) }] } });
  await settle();
  p.el('pl-list').children[0].click();
  p.el('pl-play').click();
  await settle();
  eq(dev.calls.getUserMedia, 0, 'playing never asks for the microphone');
  p.el('pl-pick').click();
  await settle();
  eq([dev.calls.selectAudioOutput, dev.calls.getUserMedia, p.el('pl-outlabel').textContent, JSON.parse(dev.store.get('jank.player.output'))], [1, 0, 'JANK USB Audio', { id: 'jk', label: 'JANK USB Audio' }], "the click opens the browser's own chooser; the choice is shown and remembered; no microphone");
  eq(shown(p, 'pl-pick'), false, 'a chosen device that works: the button goes away');
  p.box.disconnected();
  // next visit: the saved id is used silently
  dev = fakeDevices(true, { id: 'jk', label: 'JANK USB Audio' });
  ({ page: p, midi } = await connected(undefined, dev.opts));
  await settle();
  eq([p.el('pl-outlabel').textContent, shown(p, 'pl-pick'), dev.calls.selectAudioOutput, dev.calls.getUserMedia], ['JANK USB Audio', false, 0, 0], 'the saved device is used silently: no button, no chooser, no microphone');
  // a browser with no output chooser: the microphone only behind the explicit button
  dev = fakeDevices(false);
  ({ page: p, midi } = await connected(undefined, dev.opts));
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  eq([p.el('pl-pick').textContent, p.el('pl-pick-help').textContent, dev.calls.getUserMedia], ['Allow device names (asks for the microphone)', 'Chrome will ask for the microphone only to read the device names; nothing is recorded.', 0], 'no output chooser: the explicit button and its sentence; the microphone is not asked at load');
  p.el('pl-pick').click();
  await settle();
  eq([dev.calls.getUserMedia, dev.calls.stopped], [1, 1], 'only the button asks, once, and the stream is stopped at once');
  p.box.disconnected();

  // ---- over Bluetooth: the face, no Advanced ----
  page = makePage();
  vm.runInContext('setConnected("ble", true)', page.box);
  eq([pressed(page), shown(page, 'live'), shown(page, 'c-advanced-box'), shown(page, 'player')], [['false', 'true'], true, false, false], 'Bluetooth: the face shows, no Advanced box, no player');
  page.el('c-advanced').checked = true;
  page.el('c-advanced').fire('change', {});
  eq(shown(page, 'player'), false, 'Bluetooth: a ticked Advanced still shows no player');
  eq(page.frames.size, 1, 'the face draws while connected over Bluetooth too');
  page.box.disconnected();
  eq([hidden(page), page.frames.size, page.timers.size, pressed(page)], [[false, false, false, false], 0, 0, ['false', 'false']], 'Bluetooth gone: everything hidden, nothing running');

  eq(errors.map(e => String((e && e.stack) || e)), [], 'no error thrown anywhere');
  console.log('connect: ' + checks + ' checks passed');
})().catch(e => { console.error(e); process.exit(1); });
