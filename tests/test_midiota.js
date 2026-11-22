// SPDX-License-Identifier: MIT
// ui/midiota.js (the Install page's update over USB MIDI) against a simulated device that answers as core/jank_ota.c does:
// the app and the beatw target, a lost answer (resent), a damaged chunk (err crc: resent from the offset), the bytes that
// arrive equal the image, the SysEx framing round trip.
'use strict';
const assert = require('assert');
const path = require('path');
const MidiOta = require(path.join(__dirname, '..', 'ui', 'midiota.js'));
let n = 0;
const ok = (c, m) => { assert.ok(c, m); n++; };

// framing: a line out and back, split over frames as the device splits them (30 characters a frame)
{
  const f = MidiOta.frame('get');
  ok(f[0] === 0xf0 && f[1] === 0x7d && f[2] === 0x4a && f[3] === 0x4b && f[f.length - 1] === 0xf7, 'a frame is F0 7D J K ... F7');
  const lines = new MidiOta.Lines();
  const text = 'version=0.1.0\nboard=xiao\nok\n';
  const raw = (t) => [0xf0, 0x7d, 0x4a, 0x4b, ...[...t].map((ch) => ch.charCodeAt(0)), 0xf7];   // a frame as the device sends it
  for (let i = 0; i < text.length; i += 30) lines.feed(raw(text.slice(i, i + 30)));
  ok(lines.lines.join('|') === 'version=0.1.0|board=xiao|ok', 'lines come back across frames');
}

function device(target, { loseFirstAnswer = false, damageAt = -1 } = {}) {
  const lines = new MidiOta.Lines();
  const st = { got: [], begun: false, size: 0, target: null, ended: false, lost: loseFirstAnswer, damaged: false, sends: 0 };
  const answer = (t) => setTimeout(() => lines.feed(MidiOta.frame(t)), 1);
  const send = (text) => {
    st.sends++;
    const w = text.trim().split(' ');
    if (w[1] === 'begin') {
      st.begun = true; st.size = +w[2]; st.target = w[4] || 'app';
      if (st.lost) { st.lost = false; return; }         // the first answer lost: the sender resends
      answer('ok 0 120');
    } else if (w[1] === 'd') {
      const off = +w[2], data = Buffer.from(w[4], 'base64');
      const have = st.got.length;
      if (off < have) return answer('ok ' + have);
      if (off > have) return answer('err gap ' + have);
      if (off === damageAt && !st.damaged) { st.damaged = true; return answer('err crc ' + have); }
      if (MidiOta.crcHex(data) !== w[3]) return answer('err crc ' + have);
      for (const b of data) st.got.push(b);
      answer('ok ' + st.got.length);
    } else if (w[1] === 'end') {
      st.ended = st.got.length === st.size;
      answer(st.ended ? 'ok ' + st.size : 'err short ' + st.got.length);
    } else answer('ok');
  };
  return { send, lines, st };
}

(async () => {
  const app = new Uint8Array(1000).map((_, i) => (i * 13) & 255);
  app[0] = 0xe9; app[1] = 3; app[12] = 9; app[13] = 0;
  {
    const d = device('app', { loseFirstAnswer: true, damageAt: 240 });
    let last = 0;
    await MidiOta.upload(d.send, d.lines, app, 'app', (done) => { last = done; });
    ok(d.st.target === 'app' && d.st.ended, 'the app transfer ends');
    ok(Buffer.compare(Buffer.from(d.st.got), Buffer.from(app)) === 0, 'the device got the image byte for byte (a lost answer, a damaged chunk)');
    ok(last === app.length, 'progress reaches the end');
  }
  {
    const blob = new Uint8Array(300).map((_, i) => i & 255);
    blob.set([0x4a, 0x42, 0x57, 0x31]);
    const d = device('beatw');
    await MidiOta.upload(d.send, d.lines, blob, 'beatw');
    ok(d.st.target === 'beatw' && d.st.ended && Buffer.compare(Buffer.from(d.st.got), Buffer.from(blob)) === 0, 'the weights go with the beatw target');
    await assert.rejects(MidiOta.upload(d.send, d.lines, blob, 'app'), /not an ESP32-S3 app image/); n++;
    await assert.rejects(MidiOta.upload(d.send, d.lines, app, 'beatw'), /not a weights blob/); n++;
  }
  {
    const lines = new MidiOta.Lines();
    const send = () => setTimeout(() => lines.feed(MidiOta.frame('err state 0')), 1);
    const blob = new Uint8Array(300); blob.set([0x4a, 0x42, 0x57, 0x31]);
    await assert.rejects(MidiOta.upload(send, lines, blob, 'beatw'), /no room for the weights/); n++;
  }
  console.log(`midiota: ${n} checks passed`);
})().catch((e) => { console.error(e); process.exit(1); });
