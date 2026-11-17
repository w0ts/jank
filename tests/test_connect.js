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
const lib = ['ui/oled.js', 'ui/face.js', 'ui/lcd2.js', 'ui/ports.js', 'ui/autolat.js', 'ui/stick.js', 'ui/player.js'].map(f => fs.readFileSync(path.join(root, f), 'utf8')).join('\n') + '\nthis.OLED = OLED; this.Face = Face; this.Stick = Stick; this.Player = Player;';
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
    click: () => { (listeners.click || []).forEach(fn => fn({ target: el })); if (el.onclick) el.onclick({ target: el }); },
    fire: (type, ev) => (listeners[type] || []).forEach(fn => fn(ev)), contains: () => false,
    append: () => {}, add: () => {}, focus: () => {}, remove: () => {}, removeAttribute: k => { delete attrs[k]; }, replaceChildren: (...kids) => { el.children = kids; },
    closest: () => null, cloneNode: () => makeElement(), setPointerCapture: () => {}, selectedOptions: [{ text: '' }],
    getContext: () => ({ createImageData: (w = 128, h = 64) => ({ data: new Uint8ClampedArray(w * h * 4), width: w, height: h }), putImageData(img) { el.lastImage = img; } }),
  });
  const kids = new Map();
  el.querySelector = sel => { if (!kids.has(sel)) kids.set(sel, makeElement()); return kids.get(sel); };
  el.firstElementChild = makeElement.leaf ? null : null;
  Object.defineProperty(el, 'firstElementChild', { get: () => el.querySelector('first') });
  return el;
}

function makeAudio(media, withCapture) {
  const audio = makeElement();
  if (withCapture) audio.captureStream = () => ({ tracks: 1 });
  Object.assign(audio, { tagName: 'AUDIO', paused: true, ended: false, currentTime: 0, duration: NaN, volume: 1, src: '', pause() { audio.paused = true; media.paused++; }, load() {}, removeAttribute() { audio.src = ''; }, play() { audio.paused = false; return Promise.resolve(); } });
  return audio;
}

function makePage(opts = {}) {
  const elements = new Map(), timers = new Set(), frames = new Set();
  let nextTimer = 1, nextFrame = 1;
  const bus = {}, winBus = {}, media = { paused: 0, revoked: [] }, clock = { t: 1000 };
  const document = {
    getElementById: id => { if (!elements.has(id)) elements.set(id, id === 'pl-audio' ? makeAudio(media, !!opts.audioApi && opts.audioApi.capture !== false) : makeElement()); return elements.get(id); },
    addEventListener: (type, fn) => { (bus[type] = bus[type] || []).push(fn); },
    dispatchEvent: ev => { (bus[ev.type] || []).forEach(fn => fn(ev)); return true; },
    createElement: () => makeElement(), createElementNS: () => makeElement(), activeElement: null,
  };
  const setTimer = () => { const id = nextTimer++; timers.add(id); return id; };
  const box = {
    document, console, performance: { now: () => clock.t }, Option: class { constructor(text, value) { this.text = text; this.value = value; } },
    setInterval: (fn, ms) => { const id = setTimer(); box.__intervals = box.__intervals || new Map(); box.__intervals.set(id, fn); return id; }, setTimeout: fn => { const id = setTimer(); box.__timeouts = box.__timeouts || new Map(); box.__timeouts.set(id, fn); return id; },
    clearInterval: id => timers.delete(id), clearTimeout: id => { timers.delete(id); if (box.__timeouts) box.__timeouts.delete(id); },
    requestAnimationFrame: fn => { const id = nextFrame++; frames.add({ id, fn }); return id; },
    cancelAnimationFrame: id => { for (const f of frames) if (f.id === id) frames.delete(f); },
    CustomEvent: class { constructor(type, init) { this.type = type; this.detail = init && init.detail; } }, navigator: { mediaDevices: opts.mediaDevices, permissions: opts.permissions, requestMIDIAccess: opts.requestMIDIAccess },
    localStorage: opts.storage || { getItem: () => null, setItem: () => {}, removeItem: () => {} },
    URL: { createObjectURL: f => 'blob:' + f.name, revokeObjectURL: u => media.revoked.push(u) },
    window: { addEventListener: (type, fn) => { (winBus[type] = winBus[type] || []).push(fn); } }, location: { search: opts.search || '' }, URLSearchParams, TextDecoder, Uint8Array, Float32Array, MessageChannel: opts.MessageChannel || MessageChannel,
  };
  if (opts.indexedDB) { box.indexedDB = opts.indexedDB; box.window.showDirectoryPicker = opts.showDirectoryPicker; }
  if (opts.audioApi) { box.AudioContext = opts.audioApi.Context; box.window.AudioContext = opts.audioApi.Context; }
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
  return { box, el: id => document.getElementById(id), timers, frames, media, clock, key, runFrame, intervals: () => [...(box.__intervals || [])], timeouts: () => [...(box.__timeouts || [])] };
}


// a fake Web Audio: records every context, setSinkId call and connection, so the routing to the stick can be asserted exactly
function fakeAudioApi(options = {}) {
  const log = [], contexts = [], gains = [];
  class Context {
    constructor(o) { this.opts = o || {}; this.state = 'suspended'; this.sinkId = options.ignoreCtorSink ? '' : (this.opts.sinkId || ''); this.destination = { kind: 'destination' }; this.currentTime = 0; this.baseLatency = 0.010; this.outputLatency = this.opts.sinkId ? 0.040 : 0.020; contexts.push(this); log.push(['new', this.opts.sinkId || null, this.opts.sampleRate || null]); }
    async resume() { log.push(['resume']); this.state = 'running'; }
    async suspend() { log.push(['suspend']); this.state = 'suspended'; }
    async close() { log.push(['close']); this.state = 'closed'; }
    createAnalyser() { return { kind: 'analyser', fftSize: 0, getFloatTimeDomainData: a => a.fill(0.1) }; }
    createGain() { const g = { kind: 'gain', gain: { value: 1 }, channelCount: 2, connect: n => { log.push(['gain->', n.kind]); }, disconnect() {} }; gains.push(g); return g; }
    createMediaStreamSource() { const src = { kind: 'source', connect: n => { log.push(['source->', n.kind]); }, disconnect: () => log.push(['source disconnect']) }; return src; }
  }
  if (!options.noSinkId) Context.prototype.setSinkId = async function (id) { log.push(['setSinkId', id]); if (options.rejectSink) throw new Error(options.rejectSink); this.sinkId = id; };
  return { Context, log, contexts, gains, capture: options.capture };
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
  eq([p.el('oled').width, p.el('oled').height, p.el('oled').lastImage.width], [320, 240, 320], 'the default screen is the colour lcd2 landscape (320 x 240), for every board');
  p.box.disconnected();
  {
    const q = await connected(undefined, { search: '?screen=oled' });
    q.page.runFrame();
    eq([q.page.el('oled').width, q.page.el('oled').height, q.page.el('oled').lastImage.width], [128, 64, 128], '?screen=oled: the black and white 128 x 64 OLED');
    q.page.box.disconnected();
  }
  ({ page: p, midi } = await connected());
  p.runFrame();
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
  // a diag answer with no boots field is no restart
  midi.text.diag = 'save_pending=0\nok';
  for (const [, tick] of p.intervals()) tick();
  await settle();
  eq(p.el('adv-boots').textContent, 'boots - \u00b7 reset: -', 'a diag without boots: shown as unknown');
  assert.match(p.el('adv-restart').textContent, /boots 3 \u2192 4/); checks++;
  midi.text.diag = 'boots=4\nreset=sw\nsave_pending=1\nlink_settled=0\nlink_drift_ppb=0\nlink_last_err_us=0\nlink_far=0\nlink_partner_rtt_us=0\ndrive=ok\nok';
  for (const [, tick] of p.intervals()) tick();
  await settle();
  eq(p.el('adv-restart').textContent.includes('NaN'), false, 'no NaN notice');
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
  const fakeDevices = (withChooser, saved, listing, extra = {}) => {
    const calls = { getUserMedia: 0, selectAudioOutput: 0, enumerate: 0, stopped: 0 }, handlers = {}, store = new Map(saved ? [['jank.player.output', JSON.stringify(saved)]] : []);
    const mediaDevices = {
      enumerateDevices: async () => { calls.enumerate++; return (extra.current || listing || [{ kind: 'audiooutput', label: '', deviceId: 'spk' }, { kind: 'audiooutput', label: '', deviceId: 'jk' }]); },
      getUserMedia: async () => { calls.getUserMedia++; return { getTracks: () => [{ stop: () => { calls.stopped++; } }] }; },
      addEventListener: (type, fn) => { handlers[type] = fn; },
    };
    if (withChooser) mediaDevices.selectAudioOutput = async () => { calls.selectAudioOutput++; return { deviceId: 'jk', label: extra.chosenLabel || 'JANK USB Audio' }; };
    return { calls, store, handlers, extra, opts: { mediaDevices, storage: { getItem: k => (store.has(k) ? store.get(k) : null), setItem: (k, v) => store.set(k, v), removeItem: k => store.delete(k) } } };
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
  eq(shown(p, 'pl-out-row'), true, 'permission still needed: the line is shown');
  eq([p.el('pl-pick').textContent, p.el('pl-pick-help').textContent, dev.calls.getUserMedia], ['Allow device names (asks for the microphone)', 'Chrome will ask for the microphone only to read the device names; nothing is recorded.', 0], 'no output chooser: the explicit button and its sentence; the microphone is not asked at load');
  p.el('pl-pick').click();
  await settle();
  eq([dev.calls.getUserMedia, dev.calls.stopped], [1, 1], 'only the button asks, once, and the stream is stopped at once');
  p.box.disconnected();


  // ---- the routing to the stick, branch by branch, with a fake Web Audio that records every call ----
  const oneFile = (name = 'a.mp3') => ({ target: { files: [{ name, webkitRelativePath: 'Music/' + name, slice: () => ({ arrayBuffer: async () => new ArrayBuffer(0) }) }] } });
  const startPlayback = async (audioOpts, devOpts, saved, listing) => {
    const api = fakeAudioApi(audioOpts), d = fakeDevices(true, saved, listing, devOpts);
    d.opts.audioApi = api;
    const c = await connected(undefined, d.opts);
    c.page.el('c-advanced').checked = true;
    c.page.el('c-advanced').fire('change', {});
    await settle();
    c.page.el('pl-monitor').checked = true;
    c.page.el('pl-monitor').fire('change', {});
    c.page.el('pl-dir').fire('change', oneFile());
    await settle();
    return { ...c, api, d };
  };
  const play = async (c) => { c.page.el('pl-list').children[0].click(); await settle(); c.page.el('pl-audio').fire('playing', {}); await settle(); };
  // saved id valid, labels absent: the context is created with the sink, read back, no extra setSinkId, source -> mono gain -> destination
  let r = await startPlayback({}, {}, { id: 'jk', label: 'jank (303a:4a4b)' });
  await play(r);
  eq(r.api.log.filter(e => e[0] === 'new' && e[2] === 44100), [['new', 'jk', 44100]], 'saved id: the stick context is created with sinkId and 44.1 kHz');
  eq(shown(r.page, 'pl-out-row'), false, 'resolved automatically and routed without an error: the whole "stick output" line is hidden');
  eq(r.api.log.filter(e => e[0] === 'setSinkId'), [], 'its sinkId reads back as the saved id: no second setSinkId needed');
  eq(r.api.log.filter(e => /->/.test(e[0])).map(e => e.join('')), ['gain->destination', 'source->analyser', 'source->gain'], 'captureStream -> MediaStreamSource -> mono gain -> that context\'s destination');
  eq([r.api.gains[0].channelCount, r.api.gains[0].channelCountMode, r.api.gains[0].channelInterpretation], [1, 'explicit', 'speakers'], 'the stick path is mixed down to one channel explicitly');
  eq([r.api.contexts[0].state, r.api.contexts[0].sinkId, r.d.calls.getUserMedia], ['running', 'jk', 0], 'the context runs, aimed at the stick, no microphone');
  assert.match(r.page.el('pl-output-report').textContent, /stick context: running, sinkId jk/); checks++;
  assert.match(r.page.el('pl-status').textContent, /monitor \+ jank \(303a:4a4b\)/); checks++;
  // muting the monitor leaves the captured stream alone
  r.page.el('pl-monitor').checked = false;
  r.page.el('pl-monitor').fire('change', {});
  eq([r.page.el('pl-audio').muted, r.api.contexts.filter(c => c.opts.sampleRate).length, r.api.log.filter(e => e[0] === 'source disconnect').length], [true, 1, 0], 'monitor off mutes the element only: the stick path is untouched');
  r.page.box.disconnected();

  // the constructor does not take the sink (older browser): setSinkId is awaited and called with the saved id
  r = await startPlayback({ ignoreCtorSink: true }, {}, { id: 'jk', label: 'jank' });
  await play(r);
  eq(r.api.log.filter(e => e[0] === 'setSinkId'), [['setSinkId', 'jk']], 'a context that did not take the sink: `await ctx.setSinkId(id)` with the saved id');
  r.page.box.disconnected();

  // setSinkId rejecting: the exact error is shown, the monitor goes on, the choose button comes back
  r = await startPlayback({ ignoreCtorSink: true, rejectSink: 'NotFoundError: no such device' }, {}, { id: 'jk', label: 'jank' });
  await play(r);
  assert.match(r.page.el('pl-status').textContent, /jank output failed: NotFoundError: no such device, monitor only/); checks++;
  eq(shown(r.page, 'pl-out-row'), true, 'a routing error: the line is shown again');
  eq([shown(r.page, 'pl-pick'), r.page.el('pl-audio').paused], [true, false], 'the button to choose again is shown and the monitor keeps playing');
  assert.match(r.page.el('pl-output-report').textContent, /last sink error: NotFoundError: no such device/); checks++;
  r.page.box.disconnected();

  // the context exists before the device is known; choosing it later re-aims it live, with no restart of the track
  r = await startPlayback({}, {}, null);
  await play(r);
  eq(shown(r.page, 'pl-out-row'), true, 'no stick output found: the line (and its change link) is shown');
  eq([r.api.contexts.filter(c => c.opts.sampleRate).length, r.api.log.filter(e => e[0] === 'new' && e[2] === 44100), r.api.log.filter(e => e.join('') === 'source->gain').length], [1, [['new', null, 44100]], 0], 'no output chosen: a context for the meter, nothing sent to the stick');
  const pausedBefore = r.page.media.paused;
  r.page.el('pl-pick').click();
  await settle();
  eq(r.api.log.filter(e => e[0] === 'setSinkId'), [['setSinkId', 'jk']], 'the choice re-aims the running context: setSinkId(jk)');
  eq(shown(r.page, 'pl-out-row'), false, 'after the choice routes without an error: the line is hidden again');
  eq([r.api.contexts.filter(c => c.opts.sampleRate).length, r.page.media.paused - pausedBefore, r.page.el('pl-audio').paused, r.api.log.filter(e => e[0] === 'source->').map(e => e[1])], [1, 0, false, ['analyser', 'gain']], 'live: same context, the track was not stopped, the source now feeds the stick path');
  r.page.box.disconnected();

  // a saved device that is not listed (ids shown, no match) is dropped; when it appears (devicechange) it is used, live
  const listed2 = [{ kind: 'audiooutput', label: 'MacBook Speakers', deviceId: 'spk' }];
  r = await startPlayback({}, { current: listed2 }, { id: 'jk', label: 'jank (303a:4a4b)' }, listed2);
  await settle();
  eq([r.page.el('pl-outlabel').textContent, shown(r.page, 'pl-pick'), shown(r.page, 'pl-out-row')], ['none', true, true], 'a saved output that is not in a list that shows ids: dropped, the button and the line are back');
  await play(r);
  eq(r.api.log.filter(e => e[0] === 'setSinkId'), [], 'nothing is sent to a device that is not there');
  r.d.extra.current = [{ kind: 'audiooutput', label: 'MacBook Speakers', deviceId: 'spk' }, { kind: 'audiooutput', label: 'jank (303a:4a4b)', deviceId: 'jk' }];
  await r.d.handlers.devicechange();
  await settle();
  eq([r.page.el('pl-outlabel').textContent, r.api.log.filter(e => e[0] === 'setSinkId'), shown(r.page, 'pl-pick')], ['jank (303a:4a4b)', [['setSinkId', 'jk']], false], 'the stick appears (devicechange): the saved device is found again and the running context is aimed at it');
  r.page.box.disconnected();

  // labels present: the explicit button reads the names; "jank (303a:4a4b)" is matched in any case
  const named = [{ kind: 'audiooutput', label: 'Default - MacBook Speakers', deviceId: 'default' }, { kind: 'audiooutput', label: 'MacBook Speakers', deviceId: 'spk' }, { kind: 'audiooutput', label: 'JANK (303a:4a4b)', deviceId: 'jk' }];
  const dn = fakeDevices(false, null, null, { current: [{ kind: 'audiooutput', label: '', deviceId: '' }] });
  dn.opts.audioApi = fakeAudioApi({});
  ({ page: p, midi } = await connected(undefined, dn.opts));
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  dn.extra.current = named;
  p.el('pl-pick').click();
  await settle();
  eq([p.el('pl-outlabel').textContent, dn.calls.getUserMedia, dn.calls.stopped], ['JANK (303a:4a4b)', 1, 1], 'after the one permission: jank found by its label, any case; the probe stopped at once');
  assert.match(p.el('pl-note').textContent, /^Output names unlocked\./); checks++;
  eq(JSON.parse(dn.store.get('jank.player.output')), { id: 'jk', label: 'JANK (303a:4a4b)' }, 'the id and the label are saved');
  p.box.disconnected();

  // ---- the play scope: every audio file under the picked folder ----
  const rel = (path) => ({ name: path.split('/').pop(), webkitRelativePath: path, slice: () => ({ arrayBuffer: async () => new ArrayBuffer(0) }) });
  ({ page: p, midi } = await connected());
  p.el('c-advanced').checked = true;
  p.el('c-advanced').fire('change', {});
  await settle();
  p.el('pl-dir').fire('change', { target: { files: ['Music/a.mp3', 'Music/Sub/b.mp3', 'Music/Sub/Deep/c.mp3', 'Music/Sub/Deep/.hidden.mp3', 'Music/__MACOSX/x.mp3', 'Music/$RECYCLE.BIN/y.mp3', 'Music/cover.jpg'].map(rel) } });
  await settle();
  eq(p.el('pl-scope-line').textContent, '3 files in scope', 'the scope: audio files under the picked folder, junk and hidden skipped');
  eq([p.el('pl-scope').checked, shown(p, 'pl-scope-row')], [true, true], 'all folders is on by default');
  p.el('pl-list').children.find(b => b.textContent === 'a.mp3').click();
  await settle();
  eq(p.el('pl-name').textContent, 'a.mp3', 'a file of the opened folder plays');
  p.el('pl-audio').fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'a.mp3', 'a.mp3 is the last file of the whole tree (folders first, depth first): ending there stops');
  p.el('pl-next').click();
  await settle();
  eq(p.el('pl-name').textContent, 'c.mp3', 'the Next button wraps to the first file of the tree (Sub/Deep/c.mp3)');
  p.el('pl-audio').fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'b.mp3', 'next goes on through the tree: Sub/b.mp3, though only the root folder is open');
  p.el('pl-then').value = 'random';
  p.el('pl-then').fire('change', {});
  vm.runInContext('Math.random = () => 0', p.box);
  p.el('pl-audio').fire('ended', {});
  await settle();
  eq(p.el('pl-name').textContent, 'c.mp3', 'random picks any other file of the whole scope (never b.mp3 again)');
  p.el('pl-scope').checked = false;
  p.el('pl-scope').fire('change', {});
  await settle();
  p.el('pl-list').children.find(b => b.textContent === 'a.mp3').click();
  await settle();
  p.el('pl-then').value = 'next';
  p.el('pl-then').fire('change', {});
  eq([p.el('pl-next').disabled, shown(p, 'pl-scope-row')], [true, false], 'all folders off: the old behaviour, this folder only (a single file: nothing next)');
  p.el('pl-scope').checked = true;
  p.el('pl-scope').fire('change', {});
  await settle();
  eq(p.el('pl-next').disabled, false, 'on again: the whole tree is the scope again');
  p.el('pl-rescan').click();
  await settle();
  eq(p.el('pl-scope-line').textContent, '3 files in scope', 'rescan walks it again');
  p.el('c-advanced').checked = false;
  p.el('c-advanced').fire('change', {});
  eq(p.el('pl-scope-line').textContent, '0 files in scope', 'leaving Advanced cancels the scan and clears the list');
  p.box.disconnected();


  // ---- Play with nothing clicked: it starts from the files found so far, and the queue becomes the whole scope ----
  {
    const pending = [];
    class StepChannel { constructor() { this.port1 = { onmessage: null, close() {} }; this.port2 = { postMessage: () => pending.push(this.port1), close() {} }; } }
    const flush = async () => { const q = pending.splice(0); q.forEach(port => port.onmessage && port.onmessage()); await settle(4); };
    ({ page: p, midi } = await connected(undefined, { MessageChannel: StepChannel }));
    p.el('c-advanced').checked = true;
    p.el('c-advanced').fire('change', {});
    await settle();
    eq(p.el('pl-play').disabled, true, 'no source yet: Play is disabled');
    const names = ['A/a1.mp3', 'A/a2.mp3', 'B/b1.mp3', 'B/b2.mp3', 'C/c1.mp3'];
    p.el('pl-dir').fire('change', { target: { files: names.map(n => rel('Music/' + n)) } });
    await settle();
    eq([p.el('pl-play').disabled, p.el('pl-scope-line').textContent], [true, 'scanning: 0 files'], 'a source is picked and the scan has found nothing yet: Play stays disabled');
    await flush();
    await flush();
    eq([p.el('pl-play').disabled, p.el('pl-scope-line').textContent], [false, 'scanning: 2 files'], 'the first folder is in: Play is enabled while the scan goes on, nothing clicked');
    vm.runInContext('Math.random = () => 0.99', p.box);
    p.el('pl-then').value = 'random';
    p.el('pl-then').fire('change', {});
    p.el('pl-play').click();
    await settle();
    eq(['a1.mp3', 'a2.mp3'].includes(p.el('pl-name').textContent), true, 'Play with "random" starts a random one of the files found so far (' + p.el('pl-name').textContent + ')');
    await flush();
    await flush();
    await flush();
    eq(p.el('pl-scope-line').textContent, '5 files in scope', 'the scan finishes: the whole scope');
    // the queue is the whole scope now: random from a file in A can land in B or C
    const reached = new Set();
    for (let k = 0; k < 20; k++) {
      vm.runInContext('Math.random = () => ' + (k / 20), p.box);
      p.el('pl-audio').fire('ended', {});
      await settle();
      reached.add(p.el('pl-name').textContent[0]);
    }
    eq([...reached].sort(), ['a', 'b', 'c'], 'random now draws from the files of all three folders, not only the open one');
    // Play with "next" and the scope on: the first file of the scope; "all folders" off: the first of the folder shown
    p.box.disconnected();
  }
  {
    ({ page: p, midi } = await connected());
    p.el('c-advanced').checked = true;
    p.el('c-advanced').fire('change', {});
    await settle();
    p.el('pl-dir').fire('change', { target: { files: ['Music/Sub/s1.mp3', 'Music/Sub/s2.mp3', 'Music/r1.mp3'].map(rel) } });
    await settle();
    eq(p.el('pl-play').disabled, false, 'the scan is done: Play is enabled with nothing clicked');
    p.el('pl-then').value = 'next';
    p.el('pl-then').fire('change', {});
    p.el('pl-play').click();
    await settle();
    eq(p.el('pl-name').textContent, 's1.mp3', 'Play with "next": the first file of the whole scope (in a sub folder that was never opened)');
    p.el('pl-scope').checked = false;
    p.el('pl-scope').fire('change', {});
    await settle();
    p.box.disconnected();
  }
  {
    ({ page: p, midi } = await connected());
    p.el('c-advanced').checked = true;
    p.el('c-advanced').fire('change', {});
    await settle();
    p.el('pl-scope').checked = false;
    p.el('pl-scope').fire('change', {});
    p.el('pl-dir').fire('change', { target: { files: ['Music/Sub/s1.mp3', 'Music/r1.mp3', 'Music/r2.mp3'].map(rel) } });
    await settle();
    p.el('pl-then').value = 'stop';
    p.el('pl-then').fire('change', {});
    p.el('pl-play').click();
    await settle();
    eq(p.el('pl-name').textContent, 'r1.mp3', '"all folders" off: Play starts inside the folder shown (the root has r1 and r2), not from the sub folder');
    p.box.disconnected();
  }
  {
    ({ page: p, midi } = await connected());
    p.el('c-advanced').checked = true;
    p.el('c-advanced').fire('change', {});
    await settle();
    p.el('pl-dir').fire('change', { target: { files: ['Music/cover.jpg', 'Music/Sub/notes.txt'].map(rel) } });
    await settle();
    eq([p.el('pl-play').disabled, p.el('pl-scope-line').textContent], [true, 'no audio files under Music'], 'no audio file under the source: it says so and Play stays disabled');
    p.box.disconnected();
  }

  // ---- only a port named jank is ever attached to; no jank port: nothing is sent anywhere, and the page waits 15 s for one ----
  {
    const decoy = (id, name) => {
      const out = { id: 'o' + id, name, state: 'connected', frames: 0, send() { this.frames++; } };
      return { input: { id: 'i' + id, name, state: 'connected', onmidimessage: null }, output: out };
    };
    const open = async (names, withJank, extraJank = []) => {
      const page = makePage(), midi = fakeMidi();
      midi.attach(page);
      const decoys = names.map((n, i) => decoy(i, n));
      const ins = new Map(), outs = new Map();
      for (const d of decoys) { ins.set(d.input.id, d.input); outs.set(d.output.id, d.output); }
      const extras = extraJank.map((n, i) => decoy('b' + i, n));
      if (withJank) { ins.set('in', midi.input); outs.set('out', midi.output); }
      for (const d of extras) { ins.set(d.input.id, d.input); outs.set(d.output.id, d.output); }
      const access = { inputs: ins, outputs: outs, onstatechange: null };
      page.box.navigator.requestMIDIAccess = () => Promise.resolve(access);
      page.el('c-midi').click();
      await settle();
      return { page, midi, access, decoys, extras, frames: () => decoys.reduce((n, d) => n + d.output.frames, 0) };
    };
    const addJank = (c) => { c.access.outputs.set('out', c.midi.output); c.access.inputs.set('in', c.midi.input); };
    const gets = c => c.midi.sent.filter(s => s === 'get').length;

    // (a) Digitone + FM1 only: nothing attached, nothing sent, the error names the ports, the buttons stay usable
    let c = await open(['Digitone Audio+MIDI Elektron Digitone II', 'FM1-E040'], false);
    eq([c.frames(), c.midi.sent.length, c.page.box.S.line], [0, 0, null], '(a) no jank port: no byte sent to any port, nothing attached');
    eq(c.page.el('msg').textContent, "No MIDI port named jank. Plug it in (USB), or connect it in the app's Bluetooth MIDI settings. MIDI ports I see: Digitone Audio+MIDI Elektron Digitone II, FM1-E040", '(a) the line names what the browser sees');
    eq([c.page.el('msg').classList.contains('bad'), shown(c.page, 'c-ports-box'), shown(c.page, 'live'), pressed(c.page), c.page.el('c-midi').disabled, c.page.el('c-bluetooth').disabled], [true, false, false, ['false', 'false'], false, false], '(a) an error, no chooser, no face, both connection buttons usable');
    eq(c.page.timeouts().length, 1, '(a) one wait timer (the 15 s) is running');
    c.page.box.disconnected();
    eq([c.page.timers.size, c.access.onstatechange], [0, null], '(a) a disconnect releases the wait: no timer, no listener');

    // (b) Digitone + jank: jank only
    c = await open(['Digitone Audio+MIDI Elektron Digitone II'], true);
    eq([c.frames(), gets(c) > 0, c.page.box.S.line !== null, shown(c.page, 'c-ports-box')], [0, true, true, false], '(b) jank attached and spoken to, the Digitone never');
    c.page.box.disconnected();

    // (c) jank appears later: connects once; the input alone (output not yet there) is not enough
    c = await open(['Digitone Audio+MIDI Elektron Digitone II'], false);
    c.access.inputs.set('in', c.midi.input);
    c.access.onstatechange();
    await settle();
    eq([c.midi.sent.length, c.page.box.S.line], [0, null], '(c) a jank input without its output yet: still waiting');
    c.access.outputs.set('out', c.midi.output);
    c.access.onstatechange();
    await settle();
    const first = gets(c);
    eq([first, c.page.timeouts().length, c.frames()], [1, 0, 0], '(c) jank appeared: connected, one get, the wait timer cleared, the Digitone untouched');
    c.access.onstatechange();
    c.access.onstatechange();
    await settle();
    eq(gets(c), 1, '(c) more state changes attach nothing again');
    c.page.box.disconnected();

    // (d) 15 s without a jank: stop listening, say so, no late auto-connect
    c = await open(['FM1-E040'], false);
    for (const [, fn] of c.page.timeouts()) fn();
    eq(c.page.el('msg').textContent.startsWith('Stopped waiting for a jank port after 15 s. Press MIDI port to try again. MIDI ports I see: FM1-E040'), true, '(d) after 15 s it says it stopped');
    eq([c.access.onstatechange, c.page.timers.size], [null, 0], '(d) no listener, no timer left');
    addJank(c);
    await settle();
    eq([c.midi.sent.length, c.frames()], [0, 0], '(d) a jank plugged in after that connects nothing by itself');
    c.page.box.disconnected();

    // (e) USB + Bluetooth jank: the chooser lists those two, never the Digitone
    c = await open(['Digitone Audio+MIDI Elektron Digitone II'], true, ['jank 1A2B']);
    eq(c.page.el('c-ports').children.map(o => o.text), ['jank', 'jank 1A2B'], '(e) the chooser: USB first, then Bluetooth, no other device');
    eq([shown(c.page, 'c-ports-box'), c.frames()], [true, 0], '(e) shown, and the Digitone untouched');
    c.page.box.disconnected();

    // the error names the port that did not answer
    const wrong = fakeMidi({ get: '' });
    wrong.output.send = () => {};
    const wp = makePage();
    wrong.attach(wp);
    wp.box.navigator.requestMIDIAccess = () => Promise.resolve(wrong.access);
    wp.el('c-midi').click();
    await settle();
    for (let k = 0; k < 2; k++) { for (const [id, fn] of wp.timeouts()) { wp.box.clearTimeout(id); fn(); } await settle(); }
    eq(wp.el('msg').textContent.startsWith('no answer to get from jank. If the app asks for SysEx, turn it on.'), true, 'no answer to get: names the port, keeps the SysEx hint');
  }

  // ---- auto connect: at load, only when the browser already holds the MIDI permission with SysEx ----
  {
    const decoy = { input: { id: 'id0', name: 'Digitone Audio+MIDI Elektron Digitone II', state: 'connected', onmidimessage: null }, output: { id: 'od0', name: 'Digitone Audio+MIDI Elektron Digitone II', state: 'connected', frames: 0, send() { this.frames++; } } };
    const load = async ({ state = 'granted', jank = true, storage, noPermissions = false, queryThrows = false } = {}) => {
      const midi = fakeMidi();
      const ins = new Map([[decoy.input.id, decoy.input]]), outs = new Map([[decoy.output.id, decoy.output]]);
      if (jank) { ins.set('in', midi.input); outs.set('out', midi.output); }
      const access = { inputs: ins, outputs: outs, onstatechange: null };
      const calls = { query: [], request: 0 };
      const permissions = noPermissions ? undefined : { query: async (d) => { calls.query.push(d); if (queryThrows) throw new Error('unsupported'); return { state }; } };
      const page = makePage({ permissions, requestMIDIAccess: () => { calls.request++; return Promise.resolve(access); }, storage });
      midi.attach(page);
      await settle();
      const arrive = () => { access.outputs.set('out', midi.output); access.inputs.set('in', midi.input); access.onstatechange && access.onstatechange(); };
      const leave = () => { access.inputs.delete('in'); access.outputs.delete('out'); access.onstatechange && access.onstatechange(); };
      return { page, midi, access, calls, arrive, leave, decoyFrames: () => decoy.output.frames };
    };
    const gets = c => c.midi.sent.filter(s => s === 'get').length;
    const memory = () => { const m = new Map(); return { m, getItem: k => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, v), removeItem: k => m.delete(k) }; };

    // granted + jank present: connects at load, nothing to the other port
    let c = await load();
    eq([JSON.stringify(c.calls.query), c.calls.request, gets(c), c.page.box.S.kind, c.decoyFrames(), pressed(c.page)[0]], ['[{"name":"midi","sysex":true}]', 1, 1, 'midi', 0, 'true'], 'granted + jank: connected at load, 0 frames to the Digitone');
    c.page.box.disconnected();

    // granted + no jank: no error line, waits (no 15 s timer), connects once when it appears
    c = await load({ jank: false });
    eq([c.page.el('msg').textContent, shown(c.page, 'c-waiting'), c.page.timeouts().length, typeof c.access.onstatechange], ['', true, 0, 'function'], 'granted, no jank: no error line, the quiet waiting line, no timer, the listener kept');
    c.arrive();
    await settle();
    c.access.onstatechange && c.access.onstatechange();
    await settle();
    eq([gets(c), shown(c.page, 'c-waiting'), c.decoyFrames()], [1, false, 0], 'the jank appears: connected once, the waiting line gone');

    // unplug: idle, the listener re-armed, the waiting line; the jank returns (a flash restart): reconnects without a click
    c.leave();
    await settle();
    eq([c.page.box.S.kind, pressed(c.page)[0], shown(c.page, 'c-waiting'), typeof c.access.onstatechange, c.page.timeouts().length], [null, 'false', true, 'function', 0], 'unplug: idle, auto-connect re-armed (one listener, no timer)');
    c.arrive();
    await settle();
    eq([c.page.box.S.kind, gets(c)], ['midi', 2], 'the jank returns: reconnected without a click');

    // the user's disconnect turns auto-connect off, and it stays off
    c.page.el('c-midi').click();
    await settle();
    eq([c.page.box.S.kind, c.access.onstatechange, c.page.timers.size, c.page.box.S.auto], [null, null, 0, false], 'user disconnect: idle, no listener, no timer, auto-connect off');
    c.arrive();
    await settle();
    eq([gets(c), c.page.box.S.kind], [2, null], 'off: a statechange with the jank present connects nothing');
    c.page.el('c-midi').click();
    await settle();
    eq([c.page.box.S.kind, c.page.box.S.auto, gets(c)], ['midi', true, 3], 'pressing MIDI port again connects and turns it back on');
    c.page.box.disconnected();

    // remembered in localStorage: off at the next load, nothing called
    const st = memory();
    c = await load({ storage: st });
    c.page.el('c-midi').click();
    await settle();
    eq(st.m.get('jank.autoconnect'), '0', 'off is remembered');
    const again = await load({ storage: st });
    eq([again.calls.query.length, again.calls.request, again.page.box.S.kind], [0, 0, null], 'stored off: no permission query, no MIDI request at load');
    c.page.box.disconnected();

    // prompt / denied / unsupported / throwing query: requestMIDIAccess is not called at load
    for (const [what, o] of [['prompt', { state: 'prompt' }], ['denied', { state: 'denied' }], ['no permissions API', { noPermissions: true }], ['a query that throws', { queryThrows: true }]]) {
      const n = await load(o);
      eq([n.calls.request, n.page.box.S.kind, n.page.el('msg').textContent, n.access.onstatechange], [0, null, '', null], what + ': nothing requested, nothing shown, nothing listening');
    }
  }

  // ---- tooltips on what the page builds at run time: folder rows, breadcrumbs, the face's D-pad, the Join buttons ----
  {
    const c = await startPlayback({}, {}, { id: 'jk', label: 'jank (303a:4a4b)' });
    const rows = c.page.el('pl-list').children, crumbs = c.page.el('pl-crumbs').children.filter(k => k.tagName !== undefined && k.title !== undefined);
    ok(rows.length > 0 && rows.every(b => typeof b.title === 'string' && b.title.length > 0 && b.title.length <= 240), 'every folder / file row has a tooltip (' + rows.length + ' rows)');
    ok(crumbs.every(b => b.title.length > 0 && b.title.length <= 240), 'the breadcrumb buttons have tooltips');
    for (const b of ['up', 'down', 'no', 'yes']) {
      const tip = c.page.el('dev').querySelector('#arm-' + b).getAttribute('aria-label');
      ok(typeof tip === 'string' && tip.includes(b.toUpperCase()) && tip.length <= 240, 'the D-pad ' + b + ' has a tip: ' + tip);
    }
    c.page.box.disconnected();
  }

  // ---- Auto latency: set beat_latency from the two paths, only under its guards ----
  {
    const statusUsb = (source) => transcript.status.replace(/\nok\s*$/, '') + '\naudio_source=' + source + '\nok';
    const sets = c => c.midi.sent.filter(s => s.startsWith('set beat_latency'));
    const tickAll = async c => { for (const [, tick] of c.page.intervals()) tick(); c.page.el('pl-audio').fire('timeupdate', {}); await settle(); };
    const setup = async (source) => {
      const c = await startPlayback({}, {}, { id: 'jk', label: 'jank (303a:4a4b)' });
      c.midi.text.status = statusUsb(source);
      return c;
    };
    let a = await setup('usb');
    await tickAll(a);
    eq(sets(a), [], 'not at load: nothing is sent before the player plays');
    eq(a.page.el('al-line').textContent.startsWith('auto latency: stick path unknown'), true, 'the line says why: no stick path yet');
    await play(a);
    await tickAll(a);
    eq(sets(a), ['set beat_latency -20'], 'playing through the stick over USB: monitor 30 - stick 50 = -20 ms is sent, once');
    eq(a.page.el('al-line').textContent, 'auto latency: sent -20 ms (speakers 30 - stick path 50)', 'the line shows what was sent');
    a.page.clock.t += 1000;
    await tickAll(a);
    eq(sets(a).length, 1, 'rate limit: nothing more within 5 s');
    a.page.el('al-resend').click();
    await settle();
    eq(sets(a).length, 2, 'Re-send sends again at once');
    a.page.el('al-on').checked = false;
    a.page.el('al-on').fire('change', {});
    a.page.clock.t += 10000;
    await tickAll(a);
    a.page.el('al-resend').click();
    await settle();
    eq([sets(a).length, a.d.store.get('jank.autolat')], [2, '0'], 'the checkbox off: nothing is sent, and it is remembered');
    a.page.box.disconnected();

    const line = await setup('line');
    await play(line);
    await tickAll(line);
    await tickAll(line);
    eq([sets(line), line.page.el('al-line').textContent.includes('not USB')], [[], true], 'source is not USB: nothing sent, the line says so');
    line.page.box.disconnected();

    const near = await setup('usb');
    near.midi.text.status = near.midi.text.status.replace('lat_auto=25', 'lat_auto=-19');
    await play(near);
    await tickAll(near);
    await tickAll(near);
    eq([sets(near), near.page.el('al-line').textContent.includes('within 3 ms')], [[], true], 'within 3 ms of the stick lat_auto: nothing sent');
    near.page.box.disconnected();
  }

  // ---- the beat display delay: the page's own latencies, the estimate, the stored value; it never touches the stick ----
  {
    r = await startPlayback({}, {}, { id: 'jk', label: 'jank (303a:4a4b)' });
    const delays = [];
    r.page.box.document.addEventListener('jank-beat-delay', e => delays.push(e.detail.ms));
    await play(r);
    for (const [, tick] of r.page.intervals()) tick();
    await settle();
    eq([r.page.el('bd-mon').textContent, r.page.el('bd-stick').textContent], [30, 50], 'the page shows what it knows: the monitor 10 + 20 = 30 ms, the stick path 10 + 40 = 50 ms');
    eq([r.page.el('bd-num').value, r.page.el('bd-slider').value, delays.at(-1)], [-45, -45, -45], 'the default is the estimate: monitor - stick - beat_latency = 30 - 50 - 25 = -45 ms');
    eq(r.page.el('bd-beat').textContent, '25 (25 + 0)', "the stick's beat_latency, from get, with its two layers");
    const sentBefore = r.midi.sent.length;
    r.page.el('bd-slider').value = '120';
    r.page.el('bd-slider').fire('input', {});
    eq([r.page.el('bd-num').value, delays.at(-1), r.d.store.get('jank.beat.delay')], [120, 120, '120'], 'the slider sets the delay (the face is told) and it is remembered');
    r.page.el('bd-num').value = '-37';
    r.page.el('bd-num').fire('change', {});
    eq([r.page.el('bd-num').value, delays.at(-1), r.d.store.get('jank.beat.delay')], [-35, -35, '-35'], 'the number box rounds to 5 ms');
    r.page.el('bd-num').value = '9999';
    r.page.el('bd-num').fire('change', {});
    eq(delays.at(-1), 400, 'and stops at 400');
    for (const [, tick] of r.page.intervals()) tick();
    r.page.el('pl-audio').fire('timeupdate', {});
    await settle();
    eq(r.page.el('bd-num').value, 400, 'a stored delay is not replaced by a new estimate');
    eq(r.midi.sent.slice(sentBefore).filter(c => c.startsWith('set ') || c.startsWith('nudge')), [], 'the delay sends nothing to the stick');
    r.page.el('bd-estimate').click();
    eq([r.page.el('bd-num').value, delays.at(-1), r.d.store.has('jank.beat.delay')], [-45, -45, false], 'Use the estimate: back to it, the stored value gone');
    r.page.box.disconnected();
    // a stored delay is used from the start
    const dd = fakeDevices(true, null, null, {});
    dd.store.set('jank.beat.delay', '75');
    ({ page: p, midi } = await connected(undefined, dd.opts));
    p.el('c-advanced').checked = true;
    p.el('c-advanced').fire('change', {});
    await settle();
    eq(p.el('bd-num').value, 75, 'a stored delay is shown again on the next visit');
    p.box.disconnected();
  }

  // ---- Play with a remembered folder that needs its permission after a reload: the click is the gesture ----
  {
    const fileHandle = (name) => ({ kind: 'file', name, getFile: async () => rel('Music/' + name) });
    const dirHandle = (name, kids) => ({ kind: 'directory', name, entries: async function* () { for (const [n, h] of Object.entries(kids)) yield [n, h]; } });
    const music = (perm) => {
      const calls = { query: 0, request: 0 };
      let allowed = false;
      const handle = { ...dirHandle('Music', { A: dirHandle('A', { 'a1.mp3': fileHandle('a1.mp3'), 'a2.mp3': fileHandle('a2.mp3') }), B: dirHandle('B', { 'b1.mp3': fileHandle('b1.mp3') }), 'r.mp3': fileHandle('r.mp3') }),
        queryPermission: async () => { calls.query++; return allowed ? 'granted' : 'prompt'; },
        requestPermission: async () => { calls.request++; if (perm instanceof Error) throw perm; allowed = perm === 'granted'; return perm; } };
      return { handle, calls };
    };
    const fakeIdb = (value) => ({ open: () => { const req = {}; queueMicrotask(() => { req.result = { createObjectStore() {}, transaction: () => ({ objectStore: () => ({ get: () => { const r = {}; queueMicrotask(() => { r.result = value; r.onsuccess(); }); return r; }, put: () => { const r = {}; queueMicrotask(() => { r.onsuccess(); }); return r; } }) }) }; req.onsuccess(); }); return req; } });
    const withSaved = async (perm, StepChannel) => {
      const m = music(perm), opts = { indexedDB: fakeIdb(m.handle), showDirectoryPicker: async () => m.handle };
      if (StepChannel) opts.MessageChannel = StepChannel;
      const c = await connected(undefined, opts);
      c.page.el('c-advanced').checked = true;
      c.page.el('c-advanced').fire('change', {});
      await settle();
      return { ...c, m };
    };
    // granted: Play is enabled with nothing loaded; nothing is asked before the click; random from the files found so far
    const pending = [];
    class StepChannel { constructor() { this.port1 = { onmessage: null, close() {} }; this.port2 = { postMessage: () => pending.push(this.port1), close() {} }; } }
    const flush = async () => { const q = pending.splice(0); q.forEach(port => port.onmessage && port.onmessage()); await settle(4); };
    let g = await withSaved('granted', StepChannel);
    eq([g.page.el('pl-play').disabled, shown(g.page, 'pl-reopen'), g.page.el('pl-reopen').textContent, g.m.calls], [false, true, 'Open Music', { query: 1, request: 0 }], 'a saved folder that needs permission: Play is enabled, "Open Music" shows, and nothing was asked before a click');
    eq(g.page.el('pl-name').textContent, '', 'nothing is loaded (no track name)');
    vm.runInContext('Math.random = () => 0.99', g.page.box);
    g.page.el('pl-then').value = 'random';
    g.page.el('pl-then').fire('change', {});
    g.page.el('pl-play').click();
    await settle();
    eq(g.m.calls, { query: 3, request: 1 }, 'the click asks: queryPermission, then requestPermission once (and the open checks it again)');
    eq(g.page.el('pl-name').textContent, '', 'no track starts before the scan has found files');
    await flush();
    await flush();
    ok(['a1.mp3', 'a2.mp3'].includes(g.page.el('pl-name').textContent), 'Play starts a random one of the files found so far (' + g.page.el('pl-name').textContent + ')');
    await flush(); await flush(); await flush();
    eq(g.page.el('pl-scope-line').textContent, '4 files in scope', 'the scan goes on to the whole scope');
    eq([shown(g.page, 'pl-reopen'), g.page.el('pl-play').disabled], [false, false], 'the folder is open: the Open button is gone');
    g.page.box.disconnected();
    // Space over the card does the same
    g = await withSaved('granted');
    vm.runInContext('Math.random = () => 0', g.page.box);
    g.page.el('pl-then').value = 'random';
    g.page.el('pl-then').fire('change', {});
    g.page.el('player').fire('pointerenter', {});
    g.page.key('keydown', ' ');
    await settle(30);
    eq([g.m.calls.request, g.page.el('pl-name').textContent], [1, 'a1.mp3'], 'Space over the card is the same gesture: it asks, opens, starts a random track');
    g.page.box.disconnected();
    // denied
    g = await withSaved('denied');
    g.page.el('pl-play').click();
    await settle();
    eq([g.page.el('pl-note').textContent, g.page.el('pl-play').disabled, shown(g.page, 'pl-reopen'), g.page.el('pl-name').textContent],
       ['Permission to read "Music" was not given: press the folder button to choose it again, or Open Music to be asked again.', true, true, ''], 'denied: one line says so, Play is disabled again, Open Music stays, nothing starts');
    g.page.el('pl-reopen').click();
    await settle();
    eq(g.m.calls.request, 2, 'the Open button asks again');
    g.page.box.disconnected();
    // the folder is gone
    g = await withSaved(Object.assign(new Error('A requested file or directory could not be found'), { name: 'NotFoundError' }));
    g.page.el('pl-play').click();
    await settle();
    eq([g.page.el('pl-note').textContent, g.page.el('pl-play').disabled], ['The folder "Music" cannot be opened (A requested file or directory could not be found): press the folder button to choose it again.', true], 'a folder that is gone: said in one line, Play disabled');
    g.page.box.disconnected();
  }

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
