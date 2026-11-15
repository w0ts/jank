// node xiao_m2e.js PLAYER_HTML: the page against the firmware M2e (fw/integ-4): with --xiao, AUDIO > LOW CONFIDENCE and
// THRESHOLD send `set lowconf stop|run` / `set threshold N` to the XIAO and are read back from it (status, else get); the CF
// bar is the XIAO's conf (the grid-fit confidence) and the gate (stopped icon, LED) its gated= / silent=
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const consts = 'const XIAO_STATUS_FRESH_MS = 5000; const XIAO_HOME_FRESH_MS = 1500;';
const fns = ['beatStats', 'xiaoLowConf', 'xiaoGate', 'xiaoMeters', 'lowConfCommand'];
const P = new Function(consts + fns.map(name => name === 'lowConfCommand' ? html.match(/function lowConfCommand\([^)]*\) \{[^\n]*\}\n/)[0] : grab(name)).join('\n')
                       + '; return { ' + fns.join(', ') + ' };')();
ok(Number(/const XIAO_STATUS_FRESH_MS = (\d+);/.exec(html)[1]) === 5000, 'the freshness the test assumes');

// ---- the XIAO's answers: lowconf= is a word, threshold= / conf= / act= / gated= / silent= numbers ----
const status = P.beatStats('conf=62\nact=91\ngated=1\nsilent=0\nthreshold=70\nlowconf=run\nlink_peers=1'.split('\n'));
eq([status.conf, status.act, status.gated, status.silent, status.threshold, status.lowconf], [62, 91, 1, 0, 70, 'run']);
const get = P.beatStats('quantum=4\nlowconf=stop\nthreshold=80\nusbmidi=on'.split('\n'));

// ---- the settings read back: a fresh status first, else get; an older firmware: null (the page keeps its own) ----
eq(P.xiaoLowConf(get, status, 1000, 2000), { confStop: false, th: 70 });
eq(P.xiaoLowConf(get, status, 1000, 7000), { confStop: true, th: 80 });                 // the status too old: get's
eq(P.xiaoLowConf(get, null, 0, 2000), { confStop: true, th: 80 });
eq(P.xiaoLowConf(P.beatStats(['quantum=4']), P.beatStats(['link_peers=1', 'conf=40']), 1000, 1500), null);   // M2d: nothing to read
eq(P.xiaoLowConf(null, null, 0, 0), null);

// ---- the gate as the XIAO says it ----
eq(P.xiaoGate(status, 1000, 2000), { gated: true, silent: false });
eq(P.xiaoGate(P.beatStats(['gated=0', 'silent=1']), 1000, 2000), { gated: false, silent: true });
eq(P.xiaoGate(status, 1000, 7000), null);                                                // too old
eq(P.xiaoGate(P.beatStats(['link_peers=1', 'conf=40']), 1000, 2000), null);               // M2d: the page compares the bar itself

// ---- the CF bar: the XIAO's conf (the grid fit), from home or status ----
eq(P.xiaoMeters(null, 0, { rms_db: -20, rms_min_db: -25, rms_max_db: -15, conf: 62, act: 91 }, 1000, 1500).conf, 0.62);

// ---- the menu items send the XIAO's commands, the item changed ----
eq(P.lowConfCommand('lowconf', true, 80), 'set lowconf stop');
eq(P.lowConfCommand('lowconf', false, 80), 'set lowconf run');
eq(P.lowConfCommand('threshold', true, 65), 'set threshold 65');
eq(P.lowConfCommand('threshold', false, 0), 'set threshold 0');
const send = grab('sendThreshold');
ok(/if \(S && S\.xiao\) \{ xiaoSend\(lowConfCommand\(item, dev\.confStop, dev\.th\)\); setTimeout\(askGet, 300\); return; \}/.test(send), '--xiao: the XIAO, read back from get');
ok(send.indexOf('xiaoSend') < send.indexOf("api('threshold'"), '--xiao: not the local model');
ok(/if \(field === 'threshold'\) \{ dev\.th = val; sendThreshold\('threshold'\); \}/.test(html), 'THRESHOLD sends set threshold');
ok(/else if \(field === 'lowconf'\) \{ dev\.confStop = val === 1; sendThreshold\('lowconf'\); \}/.test(html), 'LOW CONFIDENCE sends set lowconf');

// ---- the screen: the XIAO's gate and silence when it says them; synced from the XIAO, not the player, with --xiao ----
ok(/gated: s\.xiao \? \(xg \? xg\.gated : !!xm && dev\.confStop && mn\.conf \* 100 < dev\.th\) : !!s\.gated/.test(html), 'gated: the XIAO\'s');
ok(/silent: !audioIn \|\| \(!s\.xiao && !!s\.silent\) \|\| !!\(xg && xg\.silent\)/.test(html), 'silent: the XIAO\'s too');
ok(/const xg = s\.xiao && !dev\.link \? xiaoGate\(XIAO\.status, XIAO\.statusAt, now\) : null;/.test(html), 'AUDIO mode only');
const sync = grab('syncFromStatus');
ok(/if \(now > holdUntil\.th && s\.xiao\) \{[^}]*xiaoLowConf\(XIAO\.cfg, XIAO\.status, XIAO\.statusAt, now\)/.test(sync), 'synced from the XIAO');
ok(sync.indexOf('xiaoLowConf') < sync.indexOf('s.min_confidence'), 'not from the player with --xiao');
console.log('xiao m2e: ' + checks + ' checks passed');
