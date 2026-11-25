// SPDX-License-Identifier: MIT
// The Install page's flavours (ui/flavour.js, `install.html?flavour=launchpad`): the parameter's parsing, the release list
// unchanged without one, the releases a flavour offers, `set usb host` against a simulated unit (the 25 s of talk before it, the
// read back), and the page's flavour parts hidden unless asked for. Run by tests/run.sh.
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Flavour = require(path.join(__dirname, '..', 'ui', 'flavour.js'));
const root = path.join(__dirname, '..');
let n = 0;
const ok = (c, m) => { assert.ok(c, m); n++; };
const eq = (a, b, m) => { assert.deepStrictEqual(a, b, m); n++; };

// ---- the parameter
eq(Flavour.parse(''), null, 'no query: no flavour');
eq(Flavour.parse(undefined), null, 'no search at all: no flavour');
eq(Flavour.parse('?flavour=launchpad'), 'launchpad', 'flavour=launchpad');
eq(Flavour.parse('?tag=v1&flavour=launchpad'), 'launchpad', 'among other parameters');
eq(Flavour.parse('?flavour=LaunchPad%20'), 'launchpad', 'case and spaces do not matter');
eq(Flavour.parse('?flavour='), null, 'empty: no flavour');
eq(Flavour.parse('?flavour=drum'), null, 'a flavour the page does not know: the plain page');
eq(Flavour.parse('?flavor=launchpad'), null, 'only "flavour" is read');
eq(Flavour.parse('?flavour=%E0%A4%A'), null, 'a broken escape: the plain page');
eq(Flavour.parse('?flavour=constructor'), null, 'no inherited names');

// ---- releases
ok(Flavour.newer('v0.0.2', 'v0.0.1') && Flavour.newer('v0.1.0', 'v0.0.9') && Flavour.newer('v0.0.10', 'v0.0.9') && Flavour.newer('v1.0.0', 'v0.9.9'), 'newer, by number');
ok(!Flavour.newer('v0.0.1', 'v0.0.1') && !Flavour.newer('v0.0.0', 'v0.0.1'), 'not newer: the same or older');
ok(!Flavour.newer('v1.0', 'v0.0.1') && !Flavour.newer('mini2.2-signoff3', 'v0.0.1') && !Flavour.newer('', 'v0.0.1'), 'a tag of another shape is never newer');
const part = { parts: [{ path: 'firmware/x/jank.bin', offset: 65536 }], beatw: { offset: 0, size: 1 } };
const releases = [
  { tag: 'v0.0.3', boards: { auto: part } },
  { tag: 'v0.0.2', boards: { xiao: part } },
  { tag: 'v0.0.1', boards: { auto: part } },
  { tag: 'test-build', boards: { auto: part } },
  { tag: 'v0.0.0' },
];
eq(Flavour.releasesFor(null, releases, 'auto'), releases.filter((r) => r.boards && r.boards.auto), 'no flavour: the list as the page always made it');
eq(Flavour.releasesFor(null, releases, 'auto').map((r) => r.tag), ['v0.0.3', 'v0.0.1', 'test-build'], 'no flavour: v0.0.1 is listed');
eq(Flavour.releasesFor('launchpad', releases, 'auto').map((r) => r.tag), ['v0.0.3'], 'launchpad: only releases after v0.0.1, for the board');
eq(Flavour.releasesFor('launchpad', releases.slice(2), 'auto'), [], 'launchpad: none when only v0.0.1 is there');
ok(/Launchpad/.test(Flavour.noneText('launchpad')) && Flavour.noneText(null) === null, 'what the page says when none fits');

// ---- set usb host, against a unit whose uptime runs with the fake clock
function unit({ uptime = 3, answers = true, refuse = false, reads = 'host', noUptime = false } = {}) {
  const u = { up: uptime, usb: 'device', log: [], setAt: null, notes: [] };
  u.ask = async (cmd) => {
    u.log.push(cmd);
    if (!answers) return null;
    if (cmd === 'status') return noUptime ? { usb: '1' } : { usb: '1', uptime: String(Math.floor(u.up)) };
    if (cmd === 'set usb host') { if (refuse) return null; u.setAt = u.up; u.usb = reads; return {}; }
    if (cmd === 'get') return { usb: u.usb, version: '0.0.2' };
    return null;
  };
  u.wait = async (ms) => { u.up += ms / 1000; };
  u.note = (t) => u.notes.push(t);
  return u;
}
async function main() {
  {
    const u = unit({ uptime: 3 });
    ok(await Flavour.setUsbHost(u.ask, u.wait, u.note), 'fresh start: set, and read back as host');
    ok(u.setAt >= Flavour.CONFIRM_S, `the setting goes only after ${Flavour.CONFIRM_S} s of uptime (at ${u.setAt} s)`);
    ok(u.log.indexOf('set usb host') > 5 && u.log.slice(0, u.log.indexOf('set usb host')).every((c) => c === 'status'), 'talk (status) until then');
    eq(u.log.filter((c) => c === 'set usb host').length, 1, 'sent once');
    eq(u.log[u.log.length - 1], 'get', 'read back last');
    ok(u.notes.some((t) => /Keep it plugged in/.test(t)), 'the page says to keep it plugged in');
  }
  {
    const u = unit({ uptime: 300 });
    ok(await Flavour.setUsbHost(u.ask, u.wait, u.note), 'running long already: set at once');
    eq(u.log.slice(0, 2), ['status', 'set usb host'], 'no waiting when the unit has run long enough');
  }
  {
    const u = unit({ uptime: 0, noUptime: true });
    await Flavour.setUsbHost(u.ask, u.wait, u.note);
    ok(u.setAt >= Flavour.CONFIRM_S, 'no uptime in the answer: the whole 25 s of talk');
  }
  {
    const u = unit({ reads: 'device' });
    ok(!(await Flavour.setUsbHost(u.ask, u.wait, u.note)), 'ok, but get does not say host: false');
  }
  for (const [what, opts] of [['no answer', { answers: false }], ['refused', { refuse: true }]]) {
    const u = unit(opts);
    let threw = false;
    try { await Flavour.setUsbHost(u.ask, u.wait, u.note); } catch (e) { threw = true; }
    ok(threw, `${what}: an error, not a silent success`);
  }

  // ---- the page
  const src = fs.readFileSync(path.join(root, 'page', 'src', 'install.html'), 'utf8');
  const html = fs.readFileSync(path.join(root, 'page', 'install.html'), 'utf8');
  ok(src.includes('<!--@include ui/flavour.js-->') && html.includes('const Flavour = ('), 'the page carries ui/flavour.js');
  ok(/const flavour = Flavour\.parse\(location\.search\);/.test(html), 'the flavour comes from the address only');
  ok(/const list = Flavour\.releasesFor\(flavour, releases, BOARD\);/.test(html), 'the release list goes through releasesFor (unchanged without a flavour)');
  for (const name of Object.keys(Flavour.FLAVOURS)) {
    for (const id of ['flavour-' + name, 'next-' + name]) {
      const tag = html.match(new RegExp('<section id="' + id + '"[^>]*>'));
      ok(tag && /\bhidden\b/.test(tag[0]), `#${id}: in the page, hidden unless the flavour is asked for`);
    }
  }
  const next = html.match(/<section id="next-launchpad"[\s\S]*?<\/section>/)[0];
  ok(/id="fl-host"[^>]*disabled/.test(next) && /set usb host/.test(next), 'step 1: the Set USB host button, disabled until the page enables it');
  ok(/powered USB hub/.test(next) && /Launchpad X/.test(next) && /Digitone II/.test(next), 'step 2: the hub, the Launchpad and the Digitone');
  ok(/live control/.test(next) && /lp7/.test(next) && /stage 8/.test(next) && /REMOTE CONTROL/.test(next), 'step 3: what the Digitone needs');
  ok(/href="https:\/\/w0ts\.github\.io\/dn2-wishes\/controller\.html/.test(html), 'a link to the Wishes site\'s Controller page');
  ok(/<title>Jank · Install<\/title>/.test(html), 'the title stays the Install page\'s');
  console.log(`flavour: ${n} checks passed`);
}
main().catch((e) => { console.error(e); process.exit(1); });
