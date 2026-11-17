// node autolat.js PLAYER_HTML: the page's Auto latency: the first send, the 2 ms debounce and the 2 s gap, the USB gate,
// manual (the checkbox; M2h: Calibrate and NUDGE MS set the AUDIO SHIFT on top and never turn it off) for the session,
// switching it back on, a reconnect, the Tab audio suggestion, Calibrate's shift and the labels
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(process.argv[2], 'utf8');
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
const konst = name => html.match(new RegExp('const ' + name + ' = ([0-9.-]+);'))[0];
let checks = 0;
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const consts = ['AUTOLAT_MIN_CHANGE_MS', 'AUTOLAT_MIN_GAP_MS', 'TAB_PATH_CONSTANT_MS'].map(konst).join('\n');
const fns = ['autoLatNew', 'autoLatStep', 'autoLatSuggestion', 'autoLatLabel', 'calibTrim'];
const P = new Function(consts + '\n' + fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

// drive the machine: returns the sends of a list of events
const run = (events, st = P.autoLatNew()) => {
  const sends = [];
  for (const ev of events) { const r = P.autoLatStep(st, ev); st = r.st; if (r.send !== null) sends.push(r.send); }
  return { st, sends };
};
const tick = (sug, now, usb = true) => ({ type: 'tick', sug, usb, now });

// the first suggestion goes out at once, rounded; the same one again does not
let r = run([tick(-25.6, 0), tick(-26, 100), tick(-26, 5000)]);
eq(r.sends, [-26]);
eq(r.st.sent, -26);

// debounce: a move of 2 ms or less stays quiet, more than 2 ms goes out, but not twice inside the gap
r = run([tick(-26, 0), tick(-24, 5000), tick(-23, 6000)]);          // -24 is 2 away: quiet; -23 is 3 away: sent
eq(r.sends, [-26, -23]);
r = run([tick(-26, 0), tick(-10, 500), tick(-10, 2100)]);           // inside the 2 s gap: held, then sent
eq(r.sends, [-26, -10]);
r = run([tick(0, 0), tick(10, 2000), tick(20, 2500), tick(20, 4100)]);
eq(r.sends, [0, 10, 20]);

// no USB source (line / none), no suggestion, or auto off: nothing goes out
eq(run([tick(5, 0, false), tick(5, 3000, false)]).sends, []);
eq(run([tick(null, 0), tick(NaN, 1000)]).sends, []);
// the XIAO's source turning to USB sends again (a new stream: its own remembered value)
eq(run([tick(5, 0), tick(5, 3000, false), tick(5, 4000, true)]).sends, [5, 5]);
eq(run([tick(5, 0, false), tick(5, 1000, true)]).sends, [5]);

// manual (Calibrate, NUDGE MS): off for the session, never sends, the state says so
r = run([tick(-26, 0), { type: 'manual' }, tick(-60, 9000), tick(-90, 20000)]);
eq(r.sends, [-26]);
eq([r.st.on, r.st.manual], [false, true]);
eq(P.autoLatLabel(r.st, -60, true, false), 'manual (auto had sent -26 ms)');
eq(P.autoLatLabel(run([{ type: 'manual' }]).st, 5, true, false), 'manual');
// switching Auto latency back on sends the current suggestion at once
r = run([tick(-26, 0), { type: 'manual' }, { type: 'auto' }, tick(-60, 9000)]);
eq(r.sends, [-26, -60]);
eq([r.st.on, r.st.manual], [true, false]);
// a new MIDI port (the XIAO replugged) sends again, even with the same value
r = run([tick(-26, 0), { type: 'reconnect' }, tick(-26, 100)]);
eq(r.sends, [-26, -26]);
// unknown events change nothing
eq(P.autoLatStep(P.autoLatNew(), { type: 'zz' }).send, null);

// the suggestion: the player's paths, or on the Tab audio pane the aligned constant (only while listening through jank)
eq(P.autoLatSuggestion('player', 'idle', false, -26), -26);
eq(P.autoLatSuggestion('player', 'idle', false, undefined), null);
eq(P.autoLatSuggestion('tab', 'listening', true, -26), 0);
eq(P.autoLatSuggestion('tab', 'idle', false, -26), null);
eq(P.autoLatSuggestion('tab', 'listening', false, -26), null);          // the player's own tracker: no XIAO involved
// switching the pane moves the suggestion: it goes out when it differs by more than 2 ms
eq(run([tick(P.autoLatSuggestion('player', 'idle', false, -26), 0), tick(P.autoLatSuggestion('tab', 'listening', true, -26), 3000)]).sends, [-26, 0]);

// the labels
const fresh = P.autoLatNew();
eq(P.autoLatLabel(fresh, 5, false, false), 'waiting: the XIAO\'s source is not USB');
eq(P.autoLatLabel(fresh, null, true, false), 'waiting for a suggestion');
eq(P.autoLatLabel({ ...fresh, sent: -26 }, -26, true, true), 'auto: -26 ms (sent)');
eq(P.autoLatLabel({ ...fresh, on: false }, 5, true, false), 'off');
eq(P.autoLatLabel({ ...fresh, sent: -65 }, -65, true, false, 3), 'auto -65 ms + trim 3 ms');   // M2h: the XIAO's two layers
eq(P.autoLatLabel({ ...fresh, sent: -65 }, -65, true, true, 0), 'auto -65 ms + trim 0 ms (sent)');
// Calibrate (M2h): the shift that makes auto + shift the suggestion; the layers unknown (an older firmware): null
eq(P.calibTrim(-60, { auto: -65, trim: 0 }), 5);
eq(P.calibTrim(-70.4, { auto: -65, trim: 3 }), -5);
eq(P.calibTrim(-60, null), null);
eq(P.calibTrim(null, { auto: -65, trim: 0 }), null);

console.log(checks + ' checks passed');
