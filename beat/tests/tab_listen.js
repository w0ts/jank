// node tab_listen.js PLAYER_HTML: the "Listen to a tab" helpers: the capture options, the jank device pick by label,
// the tab label, the level, the latency alignment and the start / stop / error state machine
const fs = require('fs');
const assert = require('assert');
const html = require('../../tests/load_html.js')(process.argv[2]);
const grab = name => html.match(new RegExp('function ' + name + '\\([^)]*\\) \\{[\\s\\S]*?\\n\\}\\n'))[0];
let checks = 0;
const eq = (a, b) => { assert.deepStrictEqual(a, b); checks++; };
const fns = ['panePick', 'paneSwitchPlan', 'tabCaptureOptions', 'pickJankDevice', 'tabLabel', 'tabLevelDb', 'latencyAlign', 'tabStep'];
const P = new Function('const TAB_MAX_DELAY_S = ' + html.match(/TAB_MAX_DELAY_S = ([0-9.]+)/)[1] + ';\n' + fns.map(grab).join('\n') + '; return { ' + fns.join(', ') + ' };')();

// Chrome's options: tabs, the tab itself muted, no window / system audio, not this page
const o = P.tabCaptureOptions();
eq([o.video, o.preferCurrentTab, o.selfBrowserSurface, o.systemAudio], [true, false, 'exclude', 'exclude']);
eq(o.audio.suppressLocalAudioPlayback, true);
eq([o.audio.echoCancellation, o.audio.noiseSuppression, o.audio.autoGainControl], [false, false, false]);

// the jank output by label (case-insensitive), outputs only
const dev = (kind, label, id) => ({ kind, label, deviceId: id });
const list = [dev('audioinput', 'jank', 'in1'), dev('audiooutput', 'MacBook Pro Speakers', 'sp'), dev('audiooutput', 'jank (USB Audio)', 'j1'), dev('audiooutput', 'Default', 'default')];
eq(P.pickJankDevice(list, 'jank'), 'j1');
eq(P.pickJankDevice([dev('audiooutput', 'JANK', 'J')], 'jank'), 'J');
eq(P.pickJankDevice(list.slice(0, 2), 'jank'), null);                       // an input named jank does not count
eq(P.pickJankDevice([dev('audiooutput', '', 'x')], 'jank'), null);          // no permission: empty labels
eq(P.pickJankDevice([], 'jank'), null);
eq(P.pickJankDevice(null, 'jank'), null);

eq(P.tabLabel('web-contents-media-stream://12:3'), 'a Chrome tab');
eq(P.tabLabel(''), 'a Chrome tab');
eq(P.tabLabel('Daft Punk - YouTube'), 'Daft Punk - YouTube');
eq(P.tabLevelDb(0), -99);
eq(Math.round(P.tabLevelDb(0.1)), -20);
eq(P.tabLevelDb(1e-9), -99);

// the lower-latency sink gets the difference as a delay, capped
eq(P.latencyAlign(0.020, 0.050), { main: 0.030000000000000002, jank: 0 });
eq(P.latencyAlign(0.050, 0.020).main, 0);
eq(P.latencyAlign(0.0, 5), { main: 0.5, jank: 0 });
eq(P.latencyAlign(undefined, NaN), { main: 0, jank: 0 });

// the state machine
const S = P.tabStep;
eq(S('idle', { type: 'start' }).state, 'asking');
eq(S('asking', { type: 'start' }).state, 'asking');                          // a second click while the dialog is open
eq(S('listening', { type: 'start' }).state, 'listening');
let r = S('asking', { type: 'granted', hasAudio: true, label: 'web-contents-media-stream://1:2' });
eq([r.state, r.msg, r.error], ['listening', 'Listening to a Chrome tab', false]);
r = S('asking', { type: 'granted', hasAudio: true, label: 'Some Tab' });
eq(r.msg, 'Listening to Some Tab');
r = S('asking', { type: 'granted', hasAudio: false });
eq([r.state, r.error], ['idle', true]);
assert.ok(/Share tab audio/.test(r.msg)); checks++;
r = S('asking', { type: 'denied', name: 'NotAllowedError' });
eq([r.state, r.error], ['idle', false]);
r = S('asking', { type: 'denied', name: 'NotFoundError' });
eq([r.state, r.error], ['idle', true]);
assert.ok(/NotFoundError/.test(r.msg)); checks++;
r = S('asking', { type: 'nodevice' });
eq([r.state, r.error], ['idle', true]);
assert.ok(/jank/.test(r.msg)); checks++;
r = S('listening', { type: 'ended' });
eq([r.state, r.error], ['idle', false]);
assert.ok(/stopped/.test(r.msg)); checks++;
eq(S('idle', { type: 'ended' }).msg, '');                                    // an 'ended' after our own stop says nothing
eq(S('listening', { type: 'stop' }).state, 'idle');
eq(S('listening', { type: 'stop', msg: 'Stopped: x' }).msg, 'Stopped: x');
r = S('listening', { type: 'fail', msg: 'boom' });
eq([r.state, r.msg, r.error], ['idle', 'boom', true]);
eq(S('idle', { type: 'bogus' }).state, 'idle');

// every worklet / capture piece the page uses is wired: the endpoint, the 44.1 kHz rate and the sink call
for (const needle of ["'/api/tab_audio?rate='", 'setSinkId(jankId)', 'getDisplayMedia(tabCaptureOptions())', "registerProcessor('tab-tap'"]) {
  assert.ok(html.includes(needle.replace(/^'|'$/g, '')), needle); checks++;
}
// the tab bar: the remembered pane, and what a switch does
eq([P.panePick('tab'), P.panePick('player'), P.panePick(null), P.panePick('x')], ['tab', 'player', 'player', 'player']);
eq(P.paneSwitchPlan('player', 'tab', 'idle'), { pane: 'tab', pausePlayer: true, stopTab: false });
eq(P.paneSwitchPlan('tab', 'tab', 'listening'), { pane: 'tab', pausePlayer: false, stopTab: false });      // the same tab again: nothing
eq(P.paneSwitchPlan('tab', 'player', 'listening'), { pane: 'player', pausePlayer: false, stopTab: true });
eq(P.paneSwitchPlan('tab', 'player', 'idle'), { pane: 'player', pausePlayer: false, stopTab: false });
eq(P.paneSwitchPlan('player', 'player', 'idle').pausePlayer, false);
for (const needle of ['data-pane="player"', 'data-pane="tab"', 'id="panebtn-tab"', "localStorage.setItem(PANE_KEY"]) { assert.ok(html.includes(needle), needle); checks++; }
// the removed cards (confidence + threshold, the BPM / mode row, the Link line) and their ids are gone; the layout wraps the XIAO card beside the device
for (const id of ['confval', 'confbar', 'confmark', 'linkstate', 'thval', 'exp', 'diff']) { assert.ok(!html.includes('id="' + id + '"') && !html.includes("$('" + id + "')"), id); checks++; }
assert.ok(html.includes('id="devwrap"') && html.includes('id="leftcol"') && html.includes('#list { flex:1 1 0; min-height:260px; overflow:auto;')); checks++;
// real tabs to the right of the device, one box for the track + pitch + volume, no accuracy table, no XIAO description
for (const id of ['accrows', 'acc', 'panebar']) { assert.ok(!html.includes('id="' + id + '"') && !html.includes("$('" + id + "')"), id); checks++; }
for (const needle of ['class="tabs" role="tablist"', 'id="rightcol"', 'class="card tabpanel"', 'id="calib"', 'id="calibmsg"', 'id="autolat"']) { assert.ok(html.includes(needle), needle); checks++; }
assert.ok(!html.includes("The XIAO's own tracker on the music")); checks++;
assert.ok(!html.includes('<h1>Beat Player</h1>')); checks++;
const devAt = html.indexOf('<div class="card device">'), right = html.indexOf('id="rightcol"'), xiao = html.indexOf('id="xiaocard"'), wrapEnd = html.indexOf('</main>');
assert.ok(devAt < xiao && xiao < right && right < wrapEnd); checks++;                       // the XIAO card under the device (left column), then the tabs column
const box = html.slice(html.indexOf('<div class="ctl">'), html.indexOf('</main>'));
for (const id of ['play', 'auto', 'time', 'seek', 'pitchval', 'pitch', 'vol']) { assert.ok(box.includes('id="' + id + '"'), id); checks++; }   // one box
assert.strictEqual((html.match(/class="now"/g) || []).length, 1); checks++;
assert.ok(!html.includes('what you hear')); checks++;
assert.ok(!html.includes('Pick a Chrome tab (YouTube')); checks++;
console.log(checks + ' checks passed');
