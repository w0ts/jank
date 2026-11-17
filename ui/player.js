/* Pure helpers of the control page's PLAYER card (no DOM): the varispeed pitch, the volume, the time text, the output
   choice, the tempo tag of the playing file, and the music folder's listing (natural sort, audio filter, breadcrumbs, a tree from webkitRelativePath).
   The simulator's own player runs on the Mac (beat/live/player.py); this one plays files in the browser. */
const Player = (() => {
  const PITCH_RANGE_PCT = 8, DEFAULT_VOLUME_PCT = 80, MAX_ROWS = 500, LEVEL_FLOOR_DB = -90, LEVEL_TAU_MS = 150;
  const AUDIO_EXT = ['mp3', 'm4a', 'aac', 'wav', 'flac', 'ogg', 'opus', 'aiff'];
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const collator = new Intl.Collator('en', { numeric: true, sensitivity: 'base' });

  // varispeed: the playback rate is 1 + pitch / 100 (with preservesPitch off the tempo and the pitch move together)
  const pitchToRate = pct => 1 + clamp(Number(pct) || 0, -PITCH_RANGE_PCT, PITCH_RANGE_PCT) / 100;
  const formatPitch = pct => { const p = clamp(Number(pct) || 0, -PITCH_RANGE_PCT, PITCH_RANGE_PCT); return (p >= 0 ? '+' : '') + p.toFixed(2) + ' %'; };
  const volumeFromPct = pct => (Number.isFinite(Number(pct)) ? clamp(Number(pct), 0, 100) / 100 : DEFAULT_VOLUME_PCT / 100);
  // a stored percentage, or the default when nothing usable is stored
  const parseVolumePct = text => { const v = text === null || text === undefined || text === '' ? NaN : Number(text); return Number.isFinite(v) ? clamp(Math.round(v), 0, 100) : DEFAULT_VOLUME_PCT; };

  function formatTime(seconds) {
    const s = Number.isFinite(seconds) && seconds > 0 ? Math.floor(seconds) : 0;
    return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0');
  }

  // the file's level as the device's IN bar reads it: RMS of the last samples in dBFS, floored at -90
  function rmsOf(samples) {
    if (!samples || samples.length === 0) return 0;
    let sum = 0;
    for (let i = 0; i < samples.length; i++) sum += samples[i] * samples[i];
    return Math.sqrt(sum / samples.length);
  }
  const rmsToDb = rms => (rms > 0 ? Math.max(LEVEL_FLOOR_DB, 20 * Math.log10(rms)) : LEVEL_FLOOR_DB);
  // an exponential ease toward the new level (in dB), `dtMs` after the last one
  const smoothDb = (prev, next, dtMs, tauMs) => prev + (next - prev) * (1 - Math.exp(-Math.max(0, dtMs) / (tauMs || LEVEL_TAU_MS)));
  const meterFraction = db => clamp((db - LEVEL_FLOOR_DB) / -LEVEL_FLOOR_DB, 0, 1);

  // ---- what this browser can do for the two outputs ----
  // env: the `typeof` of each API: { captureStream, mozCaptureStream, audioContext, audioContextSetSinkId, elementSetSinkId }
  // path: 'captureStream' (the element plays on the default output, its captureStream feeds the stick's context: the tested path),
  // 'webaudio' (the element goes through Web Audio: a gain for the monitor, the analyser, a stream to the stick), 'none' (no Web Audio).
  // stick: how the stick's output is reached: 'context' (AudioContext.setSinkId), 'element' (a hidden <audio> with setSinkId), 'none'.
  const APIS = [['captureStream', 'audio.captureStream'], ['mozCaptureStream', 'audio.mozCaptureStream'], ['audioContext', 'AudioContext'],
                ['audioContextSetSinkId', 'AudioContext.prototype.setSinkId'], ['elementSetSinkId', 'HTMLMediaElement.prototype.setSinkId']];
  function playbackPlan(env) {
    const has = key => env[key] === 'function';
    const capture = has('captureStream') || has('mozCaptureStream');
    const path = has('audioContext') && capture && has('audioContextSetSinkId') ? 'captureStream' : has('audioContext') ? 'webaudio' : 'none';
    const stick = path === 'captureStream' ? 'context' : path === 'webaudio' ? (has('elementSetSinkId') ? 'element' : has('audioContextSetSinkId') ? 'context' : 'none') : 'none';
    return { path, stick, missing: APIS.filter(([key]) => !has(key)).map(([, name]) => name) };
  }
  // one plain line when something is missing that matters, '' when both outputs can work
  function planNote(plan) {
    if (plan.path === 'none') return 'This browser has no Web Audio: only the monitor plays, and there is no level meter.';
    if (plan.stick === 'none') return "This browser cannot send sound to a chosen output (no setSinkId): only the monitor plays.";
    return '';
  }
  // the 'details' line the user can paste back: the path in use, every API with its type, and the browser
  function planDetails(plan, env, brand) {
    const apis = APIS.map(([key, name]) => name + ': ' + (env[key] === 'function' ? 'present' : 'absent (' + env[key] + ')')).join(' · ');
    return 'path: ' + plan.path + ' · stick output: ' + plan.stick + ' · ' + apis + ' · browser: ' + brand;
  }
  // "Google Chrome 126" from navigator.userAgentData.brands (the real brand, not the GREASE one), else the start of the userAgent
  function browserBrand(userAgentData, userAgent) {
    const brands = (userAgentData && userAgentData.brands) || [];
    const real = brands.filter(b => b && !/not.?a.?brand/i.test(b.brand) && b.brand !== 'Chromium');
    const pick = real[real.length - 1] || brands.find(b => b && b.brand === 'Chromium');
    return pick ? pick.brand + ' ' + pick.version : String(userAgent || 'unknown').slice(0, 90);
  }

  // where the sound goes: the monitor (this computer's default output, scaled by the volume knob) and the stick's output
  // (the file at its true level): { monitor, jank (label or ''), jankError }
  function routeText(r) {
    if (r.jankError) return 'jank output failed: ' + r.jankError + (r.monitor ? ', monitor only' : '');
    if (r.jank) return r.monitor ? 'monitor + ' + r.jank : r.jank + ' only (monitor off)';
    return r.monitor ? 'no stick output chosen: monitor only' : 'no output: no stick output chosen and monitor off';
  }
  // the status line of the card: { hasFile, paused, position, length, monitor, jank, jankError, volumePct, error } -> text
  function statusText(s) {
    if (s.error) return 'error: ' + s.error;
    if (!s.hasFile) return 'no file';
    if (s.paused) return s.position > 0 ? 'paused ' + formatTime(s.position) + ' / ' + formatTime(s.length) : 'stopped';
    const base = 'playing ' + formatTime(s.position) + ' / ' + formatTime(s.length) + ' \u00b7 ' + routeText(s);
    return s.monitor && s.volumePct === 0 ? base + ' \u00b7 no sound: volume is 0' : base;
  }

  const outputs = devices => (devices || []).filter(d => d.kind === 'audiooutput');
  // the first audio output whose label holds `needle` (the stick's USB audio is named "jank"), or null
  // ("Default - jank (303a:4a4b)" is the system default entry when jank is the default output: a named device is preferred)
  const ALIAS_IDS = ['default', 'communications'];
  function pickOutput(devices, needle) {
    const n = String(needle || 'jank').toLowerCase();
    const hits = outputs(devices).filter(x => String(x.label || '').toLowerCase().includes(n));
    const d = hits.find(x => !ALIAS_IDS.includes(x.deviceId)) || hits[0];
    return d ? d.deviceId : null;
  }
  // [{ id, label }] for the Output select; a device without a label (no media permission yet) is numbered
  const outputChoices = devices => outputs(devices).map((d, i) => ({ id: d.deviceId, label: d.label || 'Output ' + (i + 1) }));
  // Is the saved output still there? 'unknown' when the list shows no ids (no permission yet): then it is only tried, never dropped.
  function savedStatus(devices, saved) {
    if (!saved || !saved.id) return 'none';
    const list = outputs(devices);
    if (!list.some(d => d.deviceId)) return 'unknown';
    return matchSavedIn(list, saved) ? 'found' : 'stale';
  }
  const matchSavedIn = (list, saved) => list.find(d => d.deviceId === saved.id) || (saved.label ? list.find(d => d.label === saved.label) : null) || null;
  // AudioContext.sinkId reads back as an id string, or an object ({ type: 'none' }) for no output
  const sinkMatches = (current, id) => typeof current === 'string' && current === id;
  // the 'details' text of the stick output: everything needed to see why a sound does not reach it
  function outputReport(s) {
    const id8 = id => (id ? String(id).slice(0, 8) : '-');
    const named = x => (x && x.id ? id8(x.id) + (x.label ? ' "' + x.label + '"' : '') : 'none');
    const outs = outputs(s.devices).map(d => (d.label || 'no label') + ' (' + id8(d.deviceId) + ')').join(', ') || 'none listed';
    const ctx = s.ctxState ? s.ctxState + ', sinkId ' + (typeof s.ctxSinkId === 'string' ? (id8(s.ctxSinkId) === '-' ? 'default' : id8(s.ctxSinkId)) : JSON.stringify(s.ctxSinkId)) : 'none yet';
    return ['permission: ' + (s.permission || 'unknown'), 'outputs: ' + outs, 'saved: ' + named(s.saved), 'chosen: ' + named(s.chosen), 'stick context: ' + ctx, 'last sink error: ' + (s.lastError || 'none')].join(' · ');
  }

  // ---- the play scope: every audio file under the picked folder, in depth-first natural order ----
  const SCOPE_CAP = 100000;
  const JUNK_FOLDERS = ['$recycle.bin', '__macosx', 'system volume information', 'lost+found'];
  const isJunkName = name => String(name).startsWith('.') || JUNK_FOLDERS.includes(String(name).toLowerCase());
  const pathKey = path => path.join('/');
  // Walks a directory tree with injected pieces: list(dir, path) -> entries { name, kind, ... }; shouldStop() cancels; yieldFn() gives the
  // page a turn between directories; onProgress(count). Collects audio files only (name + the entry as the source gave it), folders first
  // in each directory, natural order: the order a Next button follows. Stops at `cap` files (capped: true).
  async function scanTree({ root, list, shouldStop, yieldFn, onProgress, cap }) {
    const files = [], max = cap || SCOPE_CAP;
    let capped = false;
    async function walk(dir, path) {
      if (shouldStop() || capped) return;
      const entries = sortEntries((await list(dir, path)).filter(e => !isJunkName(e.name)));
      await yieldFn();
      for (const e of entries) {
        if (shouldStop() || capped) return;
        if (e.kind === 'directory') await walk(e, [...path, e.name]);
        else if (files.length >= max) { capped = true; return; } else files.push({ ...e, path: [...path, e.name] });
      }
      onProgress(files.length);
    }
    await walk(root, []);
    return { files, capped, cancelled: shouldStop() };
  }
  const scopeLine = s => (s.scanning ? 'scanning: ' + s.count.toLocaleString('en-US') + ' files' : s.capped ? s.count.toLocaleString('en-US') + ' files (capped)' : s.count.toLocaleString('en-US') + ' files in scope');

  // the output the user chose last time, found again in a device list (ids first; a label when the list has labels and the id changed)
  function matchSaved(devices, saved) {
    if (!saved || !saved.id) return null;
    const list = outputChoices(devices);
    return list.find(c => c.id === saved.id) || (saved.label ? list.find(c => c.label === saved.label) : null) || null;
  }
  // which button the card offers for the stick's output: 'none' while a chosen device works, 'choose' (the browser's own output
  // chooser, selectAudioOutput: no microphone), or 'allow' (no chooser here: the explicit button that asks for the microphone only to
  // read the device names). Never the microphone without that button.
  function outputButton(s) {
    if (s.working) return 'none';
    return s.hasSelectAudioOutput ? 'choose' : 'allow';
  }
  const OUTPUT_HELP = {
    choose: "Choose the stick's output (jank) once.",
    allow: "Chrome will ask for the microphone only to read the device names; nothing is recorded.",
    none: '',
  };
  const OUTPUT_BUTTON_TEXT = { choose: "Choose the stick's output", allow: 'Allow device names (asks for the microphone)', none: '' };

  // the stick's output: jank first, else the remembered device when it is still there, else none (id null: monitor only)
  function resolveOutput(devices, savedId) {
    const choices = outputChoices(devices), jank = pickOutput(devices, 'jank');
    const byId = id => choices.find(c => c.id === id);
    if (jank !== null) return { id: jank, label: byId(jank).label, why: 'jank' };
    if (savedId && byId(savedId)) return { id: savedId, label: byId(savedId).label, why: 'saved' };
    return { id: null, label: 'none', why: 'default' };
  }

  const extensionOf = name => { const i = String(name).lastIndexOf('.'); return i < 0 ? '' : String(name).slice(i + 1).toLowerCase(); };
  const isAudioFile = name => AUDIO_EXT.includes(extensionOf(name));
  const byName = (a, b) => collator.compare(a.name, b.name);
  // entries { name, kind: 'directory' | 'file' }: folders first, then the audio files, each in natural number order; dotfiles dropped
  function sortEntries(entries) {
    const shown = (entries || []).filter(e => !e.name.startsWith('.'));
    return [...shown.filter(e => e.kind === 'directory').sort(byName), ...shown.filter(e => e.kind === 'file' && isAudioFile(e.name)).sort(byName)];
  }
  // the breadcrumb of a path (folder names from the root): [{ label, depth }], depth = how many names to keep when clicked
  const breadcrumbs = parts => [{ label: '/', depth: 0 }, ...parts.map((label, i) => ({ label, depth: i + 1 }))];
  const rowsToShow = (count, shownLimit) => Math.min(count, shownLimit || MAX_ROWS);

  // a tree from <input webkitdirectory> items [{ path: 'Music/a/b.mp3', file }]: the first name is the chosen folder
  function buildTree(items) {
    const root = { name: '', dirs: {}, files: [] };
    for (const item of items) {
      const parts = String(item.path).split('/').filter(Boolean);
      if (!root.name) root.name = parts[0] || '';
      let node = root;
      for (const dir of parts.slice(1, -1)) { node.dirs[dir] = node.dirs[dir] || { name: dir, dirs: {}, files: [] }; node = node.dirs[dir]; }
      if (parts.length > 1) node.files.push({ name: parts[parts.length - 1], file: item.file });
    }
    return root;
  }
  // the raw entries of the folder at `parts` (names below the root), or [] when there is none
  function listTree(root, parts) {
    let node = root;
    for (const p of parts) { node = node.dirs[p]; if (!node) return []; }
    return [...Object.values(node.dirs).map(d => ({ name: d.name, kind: 'directory' })), ...node.files.map(f => ({ name: f.name, kind: 'file', file: f.file }))];
  }
  // the next / previous index in a list of `count` tracks, or -1 past either end
  // What follows a track, by index in the shown folder's list of `count` audio files (-1: nothing). `rng` returns [0, 1).
  const nextTrack = (count, index) => (index + 1 < count ? index + 1 : -1);
  function randomTrack(count, index, rng) {
    const others = Array.from({ length: count }, (_, i) => i).filter(i => i !== index);
    return others.length ? others[Math.min(others.length - 1, Math.floor((rng || Math.random)() * others.length))] : -1;
  }
  const AFTER_MODES = ['next', 'random', 'stop'];
  const parseAfterMode = text => (AFTER_MODES.includes(text) ? text : 'next');
  // a track that ran off its end: "then" next / random / stop
  const afterTrack = (mode, count, index, rng) => (mode === 'next' ? nextTrack(count, index) : mode === 'random' ? randomTrack(count, index, rng) : -1);
  // the Next button: random picks any other file, else the next one, back to the first after the last
  function skipTrack(mode, count, index, rng) {
    if (mode === 'random') return randomTrack(count, index, rng);
    const n = nextTrack(count, index);
    return n >= 0 ? n : (count > 0 && index !== 0 ? 0 : -1);
  }


  // ---- the tempo tag of the playing file (never a folder scan): ID3v2 TBPM, MP4 tmpo, FLAC / Ogg Vorbis / Opus BPM= ----
  // Every parser takes the bytes it was given (Uint8Array) and returns a number or null; a short, cut or malformed input is null.
  const MAX_TAG_BYTES = 2 * 1024 * 1024, BPM_MIN = 20, BPM_MAX = 999;
  const latin1 = new TextDecoder('latin1'), utf8 = new TextDecoder('utf-8'), utf16le = new TextDecoder('utf-16le'), utf16be = new TextDecoder('utf-16be');
  const ascii = (b, at, n) => (at >= 0 && at + n <= b.length ? String.fromCharCode(...b.subarray(at, at + n)) : '');
  const u32be = (b, at) => (at >= 0 && at + 4 <= b.length ? ((b[at] << 24) | (b[at + 1] << 16) | (b[at + 2] << 8) | b[at + 3]) >>> 0 : -1);
  const u32le = (b, at) => (at >= 0 && at + 4 <= b.length ? ((b[at + 3] << 24) | (b[at + 2] << 16) | (b[at + 1] << 8) | b[at]) >>> 0 : -1);
  const syncsafe = (b, at) => {
    if (at < 0 || at + 4 > b.length || (b[at] | b[at + 1] | b[at + 2] | b[at + 3]) & 0x80) return -1;
    return (b[at] << 21) | (b[at + 1] << 14) | (b[at + 2] << 7) | b[at + 3];
  };

  // "124", "124.00", "124,5" (and a text with NULs or spaces around) -> 124 ...; anything else, or outside 20..999, is null
  function parseBpmText(text) {
    const m = /^\s*(\d{1,4}(?:[.,]\d{1,6})?)\s*$/.exec(String(text).replace(/\u0000/g, ''));
    const v = m ? Number(m[1].replace(',', '.')) : NaN;
    return v >= BPM_MIN && v <= BPM_MAX ? v : null;
  }
  // the text of an ID3 text frame: encoding byte (0 Latin-1, 1 UTF-16 with BOM, 2 UTF-16BE, 3 UTF-8), then the text; null for any other encoding
  function id3Text(data) {
    if (data.length < 1) return null;
    const body = data.subarray(1);
    switch (data[0]) {
      case 0: return latin1.decode(body);
      case 1: return body.length >= 2 && body[0] === 0xfe && body[1] === 0xff ? utf16be.decode(body.subarray(2)) : utf16le.decode(body.length >= 2 && body[0] === 0xff && body[1] === 0xfe ? body.subarray(2) : body);
      case 2: return utf16be.decode(body);
      case 3: return utf8.decode(body);
      default: return null;
    }
  }
  const ununsync = bytes => { const out = []; for (let i = 0; i < bytes.length; i++) { out.push(bytes[i]); if (bytes[i] === 0xff && bytes[i + 1] === 0) i++; } return Uint8Array.from(out); };

  // ID3v2 total size (header + tag, without a footer) from the first 10 bytes, or 0 when it is not an ID3v2 tag: how much to read
  function id3TotalSize(head) {
    if (ascii(head, 0, 3) !== 'ID3' || head.length < 10 || ![2, 3, 4].includes(head[3])) return 0;
    const size = syncsafe(head, 6);
    return size < 0 ? 0 : 10 + size;
  }
  // a frame's header: { id, size, headerLen, unsync, skip } at `at` in the (ununsynchronised) tag body of version `ver`, or null at padding / the end
  function id3Frame(body, at, ver) {
    if (ver === 2) {
      const id = ascii(body, at, 3), s = body.length >= at + 6 ? (body[at + 3] << 16) | (body[at + 4] << 8) | body[at + 5] : -1;
      return /^[A-Z0-9]{3}$/.test(id) && s >= 0 ? { id, size: s, headerLen: 6, unsync: false, skip: false, indicator: false } : null;
    }
    const id = ascii(body, at, 4), s = ver === 4 ? syncsafe(body, at + 4) : u32be(body, at + 4);
    if (!/^[A-Z0-9]{4}$/.test(id) || s < 0 || body.length < at + 10) return null;
    const f2 = body[at + 9];
    return ver === 4
      ? { id, size: s, headerLen: 10, unsync: (f2 & 0x02) !== 0, skip: (body[at + 9] & 0x0c) !== 0, indicator: (f2 & 0x01) !== 0 }
      : { id, size: s, headerLen: 10, unsync: false, skip: (body[at + 9] & 0xc0) !== 0, indicator: false };
  }
  function parseId3(bytes) {
    const total = id3TotalSize(bytes);
    if (total === 0) return null;
    const ver = bytes[3], flags = bytes[5];
    let body = bytes.subarray(10, Math.min(total, bytes.length));
    if (flags & 0x80) body = ununsync(body);
    let at = 0;
    if (flags & 0x40 && ver >= 3) {                                    // an extended header: skip it
      const ext = ver === 4 ? syncsafe(body, 0) : u32be(body, 0) + 4;
      if (ext < 4 || ext > body.length) return null;
      at = ext;
    }
    const want = ver === 2 ? 'TBP' : 'TBPM';
    for (let guard = 0; guard < 4096; guard++) {
      const f = id3Frame(body, at, ver);
      if (!f || at + f.headerLen + f.size > body.length) return null;   // padding, the end, or a size that runs past the tag
      let data = body.subarray(at + f.headerLen, at + f.headerLen + f.size);
      at += f.headerLen + f.size;
      if (f.id !== want || f.skip) continue;
      if (f.indicator) data = data.subarray(4);
      if (f.unsync) data = ununsync(data);
      const text = id3Text(data);
      return text === null ? null : parseBpmText(text);
    }
    return null;
  }

  // MP4 / M4A: an atom's header at the start of `b`: { size, type, headerLen } (size 0 = to the end of the file: reported as 0), or null
  function mp4Atom(b) {
    if (b.length < 8) return null;
    const size = u32be(b, 0), type = ascii(b, 4, 4);
    if (size === 1) return b.length >= 16 && u32be(b, 8) === 0 ? { size: u32be(b, 12), type, headerLen: 16 } : null;
    return { size, type, headerLen: 8 };
  }
  const MP4_BOXES = ['moov', 'udta', 'meta', 'ilst'];
  // the integer BPM of moov/udta/meta/ilst/tmpo/data in a buffer of top-level atoms (the moov atom alone will do)
  function parseMp4Tmpo(bytes) {
    function walk(b, start, end, depth) {
      let at = start;
      for (let guard = 0; guard < 256 && at + 8 <= end; guard++) {
        const a = mp4Atom(b.subarray(at, end));
        if (!a || a.headerLen > a.size && a.size !== 0) return null;
        const size = a.size === 0 ? end - at : a.size;
        if (size < a.headerLen || at + size > end) return null;
        const inner = at + a.headerLen;
        if (a.type === 'tmpo') return tmpoValue(b, inner, at + size);
        if (MP4_BOXES.includes(a.type) && depth < 6) {
          const found = walk(b, a.type === 'meta' ? inner + 4 : inner, at + size, depth + 1);
          if (found !== null) return found;
        }
        at += size;
      }
      return null;
    }
    // tmpo holds a data atom: size, 'data', 4 bytes type / flags, 4 bytes locale, then the big-endian integer
    function tmpoValue(b, start, end) {
      const a = mp4Atom(b.subarray(start, end));
      if (!a || a.type !== 'data' || start + a.size > end || a.size < 8 + 8 + 1) return null;
      let v = 0;
      for (let i = start + 16; i < start + a.size && i < start + 20; i++) v = v * 256 + b[i];
      return v >= BPM_MIN && v <= BPM_MAX ? v : null;
    }
    return walk(bytes, 0, bytes.length, 0);
  }

  // Vorbis comments (FLAC block, Vorbis packet, OpusTags): vendor string, count, then KEY=value strings, all little-endian lengths
  function commentBpm(b, at) {
    const vendor = u32le(b, at);
    if (vendor < 0 || at + 4 + vendor > b.length) return null;
    let p = at + 4 + vendor;
    const count = u32le(b, p);
    if (count < 0) return null;
    p += 4;
    for (let i = 0; i < Math.min(count, 4096); i++) {
      const len = u32le(b, p);
      if (len < 0 || p + 4 + len > b.length) return null;
      const m = /^bpm=(.*)$/is.exec(utf8.decode(b.subarray(p + 4, p + 4 + len)));
      if (m) return parseBpmText(m[1]);
      p += 4 + len;
    }
    return null;
  }
  function parseFlac(b) {
    if (ascii(b, 0, 4) !== 'fLaC') return null;
    let at = 4;
    for (let guard = 0; guard < 256 && at + 4 <= b.length; guard++) {
      const last = (b[at] & 0x80) !== 0, type = b[at] & 0x7f, len = (b[at + 1] << 16) | (b[at + 2] << 8) | b[at + 3];
      if (at + 4 + len > b.length) return null;
      if (type === 4) return commentBpm(b.subarray(at + 4, at + 4 + len), 0);
      if (last) return null;
      at += 4 + len;
    }
    return null;
  }
  // the first `want` packets of an Ogg stream (pages: 27 byte header, segment table, payload; a segment under 255 ends a packet)
  function oggPackets(b, want) {
    const packets = [];
    let at = 0, current = [];
    for (let page = 0; page < 64 && packets.length < want; page++) {
      if (ascii(b, at, 4) !== 'OggS' || at + 27 > b.length) return packets;
      const nseg = b[at + 26];
      if (at + 27 + nseg > b.length) return packets;
      let pos = at + 27 + nseg;
      for (let s = 0; s < nseg; s++) {
        const len = b[at + 27 + s];
        if (pos + len > b.length) return packets;
        current.push(b.subarray(pos, pos + len));
        pos += len;
        if (len < 255) { packets.push(Uint8Array.from(current.flatMap(c => [...c]))); current = []; }
      }
      at = pos;
    }
    return packets;
  }
  function parseOgg(b) {
    const packets = oggPackets(b, 2);
    const c = packets[1];
    if (!c) return null;
    if (ascii(c, 0, 7) === '\u0003vorbis') return commentBpm(c, 7);
    if (ascii(c, 0, 8) === 'OpusTags') return commentBpm(c, 8);
    return null;
  }

  // what the first 16 bytes say the file is: 'id3' | 'flac' | 'ogg' | 'mp4' | null
  function sniff(head) {
    if (ascii(head, 0, 3) === 'ID3') return 'id3';
    if (ascii(head, 0, 4) === 'fLaC') return 'flac';
    if (ascii(head, 0, 4) === 'OggS') return 'ogg';
    if (ascii(head, 4, 4) === 'ftyp') return 'mp4';
    return null;
  }
  // the tempo tag of a buffer that starts a file (an MP4's tags are in its moov atom: see mp4Atom for finding it deeper in the file)
  function tagBpm(bytes) {
    switch (sniff(bytes)) {
      case 'id3': return parseId3(bytes);
      case 'flac': return parseFlac(bytes);
      case 'ogg': return parseOgg(bytes);
      case 'mp4': return parseMp4Tmpo(bytes);
      default: return null;
    }
  }
  // the BPM the stick should hear for a tag BPM at a playback rate: the plain product (the stick folds half / double, this does not)
  const expectedBpm = (tag, rate) => tag * rate;

  return { MAX_TAG_BYTES, parseBpmText, id3TotalSize, parseId3, mp4Atom, parseMp4Tmpo, parseFlac, parseOgg, sniff, tagBpm, expectedBpm, PITCH_RANGE_PCT, DEFAULT_VOLUME_PCT, MAX_ROWS, AUDIO_EXT, LEVEL_FLOOR_DB, rmsOf, rmsToDb, smoothDb, meterFraction, statusText, pitchToRate, formatPitch, volumeFromPct, parseVolumePct, formatTime,
           savedStatus, sinkMatches, outputReport, SCOPE_CAP, isJunkName, pathKey, scanTree, scopeLine, matchSaved, outputButton, OUTPUT_HELP, OUTPUT_BUTTON_TEXT, playbackPlan, planNote, planDetails, browserBrand, nextTrack, randomTrack, afterTrack, skipTrack, AFTER_MODES, parseAfterMode, routeText, pickOutput, outputChoices, resolveOutput, isAudioFile, sortEntries, breadcrumbs, rowsToShow, buildTree, listTree };
})();
