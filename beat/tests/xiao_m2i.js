// node xiao_m2i.js PLAYER_HTML: the page against the firmware M2i (fw/integ-4): status's follow= is a word, drift_ppm= and
// rate_ratio= numbers; the follower's line says the state, the glide's drift and the last ratio taken
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
let checks = 0;
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const fns = ['beatStats', 'followText'];
const P = new Function(fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

const st = P.beatStats('conf=97\nfollow=glide\ndrift_ppm=-35\nrate_ratio=1.0200\nlink_peers=1'.split('\n'));
eq([st.conf, st.follow, st.drift_ppm, st.rate_ratio], [97, 'glide', -35, 1.02]);
eq(P.followText(st), 'glide, drift -35 ppm, rate x1.0200');
eq(P.followText(P.beatStats(['follow=lock', 'drift_ppm=0', 'rate_ratio=1.0000'])), 'lock');
eq(P.followText(P.beatStats(['follow=stopped', 'drift_ppm=0', 'rate_ratio=0.9700'])), 'stopped, rate x0.9700');
eq(P.followText(P.beatStats(['link_peers=1', 'conf=40'])), null);                  // M2h: no follower line
eq(P.followText(null), null);
eq(P.followText({ follow: 'glide', drift_ppm: 12, rate_ratio: 1.0 }), 'glide, drift 12 ppm');   // the player's own (JSON)
console.log(checks + ' checks passed');
