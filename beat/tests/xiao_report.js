// node xiao_report.js PLAYER_HTML: the page's XIAO helpers (reports, control-line frames, `beat` numbers) against the
// firmware's formats (jank core/jank_sysex.h)
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
const src = ['const JX_TEXT_MAX = 30;', grab('xiaoReport'), grab('xiaoBeatEpochMs'), grab('jxText'), grab('jxFrames'), grab('beatStats')].join('\n');
const { xiaoReport, xiaoBeatEpochMs, jxText, jxFrames, beatStats } =
  new Function(src + '; return { xiaoReport, xiaoBeatEpochMs, jxText, jxFrames, beatStats };')();
const frame = (text, type = 0x10, tag = 0x42) => Uint8Array.from([0xf0, 0x7d, 0x4a, tag, type, ...Buffer.from(text, 'ascii'), 0xf7]);
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };

eq(xiaoReport(frame('b 17 12403 steady 1')), { kind: 'b', n: 17, bpm: 124.03, mode: 'steady', audio: true });
eq(xiaoReport(frame('b 17 12403 steady 1 31250')), { kind: 'b', n: 17, bpm: 124.03, mode: 'steady', audio: true, lagUs: 31250 });
eq(xiaoReport(frame('b 2 12403 steady 1 -4000')).lagUs, -4000);
eq(xiaoReport(frame('b 3 12403 steady 1 31250 -4300')).linkUs, -4300);       // the Link session's beat from it
eq(xiaoReport(frame('b 3 12403 steady 1 31250 -')).linkUs, undefined);        // no session playing
eq(xiaoReport(frame('t 0 0 stopped 0 25 -')).latencyMs, 25);
eq(xiaoReport(frame('t 0 0 stopped 0 25')), { kind: 't', n: 0, bpm: 0, mode: 'stopped', audio: false, latencyMs: 25 });
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
console.log('xiao page helpers: ' + checks + ' checks passed');
