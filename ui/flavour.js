// SPDX-License-Identifier: MIT
// The Install page's flavours: `install.html?flavour=launchpad` installs the SAME firmware (no other build) and adds a heading,
// its own text and the next steps for one use: jank as the USB host of a Launchpad X and a Digitone II (docs/launchpad.md).
// Without the parameter (or with one this page does not know) the page is exactly the plain Install page.
// Also the one control line the flavour sends, `set usb host` (stored; docs/usb.md "Host mode"), as scripts/ctl.py sends it, on
// the same control line and framing the page already uses for updates (ui/midiota.js). No DOM, no Web MIDI here: node tests it
// (tests/test_flavour.js).
const Flavour = (() => {
  'use strict';
  // after: only releases newer than this tag carry what the flavour needs (v0.0.1 has no Launchpad support: firmware/main/usb_lp.c
  // came later); none: what the page says when no listed release has it
  const FLAVOURS = {
    launchpad: { label: 'Launchpad X controller for the Digitone II', after: 'v0.0.1', none: 'No release with Launchpad support yet: v0.0.1 does not have it. It comes with the next release.' },
  };
  // a new image confirms itself once it has run 20 s and a computer has spoken on the control line (firmware/main/ota.c
  // valid_task); a start as host does not confirm it. 25 s, as the page's update does, before the role changes
  const CONFIRM_S = 25, STEP_MS = 2500;

  /** the flavours for the page's picker (owner 2026-11-25: "can the installer pick them ?"), in order: [{ name, label }] */
  function list() { return Object.keys(FLAVOURS).map((name) => ({ name, label: FLAVOURS[name].label })); }

  /** the query for a choice of the picker: "?flavour=NAME" for a flavour, "" for the plain page (an unknown name too) */
  function search(name) { return Object.prototype.hasOwnProperty.call(FLAVOURS, name) ? '?flavour=' + encodeURIComponent(name) : ''; }

  /** the flavour a page address's query asks for (location.search), or null */
  function parse(search) {
    let v = null;
    try { v = new URLSearchParams(search || '').get('flavour'); } catch (e) { return null; }
    if (v === null) return null;
    v = v.trim().toLowerCase();
    return Object.prototype.hasOwnProperty.call(FLAVOURS, v) ? v : null;
  }

  /** vX.Y.Z -> [X, Y, Z], or null for a tag of another shape */
  function version(tag) {
    const m = /^v(\d+)\.(\d+)\.(\d+)$/.exec(String(tag || ''));
    return m ? [+m[1], +m[2], +m[3]] : null;
  }

  /** a newer than b (both vX.Y.Z; a tag of another shape is never newer) */
  function newer(a, b) {
    const x = version(a), y = version(b);
    if (!x || !y) return false;
    for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] > y[i];
    return false;
  }

  /** the releases the page lists: those with firmware for the board; with a flavour, only the ones that carry what it needs */
  function releasesFor(name, releases, board) {
    const f = name ? FLAVOURS[name] : null;
    return releases.filter((r) => r.boards && r.boards[board] && (!f || !f.after || newer(r.tag, f.after)));
  }

  /** what the page says when a flavour leaves no release (null without a flavour: the page's own words) */
  function noneText(name) { return name ? FLAVOURS[name].none : null; }

  /** `set usb host` on a jank plugged into the computer. ask(command) -> { key: value } up to its "ok", or null (no answer or an
   *  "err" line); wait(ms); note(text) for the page. First the control line is kept busy until the unit has run CONFIRM_S, so a
   *  new image is confirmed before it can start as host; then the setting, a little more talk, and `get` to read it back.
   *  true: `get` says usb=host. */
  async function setUsbHost(ask, wait, note) {
    const st = await ask('status');
    if (!st) throw new Error('the Jank did not answer on its control line');
    const up = Number(st.uptime);
    for (let left = Number.isFinite(up) ? CONFIRM_S - up : CONFIRM_S; left > 0; left -= STEP_MS / 1000) {
      note('Keep it plugged in... ' + Math.ceil(left) + ' s');
      await wait(STEP_MS);
      await ask('status');
    }
    note('Setting USB host...');
    if (!(await ask('set usb host'))) throw new Error('the Jank refused "set usb host"');
    for (let i = 0; i < 2; i++) { await wait(1000); await ask('status'); }
    const g = await ask('get');
    return !!g && g.usb === 'host';
  }

  return { FLAVOURS, CONFIRM_S, list, search, parse, newer, releasesFor, noneText, setUsbHost };
})();
if (typeof module !== 'undefined') module.exports = Flavour;
