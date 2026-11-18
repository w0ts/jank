// The state the reference cases start from, and how it is written into a cases file (gen_cases.js, gen_ink.js; read by tests/test_oled.c).
'use strict';

function baseState() {
  return {
    link: false, lineIn: true, bpm: 124, linkBpm: 124, conf: 0.9, th: 80, gain: 0, confStop: true,
    levelDb: -20, levelMin: -25, levelMax: -15, running: true, armed: false, trackerLeads: false, xiao: false,
    beatN: 1, peers: 2, wifi: 'good', wifiQ: null, usb: true, fitted: true, batt: 80,
    ltMode: 2, inSrc: 0, usbRole: 0, autoBar: true, bar: { state: 'locked', conf: 1 }, inOff: 0, offs: [0, 0, 0],
    rate: 2, bright: 100, sleep: 0, boot: false, splash: false, saved: false, groupPeers: 3,
    net: { mode: 0, prev: 0, group: 1, ssid: 'HomeNet', pass: 'letmein42', appass: 'jankjank', apName: 'jank-a3f9',
           status: 'CONNECTED', ip: '192.168.1.42', apClients: 1, setupName: 'jank-setup-a3f9',
           scan: [{ name: 'HomeNet', bars: 3 }, { name: 'jank-studio', bars: 2 }, { name: 'Cafe Guest', bars: 1 }, { name: 'FritzBox 7590', bars: 2 }] },
  };
}

const ABOUT_KEYS = ['version', 'tempC', 'cpuPct', 'heapKb', 'psramKb', 'flashKb', 'appUsedKb', 'appKb', 'uptimeS', 'heapMinKb', 'reset', 'clkLateUs', 'uiLoopUs', 'beatLoopUs'];
// what the ABOUT cases show: every number known
const ABOUT_FULL = { version: '0.1.0', tempC: 41, cpuPct: 37, heapKb: 123, psramKb: 7936, flashKb: 8192, appUsedKb: 1260, appKb: 3072, uptimeS: 3725, heapMinKb: 98, reset: 'POWERON', clkLateUs: 12, uiLoopUs: 4210, beatLoopUs: 8800 };
const ABOUT_NONE = Object.fromEntries(ABOUT_KEYS.map(k => [k, null]));

function dumpState(out, s) {
  const L = (k, v) => out.push('c ' + k + ' ' + v);
  const num = v => (v === null || v === undefined ? 'null' : String(v));
  for (const k of ['link', 'lineIn', 'confStop', 'running', 'armed', 'trackerLeads', 'xiao', 'usb', 'fitted', 'autoBar', 'boot', 'splash', 'saved'])
    L(k, s[k] ? 1 : 0);
  for (const k of ['bpm', 'linkBpm', 'conf', 'th', 'gain', 'levelDb', 'levelMin', 'levelMax', 'beatN', 'peers', 'batt', 'ltMode', 'inSrc',
                   'usbRole', 'inOff', 'rate', 'bright', 'sleep', 'groupPeers'])
    L(k, num(s[k]));
  if ('cpuPct' in s) L('about', ABOUT_KEYS.map(k => num(s[k])).join(' '));   // the ABOUT screen's numbers (null = unknown); only the cases that show it carry them
  L('wifi', s.wifi);
  L('offs', s.offs.join(' '));
  L('wifiQ', s.wifiQ ? [s.wifiQ.rssi, s.wifiQ.kbps, s.wifiQ.tx, s.wifiQ.retried, s.wifiQ.lost].join(' ') : 'null');
  L('bar', s.bar ? s.bar.state + ' ' + s.bar.conf : 'null');
  const n = s.net;
  for (const k of ['mode', 'prev', 'group', 'apClients']) L('net.' + k, n[k]);
  for (const k of ['ssid', 'pass', 'appass', 'apName', 'status', 'ip', 'setupName']) L('net.' + k, n[k]);
  for (const r of n.scan) L('scan', r.bars + ' ' + r.name);
}
function hex(fb, W, H) {
  const pages = new Uint8Array(W * H / 8);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (fb[y * W + x]) pages[(y >> 3) * W + x] |= 1 << (y & 7);
  return Buffer.from(pages).toString('hex');
}

module.exports = { baseState, dumpState, hex, ABOUT_FULL, ABOUT_NONE };
