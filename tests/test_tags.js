// SPDX-License-Identifier: MIT
// The player's tempo-tag parsers (ui/player.js) on synthetic bytes: ID3v2.2 / 2.3 / 2.4 with every text encoding, unsynchronisation,
// extended header, MP4 tmpo, FLAC and Ogg Vorbis / Opus comments, and malformed input (truncated, wrong or huge sizes, a bad encoding,
// garbage, random bytes). No music files. Exit status: non-zero on a failure.
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const P = new Function(fs.readFileSync(path.join(__dirname, '..', 'ui', 'player.js'), 'utf8') + '; return Player;')();
let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const bytes = (...parts) => Uint8Array.from(parts.flatMap(p => (typeof p === 'number' ? [p] : typeof p === 'string' ? [...Buffer.from(p, 'latin1')] : [...p])));
const be32 = n => [(n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255];
const le32 = n => [n & 255, (n >>> 8) & 255, (n >>> 16) & 255, (n >>> 24) & 255];
const ss32 = n => [(n >>> 21) & 127, (n >>> 14) & 127, (n >>> 7) & 127, n & 127];

// ---- the text ----
eq([P.parseBpmText('124'), P.parseBpmText('124.00'), P.parseBpmText(' 98,5 '), P.parseBpmText('124.00\u0000')], [124, 124, 98.5, 124], 'plain numbers, a comma, spaces, a trailing NUL');
eq(['', 'abc', '12x', '1e3', '-120', '19', '1000', '124 BPM', '12.', '.5'].map(P.parseBpmText), Array(10).fill(null), 'garbage and values outside 20..999 are null');

// ---- ID3v2 ----
const enc = (e, s) => {
  if (e === 0) return bytes(0, Buffer.from(s, 'latin1'));
  if (e === 1) return bytes(1, 0xff, 0xfe, Buffer.from(s, 'utf16le'));
  if (e === 2) return bytes(2, Buffer.from(s, 'utf16le').swap16());
  return bytes(3, Buffer.from(s, 'utf8'));
};
const frame = (ver, id, data, flags = 0) => (ver === 2 ? bytes(id, data.length >> 16, (data.length >> 8) & 255, data.length & 255, data)
  : bytes(id, ver === 4 ? ss32(data.length) : be32(data.length), flags >> 8, flags & 255, data));
const id3 = (ver, frames, { flags = 0, ext = [], pad = 0 } = {}) => {
  const body = bytes(ext, ...frames, new Uint8Array(pad));
  return bytes('ID3', ver, 0, flags, ss32(body.length), body);
};
for (const e of [0, 1, 2, 3]) {
  eq(P.parseId3(id3(3, [frame(3, 'TBPM', enc(e, '124.00'))])), 124, 'v2.3 TBPM, encoding ' + e);
  eq(P.parseId3(id3(4, [frame(4, 'TBPM', enc(e, '128'))])), 128, 'v2.4 TBPM, encoding ' + e);
  eq(P.parseId3(id3(2, [frame(2, 'TBP', enc(e, '90.5'))])), 90.5, 'v2.2 TBP, encoding ' + e);
}
eq(P.parseId3(id3(3, [frame(3, 'TIT2', enc(0, 'a song')), frame(3, 'TBPM', enc(0, '120')), frame(3, 'TALB', enc(0, 'x'))], { pad: 20 })), 120, 'BPM after other frames, with padding');
eq(P.parseId3(id3(3, [frame(3, 'TIT2', enc(0, 'a song'))], { pad: 20 })), null, 'no BPM frame: null');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', enc(0, '1' + '4'.repeat(200)))])), null, 'a number far outside the range: null');
// a frame size of 0x80 or more: plain in 2.3, syncsafe in 2.4 (a long text with a long number of spaces)
const long = ' '.repeat(300) + '125';
eq(P.parseId3(id3(3, [frame(3, 'TBPM', enc(0, long))])), 125, 'v2.3: a plain frame size over 255');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', enc(0, long))])), 125, 'v2.4: a syncsafe frame size over 127');
// extended header: v2.3 plain size (excluding itself), v2.4 syncsafe (including itself)
eq(P.parseId3(id3(3, [frame(3, 'TBPM', enc(0, '110'))], { flags: 0x40, ext: [...be32(6), 0, 0, 0, 0, 0, 0] })), 110, 'v2.3 extended header skipped');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', enc(0, '111'))], { flags: 0x40, ext: [...ss32(6), 1, 0] })), 111, 'v2.4 extended header skipped');
// unsynchronisation: a 0xFF followed by 0x00 in the tag; the 0x00 is dropped. A text of 'ÿ' padding before the number gives one.
const unsyncBody = (ver, ...frames) => {
  const raw = bytes(...frames), out = [];
  for (let i = 0; i < raw.length; i++) { out.push(raw[i]); if (raw[i] === 0xff) out.push(0); }
  return bytes('ID3', ver, 0, 0x80, ss32(out.length), out);
};
eq(P.parseId3(unsyncBody(3, frame(3, 'TXXX', bytes(0, 0xff, 0xff)), frame(3, 'TBPM', enc(0, '133')))), 133, 'a tag-level unsynchronised v2.3 tag');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', bytes(3, 0x31, 0x32, 0x30), 0x0002)])), 120, 'v2.4 frame-level unsync flag on clean data');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', bytes(0, 0, 0, 0, 3, '121'), 0x0001)])), 121, 'v2.4 data length indicator');
eq(P.parseId3(id3(4, [frame(4, 'TBPM', enc(0, '120'), 0x000c), frame(4, 'TBPM', enc(0, '121'))])), 121, 'a compressed / encrypted frame is skipped');
eq(P.id3TotalSize(id3(3, [frame(3, 'TBPM', enc(0, '120'))])), 10 + 10 + 4, 'the tag size to read');
eq(P.id3TotalSize(bytes('ID3', 3, 0, 0, 0x80, 0, 0, 0)), 0, 'a size with a high bit is not syncsafe');
eq(P.id3TotalSize(bytes('RIFF')), 0, 'not ID3');

// ---- ID3 malformed ----
const good = id3(3, [frame(3, 'TBPM', enc(0, '124'))]);
for (let n = 0; n < good.length; n++) eq(P.parseId3(good.subarray(0, n)) === 124 || P.parseId3(good.subarray(0, n)) === null, true, 'truncated at ' + n + ' does not throw');
eq(P.parseId3(good.subarray(0, good.length - 1)), null, 'a frame cut short: null');
eq(P.parseId3(bytes('ID3', 3, 0, 0, ss32(100), 'TBPM', be32(0x7fffffff), 0, 0, 0, '1')), null, 'a huge frame size: null');
eq(P.parseId3(bytes('ID3', 3, 0, 0, ss32(0x0fffffff), 'TBPM', be32(2), 0, 0, 0, '1')), null, 'a huge tag size with a short buffer: null, no hang');
eq(P.parseId3(id3(3, [frame(3, 'TBPM', bytes(9, '120'))])), null, 'an unknown text encoding: null');
eq(P.parseId3(id3(3, [frame(3, 'TBPM', bytes())])), null, 'an empty frame: null');
eq(P.parseId3(id3(5, [frame(3, 'TBPM', enc(0, '120'))])), null, 'an unknown version: null');
eq(P.parseId3(id3(3, [frame(3, 'TBPM', enc(1, 'abc'))])), null, 'text that is not a number: null');
eq(P.parseId3(bytes('ID3', 3, 0, 0x40, ss32(8), be32(0x7fffffff), 0, 0, 0, 0)), null, 'a huge extended header size: null');
eq(P.parseId3(new Uint8Array(0)), null, 'empty input: null');

// ---- MP4 / M4A ----
const atom = (type, ...inner) => { const body = bytes(...inner); return bytes(be32(8 + body.length), type, body); };
const tmpoAtom = (value, width = 2) => atom('tmpo', atom('data', 0, 0, 0, 21, 0, 0, 0, 0, width === 2 ? [value >> 8, value & 255] : [value]));
const m4a = (ilstItems, { metaPad = true } = {}) => bytes(atom('ftyp', 'M4A ', 0, 0, 0, 0, 'M4A '), atom('moov', atom('mvhd', new Uint8Array(100)), atom('udta', atom('meta', metaPad ? [0, 0, 0, 0] : [], atom('hdlr', new Uint8Array(20)), atom('ilst', ...ilstItems)))), atom('mdat', new Uint8Array(50)));
eq(P.tagBpm(m4a([atom('©nam', atom('data', 0, 0, 0, 1, 0, 0, 0, 0, 'x')), tmpoAtom(124)])), 124, 'M4A: tmpo as a 16 bit integer, after another item');
eq(P.tagBpm(m4a([tmpoAtom(98, 1)])), 98, 'M4A: tmpo as an 8 bit integer');
eq(P.tagBpm(m4a([atom('©nam', atom('data', 0, 0, 0, 1, 0, 0, 0, 0, 'x'))])), null, 'M4A without tmpo: null');
eq(P.tagBpm(m4a([tmpoAtom(0)])), null, 'M4A tmpo 0 (no tempo): null');
eq(P.tagBpm(m4a([tmpoAtom(5000)])), null, 'M4A tmpo out of range: null');
const moovOnly = atom('moov', atom('udta', atom('meta', [0, 0, 0, 0], atom('ilst', tmpoAtom(131)))));
eq(P.parseMp4Tmpo(moovOnly), 131, 'a moov atom alone (as read from deep in a file) parses');
eq(P.mp4Atom(bytes(be32(1234), 'moov')), { size: 1234, type: 'moov', headerLen: 8 }, 'an atom header');
eq(P.mp4Atom(bytes(be32(1), 'mdat', be32(0), be32(99))), { size: 99, type: 'mdat', headerLen: 16 }, 'an atom with a 64 bit size');
eq(P.mp4Atom(bytes(be32(1), 'mdat', be32(7), be32(99))), null, 'a 64 bit size over 4 GiB: not followed');
eq(P.mp4Atom(bytes(0, 0, 0)), null, 'a short header: null');
const m4 = m4a([tmpoAtom(124)]);
for (let n = 0; n < m4.length; n++) { const r = P.tagBpm(m4.subarray(0, n)); eq(r === null || r === 124, true, 'M4A truncated at ' + n + ' does not throw'); }
eq(P.tagBpm(bytes(atom('ftyp', 'M4A ', 0, 0, 0, 0), be32(0x7fffffff), 'moov', new Uint8Array(20))), null, 'M4A: a huge atom size: null');
eq(P.parseMp4Tmpo(bytes(be32(4), 'moov')), null, 'an atom smaller than its header: null');

// ---- FLAC ----
const comments = (vendor, list) => bytes(le32(vendor.length), vendor, le32(list.length), ...list.map(s => bytes(le32(Buffer.byteLength(s)), Buffer.from(s, 'utf8'))));
const flac = (list, { last = true } = {}) => { const c = comments('ref libFLAC', list); return bytes('fLaC', 0, 0, 0, 34, new Uint8Array(34), (last ? 0x84 : 4), (c.length >> 16) & 255, (c.length >> 8) & 255, c.length & 255, c); };
eq(P.tagBpm(flac(['TITLE=x', 'BPM=124.00'])), 124, 'FLAC: BPM=124.00');
eq(P.tagBpm(flac(['bpm=98'])), 98, 'FLAC: a lower case key');
eq(P.tagBpm(flac(['Bpm=100.5'])), 100.5, 'FLAC: a mixed case key');
eq(P.tagBpm(flac(['TITLE=x'])), null, 'FLAC without BPM: null');
eq(P.tagBpm(flac(['BPM=fast'])), null, 'FLAC: a BPM that is not a number: null');
eq(P.tagBpm(flac(['BPM=é'])), null, 'FLAC: UTF-8 text that is not a number: null');
const fl = flac(['BPM=124']);
for (let n = 0; n < fl.length; n++) { const r = P.tagBpm(fl.subarray(0, n)); eq(r === null || r === 124, true, 'FLAC truncated at ' + n + ' does not throw'); }
eq(P.tagBpm(bytes('fLaC', 0x84, 0xff, 0xff, 0xff, 1, 2, 3)), null, 'FLAC: a block longer than the buffer: null');
eq(P.tagBpm(bytes('fLaC', 4, 0, 0, 12, le32(0x7fffffff), le32(1), 0, 0, 0, 0)), null, 'FLAC: a huge vendor length: null');
eq(P.tagBpm(bytes('fLaC', 0x84, 0, 0, 16, le32(0), le32(0x7fffffff), le32(5), 'BPM=1')), null, 'FLAC: a huge comment count with too few comments: null, no hang');

// ---- Ogg Vorbis and Opus ----
const page = (seq, packetsBytes, { cont = false, serial = 1 } = {}) => {
  const segs = [];
  for (const p of packetsBytes) { let n = p.length; while (n >= 255) { segs.push(255); n -= 255; } segs.push(n); }
  return bytes('OggS', 0, cont ? 1 : 0, new Uint8Array(8), le32(serial), le32(seq), le32(0), segs.length, segs, ...packetsBytes);
};
const vorbisId = bytes(1, 'vorbis', new Uint8Array(23));
const vorbisComment = list => bytes(3, 'vorbis', comments('libvorbis', list), 1);
eq(P.tagBpm(bytes(page(0, [vorbisId]), page(1, [vorbisComment(['BPM=127.5'])]))), 127.5, 'Ogg Vorbis: the comment packet on its own page');
eq(P.tagBpm(bytes(page(0, [vorbisId]), page(1, [vorbisComment(['TITLE=x', 'bpm=100'])]))), 100, 'Ogg Vorbis: a lower case key');
eq(P.tagBpm(bytes(page(0, [vorbisId]), page(1, [vorbisComment(['TITLE=x'])]))), null, 'Ogg Vorbis without BPM: null');
// a comment packet over 255 bytes spans several lacing values; and one split over two pages
const padded = vorbisComment(['COMMENT=' + 'x'.repeat(600), 'BPM=122']);
eq(P.tagBpm(bytes(page(0, [vorbisId]), page(1, [padded]))), 122, 'Ogg Vorbis: a long comment packet (lacing 255, 255, ...)');
const cut = 255;
const firstHalf = bytes('OggS', 0, 0, new Uint8Array(8), le32(1), le32(1), le32(0), 1, 255, padded.subarray(0, cut));
const secondHalf = page(2, [padded.subarray(cut)], { cont: true });
eq(P.tagBpm(bytes(page(0, [vorbisId]), firstHalf, secondHalf)), 122, 'Ogg Vorbis: a comment packet continued on the next page');
const opusTags = list => bytes('OpusTags', comments('libopus', list));
eq(P.tagBpm(bytes(page(0, [bytes('OpusHead', new Uint8Array(11))]), page(1, [opusTags(['BPM=140'])]))), 140, 'Opus: OpusTags BPM');
const og = bytes(page(0, [vorbisId]), page(1, [vorbisComment(['BPM=127'])]));
for (let n = 0; n < og.length; n++) { const r = P.tagBpm(og.subarray(0, n)); eq(r === null || r === 127, true, 'Ogg truncated at ' + n + ' does not throw'); }
eq(P.tagBpm(bytes('OggS', 0, 0, new Uint8Array(8), le32(1), le32(0), le32(0), 255, new Uint8Array(255).fill(255))), null, 'Ogg: segment table longer than the data: null');

// ---- anything else, and random bytes behind every magic ----
eq([P.tagBpm(bytes('RIFF', new Uint8Array(30))), P.tagBpm(new Uint8Array(0)), P.tagBpm(bytes('ID3'))], [null, null, null], 'not a tagged file / too short');
let seed = 12345;
const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed & 255; };
const magics = [bytes('ID3', 3, 0, 0), bytes('ID3', 4, 0, 0x40), bytes('ID3', 2, 0, 0x80), bytes('fLaC'), bytes('OggS'), bytes(0, 0, 0, 20, 'ftyp')];
for (let i = 0; i < 600; i++) {
  const m = magics[i % magics.length], junk = Uint8Array.from({ length: 40 + (i % 80) }, rnd), data = bytes(m, junk);
  const r = P.tagBpm(data);
  eq(r === null || (r >= 20 && r <= 999), true, 'random bytes after magic ' + (i % magics.length) + ' (' + i + ') give null or a sane BPM');
}

// ---- the expected BPM: the plain product, no folding ----
eq([P.expectedBpm(124, 1), P.expectedBpm(124, 1.08), P.expectedBpm(60, 0.92)].map(v => Math.round(v * 100) / 100), [124, 133.92, 55.2], 'tag BPM x rate, never folded');

console.log('tags: ' + checks + ' checks passed');
