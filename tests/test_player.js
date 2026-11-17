// SPDX-License-Identifier: MIT
// The control page's PLAYER card helpers (ui/player.js): varispeed rate, the pitch and time texts, the output choice.
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const P = new Function(fs.readFileSync(path.join(__dirname, '..', 'ui', 'player.js'), 'utf8') + '; return Player;')();
let checks = 0;
const ok2 = (c, what) => { checks++; assert.ok(c, what); };
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };

eq(P.pitchToRate(0), 1, 'no pitch: rate 1');
eq(P.pitchToRate(8), 1.08, '+8 % is rate 1.08');
eq(P.pitchToRate(-8), 0.92, '-8 % is rate 0.92');
eq(P.pitchToRate(50), 1.08, 'clamped at +8 %');
eq(P.pitchToRate(-50), 0.92, 'clamped at -8 %');
eq(P.pitchToRate('abc'), 1, 'not a number: rate 1');
eq(Math.round(120 * P.pitchToRate(2.5) * 100) / 100, 123, 'the BPM moves with it: 120 at +2.5 % is 123');

eq(P.formatPitch(0), '+0.00 %', 'zero');
eq(P.formatPitch(1.5), '+1.50 %', 'plus sign');
eq(P.formatPitch(-0.25), '-0.25 %', 'minus');
eq(P.formatPitch(9), '+8.00 %', 'clamped');

eq(P.formatTime(0), '0:00', 'start');
eq(P.formatTime(65.9), '1:05', 'floors the seconds');
eq(P.formatTime(600), '10:00', 'ten minutes');
eq(P.formatTime(NaN), '0:00', 'NaN');
eq(P.formatTime(Infinity), '0:00', 'a stream with no length');
eq(P.formatTime(-3), '0:00', 'negative');

const devices = [
  { kind: 'audioinput', label: 'jank mic', deviceId: 'in1' },
  { kind: 'audiooutput', label: 'MacBook Speakers', deviceId: 'spk' },
  { kind: 'audiooutput', label: 'JANK USB Audio (1234:5678)', deviceId: 'jk' },
  { kind: 'videoinput', label: 'jank cam', deviceId: 'cam' },
];
eq(P.pickOutput(devices), 'jk', 'finds the stick by name, any case, outputs only');
eq(P.pickOutput(devices, 'speakers'), 'spk', 'another needle');
eq(P.pickOutput([devices[0], devices[1]]), null, 'no jank output');
eq(P.pickOutput([{ kind: 'audiooutput', label: '', deviceId: '' }]), null, 'labels empty until permission');
eq(P.pickOutput(undefined), null, 'no list');
eq(P.outputChoices(devices), [{ id: 'spk', label: 'MacBook Speakers' }, { id: 'jk', label: 'JANK USB Audio (1234:5678)' }], 'only outputs, with labels');
eq(P.outputChoices([{ kind: 'audiooutput', label: '', deviceId: 'a' }, { kind: 'audiooutput', label: '', deviceId: 'b' }]),
   [{ id: 'a', label: 'Output 1' }, { id: 'b', label: 'Output 2' }], 'no label: numbered');

eq(P.volumeFromPct(80), 0.8, 'volume 80 % is 0.8');
eq(P.volumeFromPct(150), 1, 'volume clamped high');
eq(P.volumeFromPct(-5), 0, 'volume clamped low');
eq(P.parseVolumePct(null), 80, 'nothing stored: the default 80 %');
eq(P.parseVolumePct('35'), 35, 'a stored 35');
eq(P.parseVolumePct('abc'), 80, 'junk: the default');
eq(P.parseVolumePct('250'), 100, 'stored out of range: clamped');

eq(P.resolveOutput(devices, 'spk'), { id: 'jk', label: 'JANK USB Audio (1234:5678)', why: 'jank' }, 'jank wins over the remembered device');
eq(P.resolveOutput([devices[1]], 'spk'), { id: 'spk', label: 'MacBook Speakers', why: 'saved' }, 'no jank: the remembered device');
eq(P.resolveOutput([devices[1]], 'gone'), { id: null, label: 'none', why: 'default' }, 'remembered device unplugged: none (monitor only)');
eq(P.resolveOutput([], null), { id: null, label: 'none', why: 'default' }, 'no outputs listed');

eq(P.isAudioFile('A.MP3') && P.isAudioFile('x.flac') && P.isAudioFile('y.aiff') && P.isAudioFile('z.opus'), true, 'audio extensions, any case');
eq(P.isAudioFile('cover.jpg') || P.isAudioFile('notes') || P.isAudioFile('.mp3x'), false, 'not audio');
eq(P.sortEntries([
  { name: 'track10.mp3', kind: 'file' }, { name: 'track2.mp3', kind: 'file' }, { name: 'cover.jpg', kind: 'file' },
  { name: 'B side', kind: 'directory' }, { name: 'a side', kind: 'directory' }, { name: '.hidden', kind: 'directory' }, { name: 'Track1.wav', kind: 'file' },
]).map(e => e.name), ['a side', 'B side', 'Track1.wav', 'track2.mp3', 'track10.mp3'], 'folders first, natural order, audio only, no dotfiles');
eq(P.breadcrumbs([]), [{ label: '/', depth: 0 }], 'root');
eq(P.breadcrumbs(['Music', 'House']), [{ label: '/', depth: 0 }, { label: 'Music', depth: 1 }, { label: 'House', depth: 2 }], 'a path');
eq(P.rowsToShow(2000), 500, 'at most 500 rows');
eq(P.rowsToShow(12), 12, 'few rows: all');

const tree = P.buildTree([
  { path: 'Music/a.mp3', file: 'fa' }, { path: 'Music/House/b.wav', file: 'fb' }, { path: 'Music/House/Deep/c.ogg', file: 'fc' }, { path: 'Music/House/d.txt', file: 'fd' },
]);
eq(tree.name, 'Music', 'the chosen folder is the root');
eq(P.listTree(tree, []).map(e => e.name + ':' + e.kind), ['House:directory', 'a.mp3:file'], 'root: folders then files');
eq(P.listTree(tree, ['House']).map(e => e.name), ['Deep', 'b.wav', 'd.txt'], 'one level down');
eq(P.listTree(tree, ['House', 'Deep']).map(e => e.file), ['fc'], 'two levels down, the File kept');
eq(P.listTree(tree, ['Nope']), [], 'no such folder');

eq(P.rmsOf([]), 0, 'no samples: rms 0');
eq(P.rmsOf([0.5, -0.5, 0.5, -0.5]), 0.5, 'rms of a square wave');
const sine = Array.from({ length: 4800 }, (_, i) => 0.1414 * Math.sin(2 * Math.PI * 10 * i / 4800));
ok2(Math.abs(P.rmsToDb(P.rmsOf(sine)) - -20) < 0.1, 'a sine of amplitude 0.1414 reads -20 dBFS rms');
eq(P.rmsToDb(0), -90, 'silence: the floor');
eq(P.rmsToDb(1e-9), -90, 'below the floor: the floor');
eq(Math.round(P.rmsToDb(1)), 0, 'full scale: 0 dB');
ok2(Math.abs(P.smoothDb(-90, -20, 150) - (-90 + 70 * (1 - Math.exp(-1)))) < 1e-9, 'one time constant covers 63 % of the step');
eq(P.smoothDb(-40, -20, 0), -40, 'no time: no move');
ok2(P.smoothDb(-90, -20, 100000) > -20.001, 'long time: arrives');
eq([P.meterFraction(-90), P.meterFraction(0), P.meterFraction(-45), P.meterFraction(-200)], [0, 1, 0.5, 0], 'the meter bar fraction');
const base = { hasFile: true, paused: false, position: 12.4, length: 220, monitor: true, jank: 'JANK USB Audio', jankError: '', volumePct: 80 };
eq(P.statusText({ ...base, hasFile: false }), 'no file', 'status: no file');
eq(P.statusText({ ...base, paused: true, position: 0 }), 'stopped', 'status: stopped');
eq(P.statusText({ ...base, paused: true }), 'paused 0:12 / 3:40', 'status: paused');
eq(P.statusText(base), 'playing 0:12 / 3:40 \u00b7 monitor + JANK USB Audio', 'status: both outputs');
eq(P.statusText({ ...base, jank: '' }), 'playing 0:12 / 3:40 \u00b7 no stick output chosen: monitor only', 'status: no stick output chosen');
eq(P.statusText({ ...base, monitor: false }), 'playing 0:12 / 3:40 \u00b7 JANK USB Audio only (monitor off)', 'status: jank only');
eq(P.statusText({ ...base, jank: '', monitor: false }), 'playing 0:12 / 3:40 \u00b7 no output: no stick output chosen and monitor off', 'status: nowhere');
eq(P.statusText({ ...base, jankError: 'NotAllowedError' }), 'playing 0:12 / 3:40 \u00b7 jank output failed: NotAllowedError, monitor only', 'status: the jank path failed, the monitor plays on');
eq(P.statusText({ ...base, volumePct: 0 }), 'playing 0:12 / 3:40 \u00b7 monitor + JANK USB Audio \u00b7 no sound: volume is 0', 'status: volume 0 (the monitor)');
eq(P.statusText({ ...base, volumePct: 0, monitor: false }), 'playing 0:12 / 3:40 \u00b7 JANK USB Audio only (monitor off)', 'volume 0 with the monitor off: not a "no sound" case, the stick still plays');
eq(P.statusText({ ...base, error: 'NotAllowedError' }), 'error: NotAllowedError', 'status: an error wins');

// ---- what follows a track ----
eq([P.nextTrack(3, 0), P.nextTrack(3, 2), P.nextTrack(0, -1), P.nextTrack(1, 0)], [1, -1, -1, -1], 'next: the following file, none after the last or in an empty list');
const seen = new Set();
for (let i = 0; i < 20; i++) seen.add(P.randomTrack(5, 2, () => i / 20));
eq([...seen].includes(2), false, 'random never picks the playing track');
eq(P.randomTrack(5, 2, () => 0), 0, 'random, rng 0: the first other file');
eq(P.randomTrack(5, 2, () => 0.9999999), 4, 'random, rng just under 1: the last other file');
eq(P.randomTrack(5, 4, () => 0.9999999), 3, 'random, the playing one is the last: still another one');
eq(P.randomTrack(5, 0, () => 1), 4, 'random, rng 1 (out of range): clamped to the last other file');
eq([P.randomTrack(1, 0, () => 0.5), P.randomTrack(0, -1, () => 0.5)], [-1, -1], 'random: a single-file or empty list has no other file');
eq(P.randomTrack(2, 0, () => 0.3), 1, 'random: two files, the other one');
eq([P.afterTrack('next', 3, 1), P.afterTrack('next', 3, 2), P.afterTrack('stop', 3, 0), P.afterTrack('random', 3, 0, () => 0)], [2, -1, -1, 1], 'then: next, next at the last file stops, stop ends, random');
eq([P.skipTrack('next', 3, 1), P.skipTrack('next', 3, 2), P.skipTrack('next', 1, 0), P.skipTrack('stop', 3, 0), P.skipTrack('random', 3, 1, () => 0)], [2, 0, -1, 1, 0], 'the Next button: the next file, back to the first after the last, nothing for a single file; random picks another');
eq([P.parseAfterMode('random'), P.parseAfterMode('stop'), P.parseAfterMode('next'), P.parseAfterMode(null), P.parseAfterMode('x')], ['random', 'stop', 'next', 'next', 'next'], 'the remembered "then": junk falls back to next');
eq(P.sortEntries([{ name: 'b10.mp3', kind: 'file' }, { name: 'b2.mp3', kind: 'file' }, { name: 'a.mp3', kind: 'file' }]).map(e => e.name), ['a.mp3', 'b2.mp3', 'b10.mp3'], 'the folder listing stays natural-sorted (the list random and next index into)');

// ---- what this browser has ----
const env = (over) => ({ captureStream: 'function', mozCaptureStream: 'undefined', audioContext: 'function', audioContextSetSinkId: 'function', elementSetSinkId: 'function', ...over });
eq(P.playbackPlan(env({})), { path: 'captureStream', stick: 'context', missing: ['audio.mozCaptureStream'] }, 'everything there: the captureStream path, the stick through the context');
eq(P.playbackPlan(env({ captureStream: 'undefined' })).path, 'webaudio', 'no captureStream: the Web Audio path');
eq(P.playbackPlan(env({ captureStream: 'undefined', mozCaptureStream: 'function' })), { path: 'captureStream', stick: 'context', missing: ['audio.captureStream'] }, 'mozCaptureStream counts as captureStream');
eq(P.playbackPlan(env({ captureStream: 'undefined' })), { path: 'webaudio', stick: 'element', missing: ['audio.captureStream', 'audio.mozCaptureStream'] }, 'Web Audio path: the stick through a hidden element when it has setSinkId');
eq(P.playbackPlan(env({ captureStream: 'undefined', elementSetSinkId: 'undefined' })).stick, 'context', 'Web Audio path without element setSinkId: a second context');
eq(P.playbackPlan(env({ captureStream: 'undefined', elementSetSinkId: 'undefined', audioContextSetSinkId: 'undefined' })).stick, 'none', 'no setSinkId anywhere: no way to the stick');
eq(P.playbackPlan(env({ audioContextSetSinkId: 'undefined' })).path, 'webaudio', 'captureStream but a context that cannot choose its output: the Web Audio path');
eq(P.playbackPlan(env({ audioContextSetSinkId: 'undefined' })).stick, 'element', '... which reaches the stick through the element');
eq(P.playbackPlan(env({ audioContext: 'undefined' })), { path: 'none', stick: 'none', missing: ['audio.mozCaptureStream', 'AudioContext'] }, 'no AudioContext: no path');
eq(P.planNote(P.playbackPlan(env({}))), '', 'nothing to say when both outputs work');
eq(P.planNote(P.playbackPlan(env({ captureStream: 'undefined' }))), '', 'the Web Audio path works too: nothing to say');
assert.match(P.planNote(P.playbackPlan(env({ captureStream: 'undefined', elementSetSinkId: 'undefined', audioContextSetSinkId: 'undefined' }))), /no setSinkId.*only the monitor plays/); checks++;
assert.match(P.planNote(P.playbackPlan(env({ audioContext: 'undefined' }))), /no Web Audio/); checks++;
const plan = P.playbackPlan(env({ captureStream: 'undefined' }));
eq(P.planDetails(plan, env({ captureStream: 'undefined' }), 'Google Chrome 126'),
   'path: webaudio · stick output: element · audio.captureStream: absent (undefined) · audio.mozCaptureStream: absent (undefined) · AudioContext: present · AudioContext.prototype.setSinkId: present · HTMLMediaElement.prototype.setSinkId: present · browser: Google Chrome 126', 'the details line names the path, every API and the browser');
eq(P.browserBrand({ brands: [{ brand: 'Not.A/Brand', version: '8' }, { brand: 'Chromium', version: '126' }, { brand: 'Google Chrome', version: '126' }] }, 'UA'), 'Google Chrome 126', 'the real brand, not the GREASE one or Chromium');
eq(P.browserBrand({ brands: [{ brand: 'Not;A=Brand', version: '99' }, { brand: 'Chromium', version: '126' }, { brand: 'Microsoft Edge', version: '126' }] }, 'UA'), 'Microsoft Edge 126', 'Edge');
eq(P.browserBrand({ brands: [{ brand: 'Not.A/Brand', version: '8' }, { brand: 'Chromium', version: '126' }] }, 'UA'), 'Chromium 126', 'only Chromium');
eq(P.browserBrand(undefined, 'Mozilla/5.0 (Macintosh) Safari/605.1.15'), 'Mozilla/5.0 (Macintosh) Safari/605.1.15', 'no userAgentData: the userAgent');
eq(P.browserBrand(undefined, undefined), 'unknown', 'nothing known');

// ---- the stick's output without the microphone ----
const listed = [{ kind: 'audiooutput', label: '', deviceId: 'spk' }, { kind: 'audiooutput', label: '', deviceId: 'jk' }];
eq(P.matchSaved(listed, { id: 'jk', label: 'JANK USB Audio' }), { id: 'jk', label: 'Output 2' }, 'the saved id is found in a list with no labels');
eq(P.matchSaved([{ kind: 'audiooutput', label: 'JANK USB Audio', deviceId: 'new' }], { id: 'old', label: 'JANK USB Audio' }), { id: 'new', label: 'JANK USB Audio' }, 'the saved label is found when the id changed and labels are there');
eq([P.matchSaved(listed, { id: 'gone' }), P.matchSaved(listed, null), P.matchSaved([], { id: 'jk' })], [null, null, null], 'a device that is not there: none');
eq([P.outputButton({ working: true, hasSelectAudioOutput: true }), P.outputButton({ working: true, hasSelectAudioOutput: false })], ['none', 'none'], 'a chosen device that works: no button');
eq(P.outputButton({ working: false, hasSelectAudioOutput: true }), 'choose', 'nothing chosen, the browser has an output chooser: the chooser button');
eq(P.outputButton({ working: false, hasSelectAudioOutput: false }), 'allow', 'no chooser: only then the explicit microphone button');
eq(P.OUTPUT_HELP.choose, "Choose the stick's output (jank) once.", 'the line that says what the click does');
ok2(/microphone only to read the device names; nothing is recorded/.test(P.OUTPUT_HELP.allow), 'the microphone button says why');
ok2(!/microphone/i.test(P.OUTPUT_HELP.choose + P.OUTPUT_BUTTON_TEXT.choose), 'the normal path never mentions the microphone');

// ---- which output, and what the details say ----
const dflt = [{ kind: 'audiooutput', label: 'Default - JANK USB Audio', deviceId: 'default' }, { kind: 'audiooutput', label: 'MacBook Speakers', deviceId: 'spk' }, { kind: 'audiooutput', label: 'jank (303a:4a4b)', deviceId: 'abcdef0123456789' }];
eq(P.pickOutput(dflt), 'abcdef0123456789', 'a named jank device is preferred over the system default entry');
eq(P.pickOutput([dflt[0]]), 'default', 'when only the default entry names jank, it is used');
eq(P.pickOutput([{ kind: 'audiooutput', label: 'jank (303a:4a4b)', deviceId: 'x' }]), 'x', "'jank (303a:4a4b)' matches, any case");
eq([P.savedStatus(dflt, { id: 'spk' }), P.savedStatus(dflt, { id: 'gone', label: 'nothing' }), P.savedStatus(dflt, { id: 'new', label: 'MacBook Speakers' }), P.savedStatus(dflt, null)], ['found', 'stale', 'found', 'none'], 'saved output: found by id, stale, found by label, none');
eq(P.savedStatus([{ kind: 'audiooutput', label: '', deviceId: '' }], { id: 'jk' }), 'unknown', 'no ids listed (no permission): only tried, never dropped');
eq([P.sinkMatches('jk', 'jk'), P.sinkMatches('', 'jk'), P.sinkMatches({ type: 'none' }, 'jk')], [true, false, false], 'AudioContext.sinkId read back');
eq(P.outputReport({ permission: 'granted', devices: dflt, saved: { id: 'abcdef0123456789', label: 'jank (303a:4a4b)' }, chosen: { id: 'abcdef0123456789', label: 'jank (303a:4a4b)' }, ctxState: 'running', ctxSinkId: 'abcdef0123456789', lastError: '' }),
   'permission: granted · outputs: Default - JANK USB Audio (default), MacBook Speakers (spk), jank (303a:4a4b) (abcdef01) · saved: abcdef01 "jank (303a:4a4b)" · chosen: abcdef01 "jank (303a:4a4b)" · stick context: running, sinkId abcdef01 · last sink error: none', 'the details: permission, outputs with 8 chars of the id, saved, chosen, the context, the last error');
eq(P.outputReport({ devices: [], ctxState: '', lastError: 'NotFoundError' }), 'permission: unknown · outputs: none listed · saved: none · chosen: none · stick context: none yet · last sink error: NotFoundError', 'the details with nothing known yet');

(async () => {
// ---- the play scope ----
const tree = {
  '': [{ name: 'b2.mp3', kind: 'file' }, { name: 'Sub', kind: 'directory' }, { name: 'a.mp3', kind: 'file' }, { name: '.hidden.mp3', kind: 'file' }, { name: '__MACOSX', kind: 'directory' }, { name: '$RECYCLE.BIN', kind: 'directory' }, { name: 'b10.mp3', kind: 'file' }, { name: 'cover.jpg', kind: 'file' }, { name: '._a.mp3', kind: 'file' }],
  Sub: [{ name: 'z.flac', kind: 'file' }, { name: 'Deep', kind: 'directory' }, { name: 'notes.txt', kind: 'file' }],
  'Sub/Deep': [{ name: 'd1.ogg', kind: 'file' }],
  __MACOSX: [{ name: 'junk.mp3', kind: 'file' }], '$RECYCLE.BIN': [{ name: 'trash.mp3', kind: 'file' }],
};
const listed = [];
const walkAll = async (over = {}) => P.scanTree({ root: {}, list: async (dir, path) => { listed.push(path.join('/')); return tree[path.join('/')] || []; }, shouldStop: () => false, yieldFn: async () => {}, onProgress: () => {}, ...over });
let scan = await walkAll();
eq(scan.files.map(f => f.path.join('/')), ['Sub/Deep/d1.ogg', 'Sub/z.flac', 'a.mp3', 'b2.mp3', 'b10.mp3'], 'depth-first, folders first, natural order, audio only, junk skipped');
eq([scan.capped, scan.cancelled, listed.filter(x => /MACOSX|RECYCLE/.test(x)).length], [false, false, 0], 'junk folders are not even listed');
scan = await walkAll({ cap: 3 });
eq([scan.files.length, scan.capped], [3, true], 'a cap of 3 files: capped');
let stopAfter = 0;
scan = await walkAll({ yieldFn: async () => { stopAfter++; }, shouldStop: () => stopAfter >= 2 });
eq([scan.cancelled, scan.files.length < 5], [true, true], 'cancelled between directories: it stops and says so');
let yields = 0;
await walkAll({ yieldFn: async () => { yields++; } });
eq(yields, 3, 'the page gets a turn after each directory (3 here)');
const progress = [];
await walkAll({ onProgress: n => progress.push(n) });
eq(progress, [1, 2, 5], 'progress counts the files found so far, folder by folder');
eq(P.SCOPE_CAP, 100000, 'the cap is 100,000 files');
eq([P.scopeLine({ scanning: true, count: 1234 }), P.scopeLine({ scanning: false, count: 5, capped: false }), P.scopeLine({ scanning: false, count: 100000, capped: true })], ['scanning: 1,234 files', '5 files in scope', '100,000 files (capped)'], 'the live line');
eq([P.isJunkName('.Trash'), P.isJunkName('._x.mp3'), P.isJunkName('$RECYCLE.BIN'), P.isJunkName('__MACOSX'), P.isJunkName('Music')], [true, true, true, true, false], 'junk names');
const order = scan2 => scan2.files.map(f => f.path.join('/'));
const full = await walkAll();
const listLen = full.files.length;
eq([P.afterTrack('next', listLen, listLen - 1), P.skipTrack('next', listLen, listLen - 1)], [-1, 0], 'the scope: a track ending on the last file stops, the Next button wraps');
const picks = new Set();
for (let i = 0; i < 100; i++) picks.add(P.randomTrack(listLen, 2, () => i / 100));
eq([picks.has(2), picks.size], [false, listLen - 1], 'random over the whole scope: never the one just played, any other can come');

  // ---- Play with nothing loaded: the first track ----
  eq([P.firstTrack('random', 10, () => 0), P.firstTrack('random', 10, () => 0.9999999), P.firstTrack('random', 10, () => 1), P.firstTrack('random', 10, () => 0.5)], [0, 9, 9, 5], 'random: the rng picks any file of the list, edge values 0 and just under 1 (and an out of range 1 is clamped)');
  eq([P.firstTrack('next', 10), P.firstTrack('stop', 10), P.firstTrack('next', 1), P.firstTrack('random', 1, () => 0.7)], [0, 0, 0, 0], 'next and stop: the first file; a single file: that file');
  eq([P.firstTrack('random', 0, () => 0.5), P.firstTrack('next', 0), P.firstTrack('stop', 0)], [-1, -1, -1], 'an empty list: nothing starts');
  eq(P.scopeLine({ scanning: false, count: 0, done: true, root: 'Music' }), 'no audio files under Music', 'an empty scope after the scan says so');
  eq(P.scopeLine({ scanning: false, count: 0, done: false, root: 'Music' }), '0 files in scope', 'a scope never scanned (or cancelled) does not claim it is empty');
  const seenFiles = [];
  await P.scanTree({ root: {}, list: async (dir, path) => tree[path.join('/')] || [], shouldStop: () => false, yieldFn: async () => {}, onProgress: (n, files) => seenFiles.push([n, files.length]), cap: 100 });
  eq(seenFiles, [[1, 1], [2, 2], [5, 5]], 'the progress callback also hands over the files found so far');

  // ---- random and next over a tree of three folders: any folder, not the playing one's ----
  const three = {
    '': [{ name: 'A', kind: 'directory' }, { name: 'B', kind: 'directory' }, { name: 'C', kind: 'directory' }],
    A: [{ name: 'a1.mp3', kind: 'file' }, { name: 'a2.mp3', kind: 'file' }], B: [{ name: 'b1.mp3', kind: 'file' }, { name: 'b2.mp3', kind: 'file' }], C: [{ name: 'c1.mp3', kind: 'file' }],
  };
  const scopeScan = await P.scanTree({ root: {}, list: async (dir, path) => three[path.join('/')] || [], shouldStop: () => false, yieldFn: async () => {}, onProgress: () => {} });
  const folderOf = i => scopeScan.files[i].path[0], count3 = scopeScan.files.length;
  eq(scopeScan.files.map(f => f.path.join('/')), ['A/a1.mp3', 'A/a2.mp3', 'B/b1.mp3', 'B/b2.mp3', 'C/c1.mp3'], 'the scope lists the files of all three folders in one list');
  const folders = new Set(), neverSame = [];
  for (let from = 0; from < count3; from++) for (let k = 0; k < 40; k++) {
    const to = P.randomTrack(count3, from, () => k / 40);
    folders.add(folderOf(to));
    neverSame.push(to !== from);
  }
  eq([...folders].sort(), ['A', 'B', 'C'], 'random over the scope reaches all three folders');
  ok2(neverSame.every(Boolean), 'and never the track that is playing');
  eq(folderOf(P.randomTrack(count3, 0, () => 0.9999999)) !== 'A', true, 'from a file in A, random can land in another folder (C here)');
  eq(folderOf(P.afterTrack('next', count3, 1)), 'B', 'a track ending in A\'s last file: next goes on into B');
  eq(folderOf(P.afterTrack('random', count3, 4, () => 0)), 'A', 'from the last file, random can go back to A');
  eq(folderOf(P.skipTrack('next', count3, 4)), 'A', 'the Next button wraps from the last folder to the first');

  console.log('player: ' + checks + ' checks passed');
})().catch(e => { console.error(e); process.exit(1); });
