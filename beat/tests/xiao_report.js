// node xiao_report.js PLAYER_HTML: the page's XIAO helpers (reports, control-line frames, `beat` numbers) against the
// firmware's formats (jank core/jank_sysex.h)
const fs = require('fs');
const assert = require('assert');
const html = require('../../tests/load_html.js')(process.argv[2]);
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
const src = ['const JX_TEXT_MAX = 30;', grab('xiaoReport'), grab('xiaoBeatEpochMs'), grab('jxText'), grab('jxFrames'), grab('beatStats')].join('\n');
const { xiaoReport, xiaoBeatEpochMs, jxText, jxFrames, beatStats } =
  new Function(src + '; return { xiaoReport, xiaoBeatEpochMs, jxText, jxFrames, beatStats };')();
const frame = (text, type = 0x10, tag = 0x42) => Uint8Array.from([0xf0, 0x7d, 0x4a, tag, type, ...Buffer.from(text, 'ascii'), 0xf7]);
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };

eq(xiaoReport(frame('b 17 12403 steady 1')), { kind: 'b', n: 17, bpm: 124.03, mode: 'steady', audio: true, live: false });   // 1: open, no sound
eq(xiaoReport(frame('b 17 12403 steady 2 31250')), { kind: 'b', n: 17, bpm: 124.03, mode: 'steady', audio: true, live: true, lagUs: 31250 });   // 2: sound
eq(xiaoReport(frame('b 17 12403 steady 0')), { kind: 'b', n: 17, bpm: 124.03, mode: 'steady', audio: false, live: false });   // 0: not open
eq(xiaoReport(frame('b 2 12403 steady 1 -4000')).lagUs, -4000);
eq(xiaoReport(frame('b 3 12403 steady 1 31250 -4300')).linkUs, -4300);       // the Link session's beat from it
eq(xiaoReport(frame('b 3 12403 steady 1 31250 -')).linkUs, undefined);        // no session playing
eq(xiaoReport(frame('t 0 0 stopped 0 25 -')).latencyMs, 25);
eq(xiaoReport(frame('t 0 0 stopped 0 25')), { kind: 't', n: 0, bpm: 0, mode: 'stopped', audio: false, live: false, latencyMs: 25 });
eq(xiaoReport(frame('get', 0x10, 0x4b)), null);            // a control line is not a report
eq(xiaoReport(frame('b 1 2 steady 1', 0x11)), null);       // another report type
eq(xiaoReport(frame('x 1 2 steady 1')), null);
eq(xiaoReport(Uint8Array.from([0xf8])), null);             // the clock
eq(xiaoReport(frame('b 1 2').subarray(0, 8)), null);       // cut short: no F7
eq(xiaoBeatEpochMs(1.7e12, 5000.5, 31250), 1.7e12 + 5000.5 - 31.25);
eq(xiaoBeatEpochMs(1.7e12, 5000, -4000), 1.7e12 + 5004);

const fr = jxFrames('set beat_latency -15');              // 21 characters with the newline: one frame
eq(fr.length, 1);
eq(jxText(Uint8Array.from(fr[0])), 'set beat_latency -15\n');
const long = jxFrames('x'.repeat(45));                     // 46 with the newline: 30 + 16
eq(long.map(f => f.length - 5), [30, 16]);
ok(long.every(f => f[0] === 0xf0 && f[3] === 0x4b && f[f.length - 1] === 0xf7 && f.slice(1, -1).every(b => b < 0x80)), '7-bit frames');
eq(jxText(frame('b 1 2 steady 1')), null);                // a report is not a control line

eq(beatStats(['beat=on', 'cpu_pct=39.6', 'hop_max_us=14236', 'err_us=-4300', 'mode=steady', 'phase_forces=30']),
   { cpu_pct: 39.6, hop_max_us: 14236, err_us: -4300, phase_forces: 30 });
// --xiao: the XIAO's session from its `status` (xiaoLink), and what its tracker hears (xiaoHearing)
const consts = 'const XIAO_STATUS_FRESH_MS = 5000, XIAO_PLAY_WAIT_MS = 2500;';
const { xiaoLink, xiaoHearing } = new Function(consts + grab('xiaoLink') + grab('xiaoHearing') + '; return { xiaoLink, xiaoHearing };')();
const st = (o = {}) => Object.assign({ link_peers: 2, bpm: 120, beat: 10.0, playing: 0, settled: 1, armed: 0 }, o);
eq(xiaoLink(null, 0, 1000, 4, null), null);                                   // no status yet: nothing known (no 120)
eq(xiaoLink(st(), 1000, 6001, 4, null), null);                                // older than 5 s: nothing
eq(xiaoLink(st({ bpm: 0 }), 1000, 1500, 4, null),
   { joined: false, bpm: null, beat: null, box: null, playing: false, armed: false, running: false });   // not in a session
let x = xiaoLink(st(), 1000, 1500, 4, null);                                  // stopped, 120 BPM: half a second = a beat
eq([x.joined, x.bpm, x.beat, x.box, x.playing, x.armed, x.running], [true, 120, 11, 3, false, false, false]);
eq(xiaoLink(st({ beat: 12.0 }), 1000, 1000, 4, null).box, 0);                  // beat 12 = beat 1 of the bar (box 0)
eq(xiaoLink(st({ beat: 12.0 }), 1000, 1000, 3, null).box, 0);                  // quantum 3: 12 mod 3
eq(xiaoLink(st({ beat: 13.5 }), 1000, 1000, 3, null).box, 1);
eq(xiaoLink(st({ beat: -0.5 }), 1000, 1000, 4, null).box, 3);                  // before the session's beat 0: the bar's last beat
x = xiaoLink(st({ playing: 1, armed: 1, start: 12 }), 1000, 1500, 4, null);   // sent for bar 12, at beat 11: armed
eq([x.playing, x.armed, x.running], [true, true, false]);
x = xiaoLink(st({ playing: 1, armed: 1, start: 12 }), 1000, 1500 + 500, 4, null);   // beat 12 reached between polls: running
eq([x.armed, x.running], [false, true]);
x = xiaoLink(st({ playing: 0, armed: 1, start: 12 }), 1000, 1500, 4, null);   // play waiting to be sent: armed, not playing
eq([x.playing, x.armed, x.running], [false, true, false]);
x = xiaoLink(st(), 1000, 1500, 4, { cmd: 'play', at: 1200 });                 // the page sent play after this status: armed ..
eq([x.armed, x.running], [true, false]);
eq(xiaoLink(st(), 3000, 3500, 4, { cmd: 'play', at: 1200 }).armed, false);    // .. until a status answers it (it said: not playing)
eq(xiaoLink(st(), 1000, 1200 + 2600, 4, { cmd: 'play', at: 1200 }).armed, false);   // .. or 2.5 s went by: never stuck
x = xiaoLink(st({ playing: 1 }), 1000, 1500, 4, { cmd: 'stop', at: 1200 });   // stop sent: shown stopped at once
eq([x.playing, x.armed, x.running], [false, false, false]);
eq(xiaoLink(st({ playing: 1 }), 1000, 1500, 4, null).running, true);
eq(xiaoHearing(null), { on: false, text: 'no report yet' });
eq(xiaoHearing({ audio: false, mode: 'stopped' }).text, 'no audio');
eq(xiaoHearing({ audio: true, live: false, mode: 'stopped' }), { on: false, text: 'stream open, no sound' });     // LINK mode
eq(xiaoHearing({ audio: true, live: false, mode: 'steady' }), { on: false, text: 'stream open, no sound' });
eq(xiaoHearing({ audio: true, live: true, mode: 'stopped' }), { on: true, text: 'hearing music' });             // AUDIO mode, whatever the tracker says

// the control line's answers: dated by their first frame; `status`, `get` and `beat` told apart; an err kept
{
  const XIAO = { text: '', lines: [], answerAt: 0, stats: null, status: null, statusAt: 0, cfg: null, err: '', errAt: 0 };
  const sent = [];
  const env = { XIAO, api: (n, b) => sent.push(n), beatStats, performance: { now: () => 99 },
                xiaoNudged: () => sent.push('nudged'), xiaoNudgeErr: () => {} };            // (M2d's nudge answers: xiao_m2d.js)
  const onXiaoLineText = new Function(...Object.keys(env), grab('onXiaoLineText') + '; return onXiaoLineText;')(...Object.values(env));
  onXiaoLineText('link=1\nlink_peers=2\nbpm=135.00\nbeat=', 500);
  onXiaoLineText('12.500\nplaying=1\nsettled=1\narmed=1\nstart=16.000\n', 530);
  onXiaoLineText('ok\n', 560);
  eq(XIAO.statusAt, 500);                                                     // the first frame's time, not the last
  eq([XIAO.status.bpm, XIAO.status.beat, XIAO.status.playing, XIAO.status.armed, XIAO.status.start], [135, 12.5, 1, 1, 16]);
  onXiaoLineText('group=1\nquantum=3\nbeat=on\nok\n', 700);
  eq(XIAO.cfg.quantum, 3);
  onXiaoLineText('cpu_pct=30\nphase_forces=2\nok\n', 800);
  eq([XIAO.stats.phase_forces, sent], [2, ['xiao_stats']]);
  onXiaoLineText('err audio mode\n', 900);
  eq([XIAO.err, XIAO.errAt, XIAO.statusAt], ['err audio mode', 99, 500]);
}

// the transport icon's armed outside --xiao (armedLocal): only a start that resolves by itself or by YES
{
  const dev = { link: false, hold: false, clocks: true, startAsked: false, ltMode: 2 };
  const armedLocal = new Function('dev', grab('armedLocal') + '; return armedLocal;')(dev);
  const s = (o = {}) => Object.assign({ link_playing: false, silent: false, gated: false, bpm: 128 }, o);
  eq(armedLocal(s()), true);                                                  // AUDIO: music with a tempo, Link not started: locking
  eq(armedLocal(s({ silent: true, bpm: null })), false);                      // AUDIO, silence (start-up, no track): stopped, not armed
  eq(armedLocal(s({ gated: true })), false);
  eq(armedLocal(s({ bpm: null })), false);
  eq(armedLocal(s({ link_playing: true })), false);
  dev.hold = true;
  eq(armedLocal(s()), false);                                                 // held: stopped
  dev.link = true;                                                            // LINK, page start-up / mode switch: no Start asked
  eq(armedLocal(s()), false);
  dev.startAsked = true;                                                      // still held (the user's layer, jank M2g): stopped
  eq(armedLocal(s()), false);
  dev.hold = false;                                                           // the user's Start (linkStartStop releases the hold),
  eq(armedLocal(s()), true);                                                  // the session not playing yet
  eq(armedLocal(s({ link_playing: true })), false);
  dev.ltMode = 1;                                                             // FOLLOW: waiting for the session, YES stops it
  eq(armedLocal(s()), true);
  dev.clocks = false;                                                         // stopped by YES
  eq(armedLocal(s()), false);
  dev.clocks = true;
  dev.ltMode = 0;                                                             // IGNORE with a peer: the clocks run, never armed
  eq(armedLocal(s({ peers: 1 })), false);
  eq(armedLocal(s()), true);                                                  // alone (jank M2l): its own session's next bar line
  eq(armedLocal(s({ link_playing: true })), false);
}

// LINK mode alone (jank M2l, as a lone Ableton Live): Start starts the device's own session whatever the option, and the
// outputs run with it; IGNORE with a peer runs them whatever the session; Stop stops
{
  const sent = [];
  const dev = { link: true, hold: true, clocks: false, startAsked: false, ltMode: 0 };
  const env = { dev, S: { peers: 0 }, hold: () => {}, api: (n, b) => sent.push([n, b]), xiaoSend: () => {}, XIAO: {}, askStatus: () => {},
                performance: { now: () => 0 } };
  const linkStartStop = new Function(...Object.keys(env), grab('linkStartStop') + '; return linkStartStop;')(...Object.values(env));
  const outputsRun = new Function('dev', html.match(/const outputsRun = [^\n]*\n/)[0] + '; return outputsRun;')(dev);
  for (const lt of [0, 1, 2]) {
    dev.ltMode = lt;
    sent.length = 0;
    env.S.peers = 0;
    linkStartStop(true);
    eq(sent.some(([n, b]) => n === 'transport' && b.playing === true), true);           // alone: the session starts, any option
    eq([outputsRun({ link_playing: false, peers: 0 }), outputsRun({ link_playing: true, peers: 0 })], [false, true]);
    linkStartStop(false);
    eq(sent.some(([n, b]) => n === 'transport' && b.playing === false), true);
    eq(outputsRun({ link_playing: true, peers: 0 }), false);                            // held
    env.S.peers = 1;
    sent.length = 0;
    linkStartStop(true);
    eq(sent.some(([n]) => n === 'transport'), lt === 2);                                 // with a peer: FOLLOW+LEAD only
    eq(outputsRun({ link_playing: false, peers: 1 }), lt === 0);                         // IGNORE: a peer is enough
  }
}
console.log('xiao page helpers: ' + checks + ' checks passed');
