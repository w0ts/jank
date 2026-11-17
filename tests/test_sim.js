// SPDX-License-Identifier: MIT
// The control page's simulator model (ui/sim.js) driving the real OLED module (ui/oled.js), with no browser: button
// semantics, the menu, tempo, auto-repeat, the LEDs and the keyboard mapping. Exit status: non-zero on a failure.
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const read = name => fs.readFileSync(path.join(__dirname, '..', 'ui', name), 'utf8');
const { OLED, Sim } = new Function(read('oled.js') + read('sim.js') + '; return { OLED, Sim };')();

let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };
const ok = (c, what) => { checks++; assert.ok(c, what); };
const click = (m, b, at) => { Sim.down(m, b, at); Sim.up(m, b, at + 50); };
const frame = (m, now) => Sim.framebuffer(m, now).join('');

// --- HOME: the AUDIO face with nothing playing: no tempo, no level (it never shows demo numbers) ---
let m = Sim.create();
eq(m.ui.screen, 'home', 'starts on HOME');
eq(m.bpm(), null, 'AUDIO mode: no tempo without a tracker');
ok(!m.running(), 'AUDIO mode without a tracker: the outputs do not run');
const idle = m.ctx(0);
eq([idle.bpm, idle.conf, idle.levelDb, idle.levelMin, idle.levelMax, idle.silent, idle.beatN, idle.bar], [null, 0, -90, -90, -90, true, null, null], 'Demo: silent, level at the floor, no beat, no confidence, no bar');
const home = frame(m, 0);
ok(home.includes('1'), 'the HOME screen draws something');

// --- NO opens the MENU, on release ---
Sim.down(m, 'no', 100);
eq(m.ui.screen, 'home', 'NO on HOME acts on release, not on press');
Sim.up(m, 'no', 150);
eq(m.ui.screen, 'menu', 'NO released: the MENU');
ok(frame(m, 200) !== home, 'the MENU looks different from HOME');

// --- UP / DOWN move, YES enters, NO goes back ---
const cursor0 = m.ui.cursor;
click(m, 'down', 300);
eq(m.ui.cursor, cursor0 + 1, 'DOWN moves the cursor down');
click(m, 'up', 400);
eq(m.ui.cursor, cursor0, 'UP moves it back');
click(m, 'yes', 500);
eq(m.ui.screen, 'sub', 'YES on the first item (AUDIO) enters its list');
click(m, 'no', 600);
eq(m.ui.screen, 'menu', 'NO goes back to the MENU');
click(m, 'no', 700);
eq(m.ui.screen, 'home', 'NO on the MENU goes back to HOME');

// --- auto-repeat of a held UP / DOWN off HOME ---
m = Sim.create();
click(m, 'no', 0);
Sim.down(m, 'down', 1000);
eq(m.ui.cursor, 1, 'the press moved once');
Sim.tick(m, 1000 + Sim.REPEAT.firstMs - 1);
eq(m.ui.cursor, 1, 'no repeat before the first delay');
Sim.tick(m, 1000 + Sim.REPEAT.firstMs + Sim.REPEAT.everyMs);
eq(m.ui.cursor, 3, 'two repeats by then');
ok(Sim.busy(m, 3000), 'busy while a button repeats');
Sim.up(m, 'down', 3000);
Sim.tick(m, 9000);
eq(m.ui.cursor, 3, 'released: it stops');

// --- tempo: AUDIO folds x2 / /2 (no tempo to show), LINK steps 1 BPM ---
m = Sim.create();
click(m, 'up', 0);
eq(m.ctx(0).fold, 2, 'AUDIO: UP on HOME doubles the fold');
eq([m.ui.overlay.title, m.ui.overlay.value], ['DOUBLE', '---'], 'its overlay: no tempo known, so ---');
click(m, 'down', 100); click(m, 'down', 200);
eq(m.ctx(0).fold, 0.5, 'DOWN twice: halved');
m = Sim.create();
m.setField('mode', 1);
const before = frame(m, 0);
eq(m.ctx(0).tempoStep(1), 121, 'LINK: a tempo step adds 1 BPM');
eq(m.bpm(), 121, 'the model keeps it');
click(m, 'up', 10);
eq(m.bpm(), 122, 'UP on HOME in LINK mode steps the tempo');
ok(frame(m, 2000) !== before, 'the screen shows it');
for (let i = 0; i < 100; i++) m.tempoStep(1);
eq(m.bpm(), 180, 'the tempo stops at the tracker range end');

// --- YES on HOME stops and starts the outputs; in LINK mode the LEDs follow Link's tempo (a setting), in AUDIO mode there is no beat ---
m = Sim.create();
eq(Sim.ledCss(m, 0), [null, null, null, null], 'AUDIO mode: no beat, dark LEDs');
m.setField('mode', 1);
const beat0 = Sim.ledCss(m, 0);
eq(beat0[0], 'rgb(255,158,0)', 'beat 1 of the bar: the amber LED 1 flashes');
eq(beat0.slice(1), [null, null, null], 'only LED 1');
eq(Sim.ledCss(m, 300), [null, null, null, null], 'dark between flashes');
eq(Sim.ledCss(m, 500)[1], 'rgb(0,215,0)', 'beat 2: LED 2 green');
click(m, 'yes', 600);
ok(!m.running(), 'YES on HOME stops');
eq(Sim.ledCss(m, 1000), [null, null, null, null], 'stopped: dark');
click(m, 'yes', 1100);
ok(m.running(), 'YES again starts');
ok(Sim.nextBeatInMs(m, 100) > 0 && Sim.nextBeatInMs(m, 100) <= 500, 'LINK: the next beat is within a beat');
m.setField('mode', 0);
eq(Sim.nextBeatInMs(m, 100), 500, 'AUDIO: a slow refresh, no beat edges');

// --- HOME combos are not modelled: NO held + UP does nothing and does not open the MENU ---
m = Sim.create();
Sim.down(m, 'no', 0); click(m, 'up', 10); Sim.up(m, 'no', 100);
eq(m.ui.screen, 'home', 'a combo does not act as a NO click');
eq(m.ctx(0).fold, 1, 'nor change the fold');

// --- the Demo is silent: no level, no confidence, no tempo, no beat, whatever is pressed ---
m = Sim.create();
click(m, 'up', 0); click(m, 'yes', 100); click(m, 'yes', 200);
const demo = m.ctx(5000);
eq([demo.levelDb, demo.levelMin, demo.levelMax, demo.silent, demo.conf, demo.bpm, demo.beatN, demo.bar], [-90, -90, -90, true, 0, null, null, null], 'Demo: silent state, nothing invented');
eq(Sim.ledCss(m, 5000), [null, null, null, null], 'Demo: the LEDs do not blink');
ok(!Sim.busy(m, 5000) || m.ui.overlay !== null, 'Demo: nothing keeps redrawing by itself');

// --- fields ---
m = Sim.create();
m.setField('threshold', 55);
eq(m.ctx(0).th, 55, 'setField threshold');
m.setField('out1', 7);
eq(m.ctx(0).offs, [0, 7, 0], 'setField out1');
m.setField('group', 2);
eq(m.ctx(0).net.group, 3, 'the group choice index is 0-based');
m.setField('netmode', 3);
eq(m.ctx(0).net.mode, 0, 'SETUP is not modelled: the network mode stays');
assert.throws(() => m.setField('nope', 1), /unknown field/);
checks++;

// --- the keyboard: the simulator's mapping ---
eq(Sim.KEY_BTN, { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'no', Escape: 'no', Backspace: 'no', ArrowRight: 'yes', Enter: 'yes' }, 'key table');
for (const key of Object.keys(Sim.KEY_BTN)) ok(['up', 'down', 'no', 'yes'].includes(Sim.KEY_BTN[key]), key + ' maps to a button');
ok(/Up \/ Down/.test(Sim.KEY_HINT) && /Esc/.test(Sim.KEY_HINT) && /Enter/.test(Sim.KEY_HINT), 'the hint names the keys');

console.log('sim: ' + checks + ' checks passed');
