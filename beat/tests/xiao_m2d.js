// node xiao_m2d.js PLAYER_HTML: the page against the firmware M2d (fw/integ-3): with --xiao, NO held + UP/DOWN sends
// `nudge beat +-0.5` and YES held + UP/DOWN `nudge ms +-1`, the overlays from the XIAO's answers (nudge_beats=, the user's
// total; beat_latency=, the AUDIO IN offset) and its refusals; the USB MIDI CLOCK menu item (`set usbmidi`, read back from `get`)
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
let checks = 0;
const ok = (c, msg) => { assert.ok(c, msg); checks++; };
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const fns = ['halvesText', 'nudgeOverlay', 'nudgeErrOverlay', 'beatStats'];
const P = new Function(fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

// ---- the beats in halves ----
eq(['0.5', '-0.5', '1', '-1.5', '3', '0'].map(Number).map(P.halvesText), ['+1/2', '-1/2', '+1', '-1 1/2', '+3', '0']);

// ---- the overlays from the answers, as beatStats reads them ----
const answer = text => P.beatStats(text.split('\n'));
const beat = answer('nudge_beats=0.5\nbar=locked\nbar_phi=2\nbar_count=37\nbar_scores=1.20,-0.30,0.10,0.00');
eq(beat.nudge_beats, 0.5);
eq(P.nudgeOverlay('no', beat), { title: 'NUDGE BEAT', value: '+1/2 BEAT' });
eq(P.nudgeOverlay('no', answer('nudge_beats=-1.5')), { title: 'NUDGE BEAT', value: '-1 1/2 BEAT' });
eq(P.nudgeOverlay('no', answer('nudge_beats=0')), { title: 'NUDGE BEAT', value: '0 BEAT' });
eq(P.nudgeOverlay('yes', answer('beat_latency=3')), { title: 'NUDGE MS', value: 'AUDIO IN +3 MS' });
eq(P.nudgeOverlay('yes', answer('beat_latency=-12')), { title: 'NUDGE MS', value: 'AUDIO IN -12 MS' });
eq(P.nudgeOverlay('yes', answer('beat_latency=0')), { title: 'NUDGE MS', value: 'AUDIO IN 0 MS' });
eq(P.nudgeErrOverlay('err nudge beat at its limit, -3 to 3 beats in all'), { title: 'NUDGE BEAT', value: 'MAX 3 BEATS' });
eq(P.nudgeErrOverlay('err no beat yet'), { title: 'NUDGE BEAT', value: 'NO BEAT YET' });
eq(P.nudgeErrOverlay('err link mode'), { title: 'LINK MODE', value: '' });
eq(P.nudgeErrOverlay('err busy'), { title: 'NUDGE', value: 'BUSY' });
eq(P.nudgeErrOverlay('err tempo is 20 to 999 BPM'), null);                      // not a nudge's
eq(answer('usbmidi=off\nquantum=4').usbmidi, 'off');                              // `get`'s USB MIDI CLOCK

// ---- the answers routed: `get` (quantum, beat_latency) and `status` (link_peers, nudge_beats) are not nudges' ----
const route = html.match(/else if \('quantum' in stats\) XIAO\.cfg = stats;\n\s*else if \('nudge_beats' in stats\) xiaoNudged\('no', stats\);[^\n]*\n\s*else if \('beat_latency' in stats\) xiaoNudged\('yes', stats\);/);
ok(route, 'nudge answers routed after get and status');
ok(html.indexOf("else if ('link_peers' in stats)") < html.indexOf("else if ('nudge_beats' in stats)"), 'status (with its nudge_beats=) first');

// ---- the combos send the XIAO's commands ----
const combo = grab('comboStep');
ok(/xiaoSend\(mod === 'no' \? 'nudge beat ' \+ \(d > 0 \? '\+0\.5' : '-0\.5'\) : 'nudge ms ' \+ \(d > 0 \? '\+1' : '-1'\)\)/.test(combo), 'NO: nudge beat +-0.5, YES: nudge ms +-1');
ok(!/TRACKER LEADS/.test(combo), 'no "TRACKER LEADS" any more');
ok(combo.indexOf('if (dev.link) return;') < combo.indexOf('xiaoSend'), 'LINK mode: nothing sent');

// ---- USB MIDI CLOCK in the menu ----
ok(/usbmidi: \{ title: 'USB MIDI CLOCK', opts: \['ON', 'OFF'\]/.test(html), 'the menu edit, ON / OFF');
ok(/\{ name: 'USB MIDI CLOCK', edit: 'usbmidi' \}/.test(html), 'in the menu');
ok(/xiaoSend\('set usbmidi ' \+ \(val === 0 \? 'on' : 'off'\)\)/.test(html), '--xiao: set usbmidi');
const help = html.match(/usbmidi: \{[^\n]*\n\s*help: (\[\[.*\]\]) \},/);
ok(help, 'two-line help');
for (const pair of JSON.parse(help[1].replace(/'/g, '"'))) for (const line of pair) ok(line.length <= 20, 'help line fits: ' + line);
console.log('xiao m2d: ' + checks + ' checks passed');
