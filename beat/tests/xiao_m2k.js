// node xiao_m2k.js PLAYER_HTML: the page against the firmware M2k (fw/integ-4): the confidence's HOLD, as `home` sends it
// (conf "<pct>h") and `status` says it (conf_hold=1): the CF bar's numbers carry it, the value is the one shown
const fs = require('fs');
const assert = require('assert');
const html = require('../../tests/load_html.js')(process.argv[2]);
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
let checks = 0;
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const consts = 'const XIAO_HOME_FRESH_MS = 2000, XIAO_STATUS_FRESH_MS = 5000, XIAO_FLOOR_DB = -99;';
const fns = ['homeMode', 'parseHome', 'xiaoMeters', 'meterNumbers'];
const P = new Function(consts + fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

const held = P.parseHome('home=h 12000 j3 1 0 4 2 0 - -12 93h -15 -9 1');
eq([held.conf, held.confHold], [93, true]);
const plain = P.parseHome('home=h 12000 j3 1 0 4 2 0 - -12 93 -15 -9 1');
eq([plain.conf, plain.confHold], [93, false]);
let xm = P.xiaoMeters(held, 1000, null, 0, 1500);
eq([xm.conf, xm.hold], [0.93, true]);
eq(P.meterNumbers(true, xm, null).hold, true);
xm = P.xiaoMeters(plain, 1000, null, 0, 1500);
eq(P.meterNumbers(true, xm, null).hold, false);
xm = P.xiaoMeters(null, 0, { rms_db: -20, conf: 85, conf_hold: 1 }, 1000, 1500);   // no fresh `home`: `status`
eq([xm.conf, xm.hold], [0.85, true]);
eq(P.xiaoMeters(null, 0, { rms_db: -20, conf: 85 }, 1000, 1500).hold, false);    // an older firmware: no hold
eq(P.meterNumbers(true, null, null).hold, false);
console.log(checks + ' checks passed');
