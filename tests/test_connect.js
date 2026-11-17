// SPDX-License-Identifier: MIT
// The control page's connection choice (Demo is the base) with a fake DOM and a fake MIDI transport: Demo is pressed at load; the Advanced check box and the Player card exist only over USB MIDI; a failed connect leaves it
// pressed and says why; connecting presses MIDI port; the port going away, pressing the active button again and pressing Demo all end
// in the Demo state with every timer stopped, the player stopped and released, and no error thrown.
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

const html = fs.readFileSync(path.join(__dirname, '..', 'page', 'jank.html'), 'utf8');
const playerSource = fs.readFileSync(path.join(__dirname, '..', 'ui', 'player.js'), 'utf8');
const script = id => html.match(new RegExp('<script id="' + id + '">([\\s\\S]*?)</script>'))[1];

function makeElement() {
  const listeners = {}, attrs = {}, classes = new Set();
  const el = {
    textContent: '', value: '', disabled: false, checked: false, children: [], style: {}, dataset: {}, options: [],
    classList: { add: c => classes.add(c), remove: c => classes.delete(c), contains: c => classes.has(c), toggle: (c, on) => { (on === undefined ? !classes.has(c) : on) ? classes.add(c) : classes.delete(c); } },
    setAttribute: (k, v) => { attrs[k] = String(v); }, getAttribute: k => (k in attrs ? attrs[k] : null),
    addEventListener: (type, fn) => { (listeners[type] = listeners[type] || []).push(fn); },
    click: () => (listeners.click || []).forEach(fn => fn({})),
    fire: (type, ev) => (listeners[type] || []).forEach(fn => fn(ev)), contains: () => false,
    append: () => {}, replaceChildren: () => {}, add: () => {}, focus: () => {}, remove: () => {},
  };
  return el;
}

function makeAudio(media) {
  const audio = makeElement();
  Object.assign(audio, { paused: true, ended: false, currentTime: 0, duration: NaN, volume: 1, src: '', pause() { audio.paused = true; media.paused++; }, load() {}, removeAttribute() { audio.src = ''; }, play() { audio.paused = false; return Promise.resolve(); } });
  return audio;
}

function makePage() {
  const elements = new Map();
  const timers = new Set();
  let nextTimer = 1;
  const bus = {}, events = [], media = { paused: 0, revoked: [] };
  const document = {
    getElementById: id => { if (!elements.has(id)) elements.set(id, id === 'pl-audio' ? makeAudio(media) : makeElement()); return elements.get(id); },
    addEventListener: (type, fn) => { (bus[type] = bus[type] || []).push(fn); },
    dispatchEvent: ev => { events.push(ev); (bus[ev.type] || []).forEach(fn => fn(ev)); return true; },
  };
  const setTimer = fn => { const id = nextTimer++; timers.add(id); return id; };
  const box = {
    document, console, performance: { now: () => 1000 }, Option: class { constructor(text, value) { this.text = text; this.value = value; } },
    setInterval: setTimer, setTimeout: setTimer, clearInterval: id => timers.delete(id), clearTimeout: id => timers.delete(id),
    CustomEvent: class { constructor(type, init) { this.type = type; this.detail = init && init.detail; } }, navigator: {},
    URL: { createObjectURL: () => 'blob:test', revokeObjectURL: u => media.revoked.push(u) }, requestAnimationFrame: () => 1, cancelAnimationFrame: () => {}, window: { addEventListener: () => {} }, location: { search: "" }, URLSearchParams, TextDecoder,
  };
  vm.createContext(box);
  vm.runInContext(script('logic'), box);
  vm.runInContext(script('ui'), box);
  vm.runInContext(playerSource + '; this.Player = Player;', box);
  vm.runInContext(script('player-ui'), box);
  vm.runInContext('this.S = S; this.disconnected = disconnected;', box);
  return { box, el: id => document.getElementById(id), timers, events, media };
}

const reply = (input, text) => input.onmidimessage({ data: new Uint8Array([0xf0, 0x7d, 0x4a, 0x4b, ...Buffer.from(text), 0xf7]) });
function fakeMidi() {
  const input = { id: 'in', name: 'jank', state: 'connected', onmidimessage: null };
  const output = { id: 'out', name: 'jank', state: 'connected', send: () => queueMicrotask(() => reply(input, 'ok\n')) };
  const access = { inputs: new Map([['in', input]]), outputs: new Map([['out', output]]), onstatechange: null };
  return { input, output, access };
}
const shown = (page, id) => !page.el(id).classList.contains('hide');
const pressed = page => ['c-sim', 'c-midi', 'c-bluetooth'].map(id => page.el(id).getAttribute('aria-pressed'));
const settle = () => new Promise(resolve => setImmediate(resolve));
let checks = 0;
const errors = [];
process.on('uncaughtException', e => errors.push(e));
process.on('unhandledRejection', e => errors.push(e));
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };

(async () => {
  // at load
  let page = makePage();
  eq(pressed(page), ['true', 'false', 'false'], 'Demo is pressed at load');
  eq([shown(page, 'c-advanced-box'), shown(page, 'player'), shown(page, 'advanced-extra')], [false, false, false], 'Demo: no Advanced check box, no player');

  // a failed connect: no MIDI at all
  page = makePage();
  page.box.navigator.requestMIDIAccess = () => Promise.reject(new Error('nope'));
  page.el('c-midi').click();
  await settle();
  eq(pressed(page), ['true', 'false', 'false'], 'a failed connect leaves Demo pressed');
  eq(page.el('msg').textContent, 'nope', 'and says why');
  eq(page.el('msg').classList.contains('bad'), true, 'as an error');

  // no port at all
  page = makePage();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve({ inputs: new Map(), outputs: new Map() });
  page.el('c-midi').click();
  await settle();
  eq(pressed(page), ['true', 'false', 'false'], 'no port: Demo stays pressed');
  assert.match(page.el('msg').textContent, /No MIDI port/);
  checks++;

  // connect, then the port goes away
  page = makePage();
  let midi = fakeMidi();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve(midi.access);
  page.el('c-midi').click();
  for (let i = 0; i < 10; i++) await settle();
  eq(pressed(page), ['false', 'true', 'false'], 'connected: MIDI port is the pressed one');
  eq(page.el('msg').textContent, '', 'no "Connected." text');
  ok(page.timers.size > 0, 'timers run while connected');
  midi.access.inputs.delete('in');
  midi.access.onstatechange();
  eq(pressed(page), ['true', 'false', 'false'], 'the port went away: back to Demo');
  eq(page.timers.size, 0, 'no timer left');
  eq(midi.input.onmidimessage, null, 'the input is released');
  eq(page.el('msg').textContent, "The jank's MIDI port went away.", 'and it says so');
  eq(page.box.S.line, null, 'no line left');

  // pressing the active MIDI port button again leaves it
  page = makePage();
  midi = fakeMidi();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve(midi.access);
  page.el('c-midi').click();
  for (let i = 0; i < 10; i++) await settle();
  eq(pressed(page), ['false', 'true', 'false'], 'connected again');
  page.el('c-midi').click();
  eq(pressed(page), ['true', 'false', 'false'], 'the active button pressed again: Demo');
  eq(page.timers.size, 0, 'no timer left after the second press');

  // pressing Demo leaves it too
  page = makePage();
  midi = fakeMidi();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve(midi.access);
  page.el('c-midi').click();
  for (let i = 0; i < 10; i++) await settle();
  page.el('c-sim').click();
  eq(pressed(page), ['true', 'false', 'false'], 'Demo pressed: Demo');
  eq(page.timers.size, 0, 'no timer left after Demo');
  page.el('c-sim').click();
  eq(pressed(page), ['true', 'false', 'false'], 'Demo pressed while on Demo: nothing changes');
  // Advanced: only over USB MIDI, and it owns the Player card
  page = makePage();
  midi = fakeMidi();
  page.box.navigator.requestMIDIAccess = () => Promise.resolve(midi.access);
  page.el('c-midi').click();
  for (let i = 0; i < 10; i++) await settle();
  eq([shown(page, 'c-advanced-box'), page.el('c-advanced').checked, shown(page, 'player'), shown(page, 'advanced-extra')], [true, false, false, false], 'USB: the Advanced check box shows, unchecked, and the player stays hidden');
  page.el('c-advanced').checked = true;
  page.el('c-advanced').fire('change', {});
  eq([shown(page, 'player'), shown(page, 'advanced-extra')], [true, true], 'Advanced checked: the player and the extra room show');
  page.el('pl-file').fire('change', { target: { files: [{ name: 'a.wav' }] } });
  eq(page.el('pl-name').textContent, 'a.wav', 'a file is loaded');
  eq(page.el('pl-play').disabled, false, 'Play is enabled');
  const tagged = Buffer.concat([Buffer.from('ID3'), Buffer.from([3, 0, 0, 0, 0, 0, 17]), Buffer.from('TBPM'), Buffer.from([0, 0, 0, 7, 0, 0, 0]), Buffer.from('124.00')]);
  const fileOf = (name, data) => ({ name, slice: (from, to) => ({ arrayBuffer: async () => Uint8Array.from(data.subarray(from, to)).buffer }) });
  page.el('pl-file').fire('change', { target: { files: [fileOf('t.mp3', tagged)] } });
  for (let i = 0; i < 5; i++) await settle();
  eq(page.el('pl-bpm').textContent, 'tag BPM 124.00 \u00d7 pitch \u2192 124.00', 'the tag BPM of the playing file, at pitch 0');
  page.el('pl-pitch').value = '8';
  page.el('pl-pitch').fire('input', {});
  eq(page.el('pl-bpm').textContent, 'tag BPM 124.00 \u00d7 pitch \u2192 133.92', 'it follows the pitch slider (plain product)');
  page.el('pl-pitch').value = '0';
  page.el('pl-pitch').fire('input', {});
  page.el('pl-file').fire('change', { target: { files: [fileOf('u.wav', Buffer.from('RIFF....WAVE'))] } });
  for (let i = 0; i < 5; i++) await settle();
  eq([page.el('pl-bpm').textContent, shown(page, 'pl-bpm')], ['no BPM tag', true], 'a file without a tag: "no BPM tag"');
  page.el('pl-audio').paused = false;
  page.el('c-advanced').checked = false;
  page.el('c-advanced').fire('change', {});
  eq(shown(page, 'player'), false, 'Advanced unchecked: the player hides');
  eq([page.media.paused, page.media.revoked], [1, ['blob:test', 'blob:test', 'blob:test']], 'and playback stopped; each earlier file was revoked when the next loaded, and the last on release');
  eq(page.el('pl-play').disabled, true, 'Play is disabled again');
  page.el('c-advanced').checked = true;
  page.el('c-advanced').fire('change', {});
  page.el('pl-file').fire('change', { target: { files: [{ name: 'b.wav' }] } });
  page.el('pl-audio').paused = false;
  midi.access.inputs.delete('in');
  midi.access.onstatechange();
  eq([shown(page, 'c-advanced-box'), shown(page, 'player'), page.media.paused, page.media.revoked.length], [false, false, 2, 4], 'the stick going away: Demo, no Advanced box, the player stopped and released');
  eq(pressed(page), ['true', 'false', 'false'], 'and Demo is pressed');
  // over Bluetooth there is no Advanced
  page = makePage();
  vm.runInContext('setConnected("ble", true)', page.box);
  eq([shown(page, 'c-advanced-box'), shown(page, 'player')], [false, false], 'Bluetooth: no Advanced check box, no player');
  page.el('c-advanced').checked = true;
  page.el('c-advanced').fire('change', {});
  eq(shown(page, 'player'), false, 'Bluetooth: a ticked Advanced still shows no player');
  eq(errors.map(e => String(e && e.stack || e)), [], 'no error thrown anywhere');

  console.log('connect: ' + checks + ' checks passed');
})().catch(e => { console.error(e); process.exit(1); });
function ok(c, what) { checks++; assert.ok(c, what); }
