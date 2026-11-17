/* Auto latency: the decision logic shared by the Mac simulator (beat/live/player.html) and the control page (page/jank.html). Pure: no DOM, no
   timers, no I/O; flat globals so the beat node tests can pick the functions out of the expanded page. */
/* Auto latency: the player works out the XIAO's beat_latency from the two output paths (live/latency.py: speaker path -
   jank path, in /api/status) and sends `set beat_latency` when --xiao starts, when the XIAO's audio source turns to USB
   (a new stream, or a restart) and whenever the suggestion moves by more than AUTOLAT_MIN_CHANGE_MS (at most one send per
   AUTOLAT_MIN_GAP_MS). Two layers (jank M2h): this is the AUTO base; the user's AUDIO SHIFT (NUDGE MS, Calibrate's
   `set lat_trim`) rides on top on the XIAO and never turns it off. Only the checkbox does ("manual"). */
const AUTOLAT_MIN_CHANGE_MS = 2;
const AUTOLAT_MIN_GAP_MS = 2000;
const TAB_PATH_CONSTANT_MS = 0;        // Tab audio: both sinks are delayed to the same reported latency, so the paths differ by this
function autoLatNew() { return { on: true, manual: false, sent: null, sentAt: -1e9, wasUsb: false }; }
/* ev: tick {sug (ms or null), usb (the XIAO's source is USB), now} | manual | auto | reconnect. -> { st, send (ms or null) }
   opts: { minChangeMs, minGapMs } override the player's 2 ms / 2 s (the control page uses 3 ms / 5 s) */
function autoLatStep(st, ev, opts = {}) {
  switch (ev.type) {
    case 'manual': return { st: { ...st, on: false, manual: true }, send: null };
    case 'auto': return { st: { ...st, on: true, manual: false, sent: null }, send: null };
    case 'reconnect': return { st: { ...st, sent: null, wasUsb: false }, send: null };
    case 'tick': {
      const next = { ...st, wasUsb: !!ev.usb };
      if (!st.on || !ev.usb || !Number.isFinite(ev.sug)) return { st: next, send: null };
      const sug = Math.round(ev.sug);
      const first = st.sent === null || !st.wasUsb;
      const moved = Math.abs(sug - st.sent) > (opts.minChangeMs ?? AUTOLAT_MIN_CHANGE_MS) && ev.now - st.sentAt >= (opts.minGapMs ?? AUTOLAT_MIN_GAP_MS);
      if (!first && !moved) return { st: next, send: null };
      return { st: { ...next, sent: sug, sentAt: ev.now }, send: sug };
    }
    default: return { st, send: null };
  }
}
/* the suggestion for the pane: the player's paths, or (Tab audio, listening through jank) the aligned sinks' constant */
function autoLatSuggestion(pane, tabState, tabToJank, statusSug) {
  if (pane === 'tab') return tabState === 'listening' && tabToJank ? TAB_PATH_CONSTANT_MS : null;
  return Number.isFinite(statusSug) ? statusSug : null;
}
function autoLatLabel(st, sug, usb, sentNow, trim = null) {
  if (st.manual) return 'manual' + (st.sent === null ? '' : ' (auto had sent ' + st.sent + ' ms)');
  if (!st.on) return 'off';
  if (!usb) return 'waiting: the XIAO\'s source is not USB';
  if (!Number.isFinite(sug)) return 'waiting for a suggestion';
  const auto = st.sent === null ? sug : st.sent;
  return (Number.isFinite(trim) ? 'auto ' + auto + ' ms + trim ' + trim + ' ms' : 'auto: ' + auto + ' ms') + (sentNow ? ' (sent)' : '');
}
/* Calibrate (M2h): the AUDIO SHIFT that makes auto + shift the suggested latency, so it composes with Auto latency; null:
   the XIAO's layers not known (an older firmware: the whole value goes to `set beat_latency`) */
function calibTrim(sug, lat) {
  if (sug === null || sug === undefined || !lat) return null;
  return Math.round(sug) - lat.auto;
}

/* ---- the control page's Auto latency (page/src/jank.html, Advanced): the same machine, fed by what the browser knows ---- */
const MIN_BEAT_LATENCY_MS = -300, MAX_BEAT_LATENCY_MS = 300;       // the stick's range (core/jank_core.h; live/latency.py)
const AUTOLAT_PAGE = { minChangeMs: 3, minGapMs: 5000 };
function clampBeatLatency(ms) { return Math.max(MIN_BEAT_LATENCY_MS, Math.min(MAX_BEAT_LATENCY_MS, Math.round(ms))); }
/* beat_latency = the monitor path - the stick path (live/latency.py: the music is heard that long after its samples reach the stick);
   null while either is unknown */
function autoLatPageSuggestion(monitorMs, stickMs) {
  return Number.isFinite(monitorMs) && Number.isFinite(stickMs) ? clampBeatLatency(monitorMs - stickMs) : null;
}
/* the one line under Advanced: what was sent, or why nothing was. s: { on, playing (the stick path plays), monitorMs, stickMs, source ('usb'
   | 'line' | 'none' | null: unknown), current (the stick's auto base, ms or null), sent (ms or null), error, minChangeMs } */
function autoLatPageLabel(s) {
  if (!s.on) return 'off: the stick keeps its own value';
  if (s.error) return 'not sent: ' + s.error;
  if (s.source !== null && s.source !== 'usb') return 'not sent: the stick\'s audio source is ' + s.source + ', not USB';
  if (!Number.isFinite(s.stickMs) || !s.playing) return 'stick path unknown (it is measured while the stick path plays)';
  if (!Number.isFinite(s.monitorMs)) return 'speakers latency unknown';
  const sug = autoLatPageSuggestion(s.monitorMs, s.stickMs), why = '(speakers ' + s.monitorMs + ' - stick path ' + s.stickMs + ')';
  if (s.sent !== null && s.sent !== undefined) return 'sent ' + s.sent + ' ms ' + why;
  if (Number.isFinite(s.current) && Math.abs(sug - s.current) <= (s.minChangeMs ?? AUTOLAT_PAGE.minChangeMs)) return 'within ' + (s.minChangeMs ?? AUTOLAT_PAGE.minChangeMs) + ' ms of the stick\'s ' + s.current + ' ms: nothing sent ' + why;
  return 'waiting to send ' + sug + ' ms ' + why;
}
