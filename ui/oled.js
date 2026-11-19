/* ====================================================================================================
   OLED UI for jank mini 2.2, screen version. 128 x 64, 1 bit per pixel, 5x7 pixel font (scaled x2/x3 for big text).
   Pure logic: no DOM. Input: press(ui, button, now_ms, ctx) with button 'up' | 'down' | 'no' (left arm) | 'yes'
   (right arm); output: draw(fb, ui, ctx, now_ms) fills a 128*64 Uint8Array (1 = white pixel). Written so it can be
   ported to the firmware as it is: one state struct, one input function, one draw function, a tiny font.

   ctx, read by the UI (the host fills it from /api/status and its own emulated state): link (mode is LINK; the host derives it from the line-in plug detect: plugged = AUDIO, unplugged = LINK), bpm
   (audio mode, after the half/double fold), linkBpm, mode, held (the user's hold: square when not running and not armed, else pause), conf 0..1, th percent, levelDb (dBFS), silent, gated,
   playing (Link transport), running / armed (the transport icon), fold (AUDIO: 1 2 4 0.5 0.25, the label X2 /2 ... top left; --xiao from the status's fold= / the home line's mode), foldAuto (AUTO RANGE set it), xiao (--xiao: YES sends play / stop in both modes), peers, wifi ('good'|'weak'|'none'), beatN (integer beat or null), beatMs (ms since the beat), usb, fitted, batt percent,
   autoBar (the AUTO BAR setting: 0 OFF, 1 AT START, 2 ALWAYS; true / false = 1 / 0), bar (the downbeat estimator: { state, conf } or null),
   offs [MIDI OUT 1, MIDI OUT 2, CLK] in ms, rate (index into the clock divisions), boot, splash.
   net: { mode 0 JANK | 1 LINK CLIENT | 2 LINK AP | 3 SETUP, prev (mode to return to after SETUP), group (the jank GROUP 1..9: the units on the same group form one Link session over open Wi-Fi frames, no router), ssid, pass, appass, apName,
   status, ip, apClients, scan [{name, bars}], setupName }, groupPeers, setup (mode is SETUP), qr { size, mod }, saved.
   ctx actions: tempoStep(+1|-1) -> new BPM shown, setField(name, value), setHold(bool) (stop: hold; start: resync, then release).

   BUTTONS: UP / DOWN / NO (left) / YES (right).
   SCREENS AND TRANSITIONS
     HOME     everything at once: mode tag AUDIO|LINK, state (STEADY FASTER SLOWER STOPPED GATED SILENT PLAYING), BPM large,
              the layouts are described at drawHome.
                UP / DOWN   LINK mode: Link tempo +/-1 BPM (the host auto-repeats while held).
                            AUDIO mode: x2 / /2, the tracker's half/double fix. 1 s overlay with the new BPM
                YES         (on release) Stop / Start of the outputs (MIDI clock, modular clock); the top-left icon shows
                            it. AUDIO: Stop holds Link and the clocks, Start resyncs and runs. LINK: the device's own
                            clocks start / stop (and the session, in FOLLOW+LEAD; see LINK TRANSPORT).        NO (on release) -> MENU
                            With the XIAO (--xiao): `play` (it starts on its next bar line) or `stop`, by the icon, in LINK and
                            in AUDIO (the hold of the outputs). UP / DOWN in AUDIO send `fold x2` / `fold /2`.
                TRANSPORT ICON  triangle = running, square = the user stopped (held), two bars (pause) = started but not running (the machine layer holds the outputs: no bpm yet, gate, silence, LOST), blinking triangle = armed: a start asked for and
                            not reached yet, one that resolves by itself (the next bar line, the tracker locking on
                            music it hears) or by YES; never shown for a session that is merely stopped
                NO + UP/DOWN   AUDIO mode: PHASE, half a beat relative to the tracked phase (1 s overlay "PHASE +1/2", "AUTO"
                               at 0): one step fixes an off-beat lock, two move the bar's 1 by a beat. Two layers (jank
                               M2h): the user's PHASE rides on AUTO BAR's bar, which stays in charge; 0 is AUTO
                BAR MARKS   AUDIO mode, AUTO BAR on: a bar under the first box = LOCKED (the bar's 1 is the music's 1),
                            the line under the boxes = how sure; nothing under the boxes = still listening
                YES + UP/DOWN  AUDIO mode: AUDIO SHIFT, the timing 1 ms (every output; repeats while held; 1 s overlay
                               "AUDIO SHIFT +3 MS", "AUTO" at 0): the user's shift on top of the auto latency (jank M2h).
                               A combo cancels the NO / YES release action.
     MENU     AUDIO, LINK TRANSPORT (LINK mode: IGNORE = the clocks run on Link's tempo and beat, own Start / Stop only;
              FOLLOW = they also start / stop with the session; FOLLOW+LEAD = and our Start / Stop starts / stops the session),
              AUTO BAR (AUDIO mode: OFF = the bar left where it is; AT START = the bar's 1 found in the music and Link's bar moved onto it
              at a start only, never while playing; ALWAYS = also while playing),
              NETWORK, USB, OFFSET, CLK RATE, DISPLAY, BATTERY (UP/DOWN move; the mode is not a setting: plugging in the line = AUDIO)
                YES on AUDIO / OFFSET -> SUB                    YES on NETWORK -> NET
                YES on CLK RATE / AUTO BAR / USB -> EDIT              YES on BATTERY -> INFO       NO -> HOME
     SUB      a list of EDITS: UP/DOWN move, YES -> EDIT, NO -> MENU
                DISPLAY: BRIGHTNESS (10..100 %), SCREEN OFF (NEVER / 30 S / 1 MIN / 5 MIN without a button; the first
                         press on a dark screen only wakes it; SETUP keeps the screen on)
                AUDIO:  LEVEL (-12..+12 dB, attenuator / boost before the tracker), LOW CONFIDENCE (KEEP RUNNING or
                        STOP / START: under the threshold the transport stops), THRESHOLD (%); --xiao: the XIAO's own
                        (`set lowconf stop|run`, `set threshold N`, read back from its status / get)
                OFFSET: AUDIO IN (-100..+100 ms: the input latency, moves every output; the YES + UP/DOWN nudge
                        changes it), MIDI OUT 1, MIDI OUT 2, CLK OUT in ms (-50..+50, per output)
     NET      TYPE (JANK / LINK CLIENT / LINK AP / SETUP, applied on YES) and per mode: JANK: PEERS; CLIENT: NETWORK (-> SCAN),
              PASSKEY (-> TEXT), STATUS (-> NETINFO); AP: NAME and PASSKEY (-> TEXT), STATUS (-> NETINFO). NO -> MENU.
              Choosing SETUP goes to CONFIRM ("STOP AND START SETUP?", YES starts, NO back).
     SCAN     fake scan list with signal bars: UP/DOWN move, YES picks, NO back
     TEXT     character editor (a-z A-Z 0-9 - _ . ! @ #): UP/DOWN cycle the character at the cursor (the host repeats while held),
              YES = next character, YES on the empty slot past the end = confirm, NO = delete the last character,
              NO on an empty text = cancel. Passkeys are masked except at the cursor. Hint: easier: web UI + address
     SETUP    replaces every screen: the device is stopped, the open hotspot jank-setup-XXXX runs; the screen shows its QR code,
              the name and 192.168.4.1. NO asks "LEAVE SETUP?": YES returns to the previous network mode and runs again
     INFO     read-only (battery: %, charging/discharging, estimated volt; ABOUT: name, version, repo, uptime, reset, chip temp, CPU, memory, flash, clock lateness, loop times; UP / DOWN scroll)  NO or YES -> MENU
     EDIT     (choices cycle round, numbers stop at their ends) the value drawn inverted (white box, black text). UP/DOWN change it (applied live), YES keeps it and
              returns to where EDIT was opened (MENU or SUB), NO restores the old value and returns.
   ==================================================================================================== */
const OLED = (() => {
  const OLED_W = 128, OLED_H = 64;                                   // the OLED boards' grid; the lcd2 layout has its own (SIZES)
  let W = OLED_W, H = OLED_H;                                        // the grid of the screen being drawn (set by draw(), per layout)
  let fine = false;                                                  // the lcd2 layout: text and icons at the panel's own 320 x 240 pixels (see text())
  const ROWS = 7;
  const FONT_SRC = {
    '0': '.###.|#...#|#..##|#.#.#|##..#|#...#|.###.', '1': '..#..|.##..|..#..|..#..|..#..|..#..|.###.',
    '2': '.###.|#...#|....#|...#.|..#..|.#...|#####', '3': '.###.|#...#|....#|..##.|....#|#...#|.###.',
    '4': '...#.|..##.|.#.#.|#..#.|#####|...#.|...#.', '5': '#####|#....|####.|....#|....#|#...#|.###.',
    '6': '..##.|.#...|#....|####.|#...#|#...#|.###.', '7': '#####|....#|...#.|..#..|.#...|.#...|.#...',
    '8': '.###.|#...#|#...#|.###.|#...#|#...#|.###.', '9': '.###.|#...#|#...#|.####|....#|...#.|.##..',
    'A': '.###.|#...#|#...#|#####|#...#|#...#|#...#', 'B': '####.|#...#|#...#|####.|#...#|#...#|####.',
    'C': '.###.|#...#|#....|#....|#....|#...#|.###.', 'D': '###..|#..#.|#...#|#...#|#...#|#..#.|###..',
    'E': '#####|#....|#....|####.|#....|#....|#####', 'F': '#####|#....|#....|####.|#....|#....|#....',
    'G': '.###.|#...#|#....|#.###|#...#|#...#|.####', 'H': '#...#|#...#|#...#|#####|#...#|#...#|#...#',
    'I': '.###.|..#..|..#..|..#..|..#..|..#..|.###.', 'J': '..###|...#.|...#.|...#.|...#.|#..#.|.##..',
    'K': '#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#', 'L': '#....|#....|#....|#....|#....|#....|#####',
    'M': '#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#', 'N': '#...#|##..#|#.#.#|#..##|#...#|#...#|#...#',
    'O': '.###.|#...#|#...#|#...#|#...#|#...#|.###.', 'P': '####.|#...#|#...#|####.|#....|#....|#....',
    'Q': '.###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#', 'R': '####.|#...#|#...#|####.|#.#..|#..#.|#...#',
    'S': '.####|#....|#....|.###.|....#|....#|####.', 'T': '#####|..#..|..#..|..#..|..#..|..#..|..#..',
    'U': '#...#|#...#|#...#|#...#|#...#|#...#|.###.', 'V': '#...#|#...#|#...#|#...#|#...#|.#.#.|..#..',
    'W': '#...#|#...#|#...#|#.#.#|#.#.#|##.##|#...#', 'X': '#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#',
    'Y': '#...#|#...#|.#.#.|..#..|..#..|..#..|..#..', 'Z': '#####|....#|...#.|..#..|.#...|#....|#####',
    ',': '.....|.....|.....|.....|.....|.##..|.#...', '.': '.....|.....|.....|.....|.....|.##..|.##..', ':': '.....|.##..|.##..|.....|.##..|.##..|.....',
    '%': '##..#|##..#|...#.|..#..|.#...|#..##|#..##', '+': '.....|..#..|..#..|#####|..#..|..#..|.....',
    '-': '.....|.....|.....|#####|.....|.....|.....', '/': '....#|....#|...#.|..#..|.#...|#....|#....',
    ' ': '.....|.....|.....|.....|.....|.....|.....',
    'a': '.....|.....|.###.|....#|.####|#...#|.####', 'b': '#....|#....|####.|#...#|#...#|#...#|####.',
    'c': '.....|.....|.###.|#....|#....|#...#|.###.', 'd': '....#|....#|.####|#...#|#...#|#...#|.####',
    'e': '.....|.....|.###.|#...#|#####|#....|.###.', 'f': '..##.|.#..#|.#...|###..|.#...|.#...|.#...',
    'g': '.....|.####|#...#|#...#|.####|....#|.###.', 'h': '#....|#....|#.##.|##..#|#...#|#...#|#...#',
    'i': '..#..|.....|.##..|..#..|..#..|..#..|.###.', 'j': '...#.|.....|..##.|...#.|...#.|#..#.|.##..',
    'k': '#....|#....|#..#.|#.#..|##...|#.#..|#..#.', 'l': '.##..|..#..|..#..|..#..|..#..|..#..|.###.',
    'm': '.....|.....|##.#.|#.#.#|#.#.#|#...#|#...#', 'n': '.....|.....|#.##.|##..#|#...#|#...#|#...#',
    'o': '.....|.....|.###.|#...#|#...#|#...#|.###.', 'p': '.....|####.|#...#|#...#|####.|#....|#....',
    'q': '.....|.####|#...#|#...#|.####|....#|....#', 'r': '.....|.....|#.##.|##..#|#....|#....|#....',
    's': '.....|.....|.####|#....|.###.|....#|####.', 't': '.#...|.#...|###..|.#...|.#...|.#..#|..##.',
    'u': '.....|.....|#...#|#...#|#...#|#..##|.##.#', 'v': '.....|.....|#...#|#...#|#...#|.#.#.|..#..',
    'w': '.....|.....|#...#|#...#|#.#.#|#.#.#|.#.#.', 'x': '.....|.....|#...#|.#.#.|..#..|.#.#.|#...#',
    'y': '.....|#...#|#...#|.####|....#|....#|.###.', 'z': '.....|.....|#####|...#.|..#..|.#...|#####',
    '_': '.....|.....|.....|.....|.....|.....|#####', '!': '..#..|..#..|..#..|..#..|..#..|.....|..#..',
    '@': '.###.|#...#|#.###|#.#.#|#.###|#....|.###.', '#': '.#.#.|#####|.#.#.|.#.#.|.#.#.|#####|.#.#.',
    '*': '.....|#.#.#|.###.|#####|.###.|#.#.#|.....'
  };
  const FONT = {};
  for (const k of Object.keys(FONT_SRC)) FONT[k] = FONT_SRC[k].split('|');
  const NARROW = { '.': 1, ':': 1, ',': 1 };                 // glyphs drawn from column 1, two columns wide
  const adv = ch => (fine ? 2 : 1) * (NARROW[ch] ? 3 : ch === ' ' ? 4 : 6);
  // the lcd2 layout's small text: the 5 x 7 face doubled with Scale2x (10 x 14, 2 px strokes), a fixed 12 px advance, the glyph 1 px into its cell
  const FINE = {};
  const fineGlyph = ch => FINE[ch] || (FINE[ch] = BigFont.scale2x(FONT[ch] || FONT[' ']));
  const rows2x = (() => { const m = new Map(); return rows => { if (!m.has(rows)) m.set(rows, BigFont.scale2x(rows)); return m.get(rows); }; })();

  const px = (fb, x, y, v) => { if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] = v; };
  const rect = (fb, x, y, w, h, v) => { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) px(fb, x + i, y + j, v); };
  const frame = (fb, x, y, w, h, v) => { rect(fb, x, y, w, 1, v); rect(fb, x, y + h - 1, w, 1, v); rect(fb, x, y, 1, h, v); rect(fb, x + w - 1, y, 1, h, v); };
  const xorCol = (fb, x, y0, y1) => { for (let y = y0; y <= y1; y++) if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] ^= 1; };
  const textW = (s, sc) => { let w = 0; for (const ch of s) w += adv(ch) * sc; return Math.max(0, w - sc); };
  function text(fb, s, x, y, sc, v) {
    let cx = x;
    for (const ch of String(s)) {
      if (fine) {
        const g = fineGlyph(ch), off = 2 * (NARROW[ch] || 0);
        for (let r = 0; r < 14; r++) for (let c = 0; c < 10; c++) if (g[r][c] === '#') rect(fb, cx + (1 + c - off) * sc, y + r * sc, sc, sc, v);
        cx += adv(ch) * sc;
        continue;
      }
      const g = FONT[ch] || FONT[' '], off = NARROW[ch] || 0;
      for (let r = 0; r < ROWS; r++) for (let c = 0; c < 5; c++) if (g[r][c] === '#') rect(fb, cx + (c - off) * sc, y + r * sc, sc, sc, v);
      cx += adv(ch) * sc;
    }
  }
  const textR = (fb, s, xr, y, sc, v) => text(fb, s, xr - textW(String(s), sc), y, sc, v);
  const textC = (fb, s, y, sc, v) => text(fb, s, Math.round((W - textW(String(s), sc)) / 2), y, sc, v);

  // ---- the INK plane (the colour variant, ui/lcd2.js): draw() can fill a second plane, one byte per pixel, naming WHICH lit pixels
  // carry information. Ids are meanings, never colours; the screen's renderer maps them. 0 = the neutral default. Only with
  // ctx.layout === 'lcd2' (which also draws a layout of its own, on its own grid: see LCD2 LAYOUT below); the OLED boards pass no
  // plane, draw no ink and keep their bytes exactly. tag() marks the pixels that are LIT right now inside a box, so it is called
  // after the element is drawn.
  const INK = { NONE: 0, GOOD: 1, WARN: 2, BAD: 3, DIM: 4, ONE: 5 };       // ONE: the bar's first beat while it is lit (the LED language's bar 1)
  let inkFb = null, inkOut = null;
  function tag(id, x, y, w, h) {
    if (!inkOut || id === INK.NONE) return;
    for (let j = Math.max(0, y); j < Math.min(H, y + h); j++) for (let i = Math.max(0, x); i < Math.min(W, x + w); i++) if (inkFb[j * W + i] === 1) inkOut[j * W + i] = id;
  }
  const untag = (x, y, w, h) => { if (inkOut) for (let j = Math.max(0, y); j < Math.min(H, y + h); j++) for (let i = Math.max(0, x); i < Math.min(W, x + w); i++) inkOut[j * W + i] = 0; };

  const RATES = ['1/4', '1/8', '1/16', '1/32', '24 PPQN'];          // the modular CLK output's division
  const RATES_SHORT = ['1/4', '1/8', '1/16', '1/32', '24P'];
  const DEFAULT_RATE = 2;
  const NET_MODES = ['JANK', 'LINK CLIENT', 'LINK AP', 'SETUP'], NET_SHORT = ['JANK', 'CLIENT', 'AP', 'SETUP'];
  const SETUP = 3;
  const CHARS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.!@#';
  const MAX_TEXT = 63;
  // what EDIT can change: opts = a list of choices (value = index), else a number lo..hi in steps
  // help: two short lines (21 characters at most) under the value; for a list of choices, one pair per choice
  const LATER = '+ LATER  - EARLIER';
  const SLEEP_NAMES = ['NEVER', '30 S', '1 MIN', '5 MIN'];
  const LT_MODES = ['IGNORE', 'FOLLOW', 'FOLLOW+LEAD'];                // index = dev.ltMode; default FOLLOW+LEAD (2)
  const LT_SHORT = ['IGNORE', 'FOLLOW', 'BOTH'];
  // the HOME top bar's LINK-mode transport icons, 1-bit rows ('#' = lit): IGNORE = a crossed circle (our own start only),
  // FOLLOW = an arrow coming in (the session's start / stop), FOLLOW+LEAD = a double arrow (in and out)
  const LT_ICONS = [
    ['..###..', '.#...##', '#...#.#', '#..#..#', '#.#...#', '##...#.', '..###..'],
    ['...#.....', '..##.....', '.#######.', '########.', '.#######.', '..##.....', '...#.....'],
    ['.........', '..#...#..', '.##...##.', '#########', '.##...##.', '..#...#..', '.........'],
  ];
  const NET_ICON_XR = 86;                                            // the network icon's right edge: a gap before the plug
  const PLUG_X = 92, BATT_X = 107;
  const AUTOBAR_NAMES = ['OFF', 'AT START', 'ALWAYS'], AUTOBAR_SHORT = ['OFF', 'START', 'ALWAYS'];   // index = dev.autoBar (the old true / false = 1 / 0)
  const autoBarIndex = v => (v === true ? 1 : v === false || v == null ? 0 : v);
  // the fold's label on HOME (AUDIO): x2 x4 /2 /4, nothing at 1
  const foldLabel = k => (k === 2 ? 'X2' : k === 4 ? 'X4' : k === 0.5 ? '/2' : k === 0.25 ? '/4' : '');
  const USB_ICON = ['.....####..', '.....######', '#########..', '.....######', '.....####..'];
  function icon(fb, rows, x, y) { rows.forEach((r, j) => { for (let i = 0; i < r.length; i++) if (r[i] === '#') px(fb, x + i, y + j, 1); }); }                       // the menu row (LINK TRANSPORT + value fits 21 characters)
  const EDITS = {
    netmode: { title: 'NETWORK', opts: NET_MODES, get: c => c.net.mode, live: false,      // applied on YES, SETUP asks first
               help: [['JANK GROUP NETWORK', ''], ['JOIN A WIFI ROUTER', ''], ['OWN WIFI NETWORK', ''], ['STOPS THE DEVICE', 'OPENS SETUP HOTSPOT']] },
    inoff: { title: 'AUDIO IN', lo: -100, hi: 100, step: 1, unit: ' MS', get: c => c.inOff,     // input latency: moves every output
             help: ['INPUT LATENCY, MOVES', 'ALL OUTPUTS. + LATER'] },
    out0: { title: 'MIDI OUT 1', lo: -50, hi: 50, step: 1, unit: ' MS', get: c => c.offs[0], help: ['MIDI OUT 1 TIMING', LATER] },
    out1: { title: 'MIDI OUT 2', lo: -50, hi: 50, step: 1, unit: ' MS', get: c => c.offs[1], help: ['MIDI OUT 2 TIMING', LATER] },
    out2: { title: 'CLK OUT', lo: -50, hi: 50, step: 1, unit: ' MS', get: c => c.offs[2], help: ['MODULAR CLOCK TIMING', LATER] },
    rate: { title: 'CLOCK RATE', opts: RATES, get: c => c.rate,
            help: [['1 PULSE PER BEAT', 'ON CLK OUT'], ['2 PULSES PER BEAT', 'ON CLK OUT'], ['4 PULSES PER BEAT', 'ON CLK OUT'],
                   ['8 PULSES PER BEAT', 'ON CLK OUT'], ['24 PULSES PER BEAT', 'ON CLK OUT']] },
    threshold: { title: 'THRESHOLD', lo: 0, hi: 100, step: 5, unit: '%', get: c => c.th, help: ['BELOW IT: STOP', '(LOW CONFIDENCE)'] },
    gain: { title: 'LEVEL', lo: -12, hi: 12, step: 1, unit: ' dB', get: c => c.gain, help: ['INPUT LEVEL', '- CUT   + BOOST'] },   // before the tracker
    ltmode: { title: 'LINK TRANSPORT', opts: LT_MODES, get: c => c.ltMode,      // LINK mode only: the session's start / stop
              help: [['CLOCKS RUN ON LINK', 'TEMPO, OWN START'], ['CLOCKS START + STOP', 'WITH THE SESSION'],
                     ['FOLLOWS, AND START/', 'STOP DRIVES LINK']] },
    bright: { title: 'BRIGHTNESS', lo: 10, hi: 100, step: 10, unit: '%', get: c => c.bright, help: ['SCREEN BRIGHTNESS', 'LOWER SAVES BATTERY'] },
    sleep: { title: 'SCREEN OFF', opts: SLEEP_NAMES, get: c => c.sleep,
             help: [['SCREEN STAYS ON', ''], ['DARK AFTER 30 S', 'ANY BUTTON WAKES IT'], ['DARK AFTER 1 MIN', 'ANY BUTTON WAKES IT'],
                    ['DARK AFTER 5 MIN', 'ANY BUTTON WAKES IT']] },
    group: { title: 'JANK GROUP', opts: ['1', '2', '3', '4', '5', '6', '7', '8', '9'], get: c => c.net.group - 1, live: false,
             help: Array(9).fill(['UNITS ON THE SAME', 'GROUP PLAY TOGETHER']) },     // the jank README: GROUP 1..9, no router
    lowconf: { title: 'LOW CONFIDENCE', opts: ['KEEP RUNNING', 'STOP / START'], get: c => (c.confStop ? 1 : 0),
               help: [['NEVER STOPS', ''], ['STOPS BELOW THE', 'THRESHOLD']] },
    // what the tracker listens to: AUTO = the line in when plugged, else USB audio when it plays
    // (LINK: USB audio is ignored, a plug in the line in still wins: it is mechanical)
    insrc: { title: 'SOURCE', opts: ['AUTO', 'LINE IN', 'USB AUDIO', 'LINK'], get: c => c.inSrc,
             help: [['LINE IN IF PLUGGED', 'ELSE USB AUDIO'], ['THE LINE IN JACK', 'ONLY'], ['AUDIO OVER USB', 'ONLY'], ['USB AUDIO IGNORED', 'LINE STILL WINS']] },
    // the USB port's role; MIDI and USB audio work in both (host: battery units, the gear has its own power)
    usbrole: { title: 'USB', opts: ['DEVICE', 'HOST'], get: c => c.usbRole,
               help: [['TO A COMPUTER', 'MIDI + AUDIO'], ['TO SELF-POWERED', 'MIDI + AUDIO GEAR']] },
    // the MIDI clock over USB-MIDI (to the computer, or to the gear in HOST), M2d's usbmidi; the MIDI outs always have it
    usbmidi: { title: 'USB MIDI CLOCK OUT', opts: ['ON', 'OFF'], get: c => c.usbMidi,
               help: [['CLOCK ALSO OVER USB', 'TO COMPUTER OR GEAR'], ['CLOCK ON MIDI OUTS', 'ONLY, NOT OVER USB']] },
    // the downbeat estimator (AUDIO mode): ON finds the bar's 1 in the music and moves Link's bar onto it. A NUDGE BEAT
    // makes it MANUAL (an M on HOME) until AUTO BAR is picked again (YES on ON, even unchanged) or the tempo starts over
    // OFF: no placement. AT START (default): the 1 is placed while the session is stopped or its start is not reached, or
    // once at a new start for the bar; never mid-play. ALWAYS: also while playing (moves can come at any time)
    autobar: { title: 'AUTO BAR', opts: AUTOBAR_NAMES, get: c => autoBarIndex(c.autoBar), live: false, repick: true,
               help: [['THE BAR 1 STAYS', 'WHERE IT IS'], ['1 PLACED AT A START', 'NEVER WHILE PLAYING'], ['MOVES THE 1 ALSO', 'WHILE PLAYING']] },
  };
  const helpOf = (sp, v) => (sp.opts ? sp.help[v] : sp.help) || ['', ''];
  const MENU = [                                                    // the top-level menu, in order
    { name: 'AUDIO', sub: 'audio' }, { name: 'LINK TRANSPORT', edit: 'ltmode' }, { name: 'AUTO BAR', edit: 'autobar' }, { name: 'NETWORK', net: true }, { name: 'USB', edit: 'usbrole' }, { name: 'USB MIDI CLOCK OUT', edit: 'usbmidi' }, { name: 'OFFSET', sub: 'offsets' },
    { name: 'CLK RATE', edit: 'rate' }, { name: 'DISPLAY', sub: 'display' }, { name: 'BATTERY', info: 'battery' },
    { name: 'ABOUT', info: 'about' },
  ];
  // sub-menus: a title and the EDITS they list
  const SUBS = { audio: { title: 'AUDIO', rows: ['insrc', 'gain', 'lowconf', 'threshold'] }, offsets: { title: 'OFFSET', rows: ['inoff', 'out0', 'out1', 'out2'] },
                 display: { title: 'DISPLAY', rows: ['bright', 'sleep'] } };
  const VISIBLE = 6;
  const OVERLAY_MS = 1000;
  const METER_MIN_DB = -90, SILENCE_DB = -60;
  const sgn = v => (v > 0 ? '+' : '') + v;
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

  const create = () => ({ screen: 'home', cursor: 0, top: 0, ocur: 0, ascroll: 0, ncur: 0, scur: 0, stop: 0, edit: null, txt: null, leave: false, overlay: null });
  const editText = (spec, v) => (spec.opts ? spec.opts[v] : (spec.lo < 0 ? sgn(v) : v) + spec.unit);

  function startEdit(ui, field, c) {
    const orig = EDITS[field].get(c);
    ui.edit = { field, orig, val: orig, ret: ui.screen };
    ui.screen = 'edit';
  }
  function stepEdit(ui, c, d) {
    const e = ui.edit, sp = EDITS[e.field];
    const n = sp.opts ? sp.opts.length : 0;                       // choices cycle round; numbers stop at their ends
    e.val = sp.opts ? (e.val + d + n) % n : clamp(e.val + sp.step * d, sp.lo, sp.hi);
    if (sp.live !== false) c.setField(e.field, e.val);              // live
  }
  function endEdit(ui, c, keep) {
    const e = ui.edit, sp = EDITS[e.field];
    ui.edit = null; ui.screen = e.ret;
    if (sp.live === false) {                                         // applied only when confirmed (repick: also unchanged)
      if (keep && (e.val !== e.orig || sp.repick)) {
        if (e.field === 'netmode' && e.val === SETUP) ui.screen = 'confirm';      // stopping the device needs a second YES
        else c.setField(e.field, e.val);
      }
    } else if (!keep) c.setField(e.field, e.orig);
  }
  // NETWORK screen rows for the current mode: { name, val, act } (act = what YES does, null = read-only)
  function netRows(c) {
    const n = c.net, rows = [{ name: 'TYPE', val: NET_SHORT[n.mode], act: 'mode', help: ['HOW THE DEVICE JOINS', 'A NETWORK'] }];
    if (n.mode === 0) rows.push({ name: 'GROUP', val: String(n.group), act: 'group', help: ['UNITS ON THE SAME', 'GROUP PLAY TOGETHER'] },
                                { name: 'PEERS', val: String(c.groupPeers), act: null, help: ['JANK DEVICES IN', 'THE GROUP'] });
    else if (n.mode === 1) rows.push({ name: 'NETWORK', val: n.ssid || '-', act: 'ssid', help: ['THE ROUTER TO JOIN', ''] },
                                     { name: 'PASSKEY', val: '*'.repeat(Math.min(n.pass.length, 6)), act: 'pass', help: ['ITS WIFI PASSKEY', ''] },
                                     { name: 'STATUS', val: n.status, act: 'status', help: ['CONNECTION STATE', 'AND IP ADDRESS'] });
    else rows.push({ name: 'NAME', val: n.apName, act: 'apname', help: ['NAME OF THE', 'DEVICE NETWORK'] },
                   { name: 'PASSKEY', val: '*'.repeat(Math.min(n.appass.length, 6)), act: 'appass', help: ['PASSKEY OF THE', 'DEVICE NETWORK'] },
                   { name: 'STATUS', val: n.apClients + ' CLIENTS', act: 'status', help: ['ADDRESS AND', 'CONNECTED CLIENTS'] });
    return rows;
  }
  function startText(ui, c, field) {
    const text = { pass: c.net.pass, appass: c.net.appass, apname: c.net.apName }[field];
    ui.txt = { field, text, pos: 0, mask: field === 'pass' || field === 'appass' };
    ui.screen = 'text';
  }
  // character editor: UP/DOWN cycle the character at the cursor, YES next (on the empty slot past the end: confirm),
  // NO deletes the last character (on an empty text: cancel)
  function pressText(ui, c, btn, d) {
    const t = ui.txt, n = t.text.length;
    if (d) {
      if (t.pos >= n) { if (n < MAX_TEXT) t.text += CHARS[d > 0 ? 0 : CHARS.length - 1]; }
      else {
        const i = CHARS.indexOf(t.text[t.pos]), j = i < 0 ? 0 : (i + d + CHARS.length) % CHARS.length;
        t.text = t.text.slice(0, t.pos) + CHARS[j] + t.text.slice(t.pos + 1);
      }
    } else if (btn === 'yes') {
      if (t.pos >= n) { c.setField(t.field, t.text); ui.txt = null; ui.screen = 'net'; }
      else t.pos++;
    } else if (n === 0) { ui.txt = null; ui.screen = 'net'; }
    else { t.text = t.text.slice(0, -1); t.pos = Math.min(t.pos, t.text.length); }
  }

  function press(ui, btn, now, c) {
    const d = btn === 'up' ? 1 : btn === 'down' ? -1 : 0;
    if (c.setup) {                                                   // SETUP replaces every other screen
      if (ui.leave) { if (btn === 'yes') c.setField('netmode', c.net.prev); if (!d) ui.leave = false; }
      else if (btn === 'no') ui.leave = true;
      return;
    }
    if (ui.screen === 'home') {
      if (d) {
        const v = c.tempoStep(d);                                  // link: +/-1 BPM; audio: x2 or /2 (--xiao: `fold x2` / `fold /2`)
        if (!c.link) ui.overlay = { until: now + OVERLAY_MS, title: d > 0 ? 'DOUBLE' : 'HALF', value: v == null ? '---' : v.toFixed(1) };   // a Link step shows on the big tempo itself
      } else if (btn === 'yes') {                                   // Stop / Start the outputs; the transport icon shows it
        if (c.link || c.xiao) c.linkStartStop(!(c.running || c.armed));     // LINK: the clocks, and the session in FOLLOW+LEAD; --xiao: play / stop
        else c.setHold(c.running || c.armed);                        // AUDIO: follows the icon: running/armed -> Stop (hold), stopped -> Start
      }
      else { ui.screen = 'menu'; ui.overlay = null; }
    } else if (ui.screen === 'menu') {
      if (d) {
        ui.cursor = clamp(ui.cursor - d, 0, MENU.length - 1);
        ui.top = clamp(ui.top, ui.cursor - VISIBLE + 1, ui.cursor);
      } else if (btn === 'no') ui.screen = 'home';
      else {
        const it = MENU[ui.cursor];
        if (it.net) { ui.screen = 'net'; ui.ncur = 0; }
        else if (it.edit) startEdit(ui, it.edit, c);
        else if (it.sub) { ui.screen = 'sub'; ui.sub = it.sub; ui.ocur = 0; }
        else { ui.screen = 'info'; ui.page = it.info; ui.ascroll = 0; }
      }
    } else if (ui.screen === 'sub') {
      const rows = SUBS[ui.sub].rows;
      if (d) ui.ocur = clamp(ui.ocur - d, 0, rows.length - 1);
      else if (btn === 'no') ui.screen = 'menu';
      else startEdit(ui, rows[ui.ocur], c);
    } else if (ui.screen === 'net') {
      const rows = netRows(c);
      if (d) ui.ncur = clamp(ui.ncur - d, 0, rows.length - 1);
      else if (btn === 'no') ui.screen = 'menu';
      else {
        const act = rows[ui.ncur].act;
        if (act === 'mode') startEdit(ui, 'netmode', c);
        else if (act === 'ssid') { ui.screen = 'scan'; ui.scur = 0; ui.stop = 0; }
        else if (act === 'group') startEdit(ui, 'group', c);
        else if (act === 'pass' || act === 'appass' || act === 'apname') startText(ui, c, act);
        else if (act === 'status') ui.screen = 'netinfo';
      }
    } else if (ui.screen === 'scan') {
      const list = c.net.scan;
      if (d) { ui.scur = clamp(ui.scur - d, 0, Math.max(0, list.length - 1)); ui.stop = clamp(ui.stop, ui.scur - 4, ui.scur); }
      else if (btn === 'yes' && list.length) { c.setField('ssid', list[ui.scur].name); ui.screen = 'net'; }
      else if (btn === 'no') ui.screen = 'net';
    } else if (ui.screen === 'text') {
      pressText(ui, c, btn, d);
    } else if (ui.screen === 'netinfo') {
      if (!d) ui.screen = 'net';
    } else if (ui.screen === 'confirm') {
      if (btn === 'yes') { c.setField('netmode', SETUP); ui.screen = 'net'; }
      else if (btn === 'no') ui.screen = 'net';
    } else if (ui.screen === 'info') {
      if (!d) ui.screen = 'menu';
      else if (ui.page === 'about') { const vis = c.layout === 'lcd2' ? ABOUT_VIS_LCD2 : ABOUT_VIS_OLED; ui.ascroll = clamp(ui.ascroll - d * vis, 0, Math.floor((ABOUT_ROWS - 1) / vis) * vis); }      // ABOUT: a press moves one page (starts are multiples of vis, no row on two pages), clamped
    } else if (ui.screen === 'edit') {
      if (d) stepEdit(ui, c, d);
      else endEdit(ui, c, btn === 'yes');
    }
  }

  const titleBar = (fb, title, right) => {
    rect(fb, 0, 0, W, 9, 1); text(fb, title, 2, 1, 1, 0); if (right) textR(fb, right, W - 2, 1, 1, 0);
  };
  const boxed = (fb, x, y, w, h) => { rect(fb, x, y, w, h, 1); frame(fb, x + 2, y + 2, w - 4, h - 4, 0); };
  const bar = (fb, x, y, w, frac, tickFrac) => {                 // outlined bar, fill, an XOR tick through it
    frame(fb, x, y, w, 7, 1); rect(fb, x + 1, y + 1, Math.round((w - 2) * clamp(frac, 0, 1)), 5, 1);
    if (tickFrac != null) xorCol(fb, x + Math.round((w - 1) * tickFrac), y - 2, y + 8);
  };

  // a full-width 11 px bar with its value written inside, XOR-ed so it reads over the fill and over the empty part
  function valueBar(fb, y, frac, tickFrac, label, right, hatch) {
    const wFill = Math.round(122 * clamp(frac, 0, 1));
    if (hatch) {                                                            // a HOLD: every other pixel, a checkerboard
      for (let yy = y + 1; yy < y + 10; yy++) for (let xx = 3; xx < 3 + wFill; xx++) if ((xx + yy) % 2 === 0) px(fb, xx, yy, 1);
    } else rect(fb, 3, y + 1, wFill, 9, 1);                                 // no outline: the fill, its text and the tick only
    if (tickFrac >= 0) xorCol(fb, 2 + Math.round(123 * tickFrac), y - 1, y + 11);     // (< 0: no tick: the graded confidence has no threshold)
    const lx = right ? 123 - textW(label, 1) : 5;
    const tmp = new Uint8Array(W * H); text(tmp, label, lx, y + 2, 1, 1);
    if (hatch) {                                                            // a checkerboard under XOR-ed text is unreadable: clear a box for the label, then OR it in
      for (let yy = y + 1; yy < y + 10; yy++) for (let xx = lx - 1; xx < lx + textW(label, 1) + 1; xx++) px(fb, xx, yy, 0);
      for (let i = 0; i < tmp.length; i++) fb[i] |= tmp[i];
      return;
    }
    for (let i = 0; i < tmp.length; i++) fb[i] ^= tmp[i];
  }
  // the placeholder jank glyph, 11 x 8: three linked dots (no jank icon exists in ~/GitHub/jank or ~/GitHub/optimist yet)
  function drawJankGlyph(fb, x, y) {
    for (const [gx, gy, w, h] of [[0, 5, 3, 3], [4, 0, 3, 3], [8, 5, 3, 3]]) rect(fb, x + gx, y + gy, w, h, 1);
    for (const [gx, gy] of [[3, 4], [3, 3], [7, 3], [7, 4], [3, 6], [4, 6], [5, 6], [6, 6], [7, 6]]) px(fb, x + gx, y + gy, 1);
  }
  // network icon of the top bar, right edge xr: JANK glyph, Wi-Fi bars (CLIENT), bars + AP (AP); crossed out without signal
  function drawNetIcon(fb, c, xr) {
    const m = c.net.mode, bars = c.wifi === 'good' ? 3 : c.wifi === 'weak' ? 1 : 0;
    let left = xr - 10;
    if (m === 0) drawJankGlyph(fb, xr - 10, 0);
    else {
      const bx = m === 2 ? xr - 20 : xr - 7;
      for (let i = 0; i < 3; i++) { const h = 2 + 2 * i; if (i < bars) rect(fb, bx + 3 * i, 8 - h, 2, h, 1); else rect(fb, bx + 3 * i, 7, 2, 1, 1); }
      if (m === 2) text(fb, 'AP', xr - 10, 0, 1, 1);
      left = bx;
    }
    if (c.wifi === 'none') for (let i = 0; i < 9; i++) fb[(8 - i) * W + left + i + (m === 0 ? 1 : 0)] ^= 1;
  }
  // policy shared with the lcd2 layout (colour = information): signal quality, link state, battery, level, confidence
  const LINK_JOINED_MIN = 1;                                          // Link peers needed to call the stick joined
  const SIGNAL_GOOD_DBM = -67, SIGNAL_POOR_DBM = -80;                  // good >= -67, fair -80..-68, poor < -80 (-80 is the Wi-Fi page's BAD line)
  const signalQuality = dbm => (dbm === null || dbm === undefined || dbm === 0 ? 'none' : dbm >= SIGNAL_GOOD_DBM ? 'good' : dbm >= SIGNAL_POOR_DBM ? 'fair' : 'poor');
  const LOST_BAD_PCT = 10, LOST_WARN_PCT = 3, RETRY_BAD_PCT = 30;       // the air rows of the lcd2 NETWORK screen (MODE JANK): LOST red from 10 %, amber from 3 %; RX RETRY red from 30 % (the old Wi-Fi page's lines)
  const lostInk = p => (p >= LOST_BAD_PCT ? INK.BAD : p >= LOST_WARN_PCT ? INK.WARN : INK.GOOD);
  const SIGNAL_INK = { good: INK.GOOD, fair: INK.WARN, poor: INK.BAD, none: INK.DIM };
  // joined = green; alone = dim in AUDIO mode (a lone leader is normal), red in LINK mode (it has no tempo to follow)
  const linkInk = c => ((c.peers || 0) >= LINK_JOINED_MIN ? INK.GOOD : c.link ? INK.BAD : INK.DIM);
  // the HOME top bar's boxes (x ranges, inclusive; null = not drawn), so that nothing can overlap: the transport icon, in LINK
  // mode the transport option's icon, in AUDIO mode the fold label (X2 /2 ...), the network's name (right-aligned, truncated
  // to what is left), the network icon, the USB plug, the battery
  function topBar(c) {
    const m = c.net.mode, xr = NET_ICON_XR, netL = xr - [10, 7, 20][m];
    const lt = c.link ? LT_ICONS[c.ltMode] : null, fl = !c.link ? foldLabel(c.fold) : '';
    const left = lt ? { l: 12, r: 12 + lt[0].length - 1 } : fl ? { l: 12, r: 12 + textW(fl, 1) - 1 } : null;
    const nameLeft = left ? left.r + 5 : 10;
    const full = [c.net.group != null ? 'GRP ' + c.net.group : '', c.net.ssid, c.net.apName][m] || '';
    const name = full ? fit(full, netL - 2 - nameLeft) : '';
    return { tri: { l: 2, r: 8 }, left, label: fl, lt, name, nameBox: name ? { l: netL - 2 - textW(name, 1), r: netL - 3 } : null, nameR: netL - 2,
             net: { l: netL, r: xr }, xr,
             usb: c.usb ? { l: PLUG_X, r: PLUG_X + USB_ICON[0].length - 1 } : null, batt: c.fitted ? { l: BATT_X, r: BATT_X + 16 } : null };
  }
  const BATT_BAD_PCT = 15, BATT_WARN_PCT = 30;                       // the battery icon: red under 15 %, amber under 30 %, else green
  function drawPower(fb, c) {
    const t = topBar(c);
    drawNetIcon(fb, c, t.xr);
    if (c.usb) icon(fb, USB_ICON, PLUG_X, 1);                        // a plug on its cable
    if (c.fitted) {
      frame(fb, 107, 0, 15, 7, 1); rect(fb, 122, 2, 2, 3, 1);
      rect(fb, 109, 2, Math.round(clamp(c.batt, 0, 100) / 100 * 11), 3, 1);
    }
  }
  // the bottom status line: one item at a time, cycling (as on Optimist)
  const STATUS_CYCLE_MS = 2000;                                      // 2 s each, as Optimist's STATE line
  const ARMED_BLINK_MS = 400;
  // Wi-Fi quality, as Optimist's Wi-Fi page shows it (op_wifi.c wf_q_text): the signal and the rate, then the share of
  // the frames sent over the last 10 s that needed a retry and that were never acknowledged. Optimist colours them (good /
  // warn / bad); a 1-bit screen draws a BAD value inverted instead. Same thresholds as Optimist.
  // one line (the user: "retry, loss % and wifi stats could fit on one status line"): "-58dBm 24M R3% L0%" spread over
  // the width, each part inverted on its own when BAD; the worst case "-100dBm 72M R99% L99%" is 21 characters
  function wifiItems(q) {
    if (!q) return [];
    if (!q.rssi) return [{ segs: [{ t: 'NO SIGNAL', bad: true }] }];
    const pct = n => (q.tx ? Math.round(100 * n / q.tx) : null), retry = pct(q.retried), lost = pct(q.lost);
    return [{ segs: [
      { t: q.rssi + 'dBm', bad: q.rssi < -80 },
      { t: Math.round(q.kbps / 1000) + 'M', bad: q.rssi < -80 },
      { t: 'R' + (retry == null ? '--' : retry + '%'), bad: retry != null && retry >= 30 },
      { t: 'L' + (lost == null ? '--' : lost + '%'), bad: lost != null && lost >= 10 },
    ], spread: true }];
  }
  function statusLine(fb, c, now, y) {
    const items = [];                                                // the mode is not listed: the meters (or their absence) say it
    // Link peers in every network mode (jank is Link too, over its own air); no peers: no peer item. The count joins the
    // Wi-Fi line as "P2" when it fits (the user: "the number of peers could also fit in the same status line with wifi"),
    // else it keeps an item of its own in the cycle
    const wifi = wifiItems(c.wifiQ);
    const peerSeg = c.peers > 0 ? { t: 'P' + c.peers, bad: false } : null;
    const fits = segs => segs.reduce((a, g) => a + textW(g.t, 1), 0) + (segs.length - 1) * (adv(' ') + 1) <= W - 4;
    if (peerSeg && wifi.length && fits([peerSeg, ...wifi[0].segs])) wifi[0] = { ...wifi[0], segs: [peerSeg, ...wifi[0].segs], spread: true };
    else if (peerSeg) items.push({ t: c.peers === 1 ? '1 LINK PEER' : c.peers + ' LINK PEERS', bad: false });
    items.push(...wifi);
    if (!items.length) return;
    const it = items[Math.floor(now / STATUS_CYCLE_MS) % items.length];
    const segs = it.segs || [{ t: it.t, bad: it.bad }];
    // spread (the user: "make better use of the space"): the parts fill the whole width, the first flush left, the last
    // flush right, the free pixels shared out as equal gaps
    const ws = segs.map(s => textW(s.t, 1)), free = W - 4 - ws.reduce((a, b) => a + b, 0);
    const n = segs.length - 1, gaps = it.spread && n > 0 && free >= n * (adv(' ') + 1) ? n : 0;   // too tight: plain spaces
    let x = 2;
    segs.forEach((s, i) => {                                           // each part inverted on its own when BAD
      const w = ws[i];
      if (s.bad) { rect(fb, x - 2, y - 1, w + 4, 9, 1); text(fb, s.t, x, y, 1, 0); }
      else text(fb, s.t, x, y, 1, 1);
      // spread: gap i gets its share of the free pixels; else the 1 px glyph gap textW drops
      x += w + (gaps ? Math.floor(free * (i + 1) / gaps) - Math.floor(free * i / gaps) : 1 + (it.spread ? adv(' ') : 0));
    });
  }
  function beatBoxes(fb, c, x0, y, w, h, gap) {          // the current beat's box is filled for the whole beat; no beat: all empty
    for (let i = 0; i < 4; i++) {
      const x = x0 + i * (w + gap), on = c.beatN != null && ((c.beatN % 4) + 4) % 4 === i;
      if (on) rect(fb, x, y, w, h, 1);
      else frame(fb, x, y, w, h, 1);
    }
  }
  // the downbeat estimator under the AUDIO boxes (bar: { state 'off'|'listening'|'locked'|'manual', conf 0..1 } or null):
  // LOCKED lights the 1's box (a bar under the first box: the bar's 1 is the music's), a 1-pixel tick under the four boxes
  // grows with the confidence (the lead against the commit level; listening and locked), MANUAL writes an M; OFF: nothing
  function barMarks(fb, bar, x0, y, w, h, gap) {
    if (!bar || bar.state === 'off') return;
    if (bar.state === 'manual') { text(fb, 'M', x0, y + h + 3, 1, 1); return; }
    if (bar.state === 'locked') rect(fb, x0, y + h + 1, w, 2, 1);
    const n = Math.round((4 * (w + gap) - gap) * clamp(bar.conf || 0, 0, 1));          // up to the boxes' width
    if (n > 0) rect(fb, x0, y + h + 4, n, 1, 1);
  }
  // HOME has two layouts, each designed for its content. Less is better: the UI explains itself.
  //   top bar: the transport icon (triangle = playing, blinking = waiting, square = stopped), in LINK mode the transport
  //            option's icon (LT_ICONS), the network's name (truncated), network icon, USB plug, battery
  //   AUDIO (any source: line in, USB audio, the XIAO's):
  //     y9   BPM x3 (left); four beat boxes (right), the current beat's box filled; under them the AUTO BAR marks
  //          (barMarks: the 1's box lit when LOCKED, an M when MANUAL; the confidence line under the boxes)
  //     y31  IN bar (smoothed short-term RMS in dBFS, min / max lines over the last few seconds, tick = -60 silence)   y44  CF bar (tick = threshold; hatched with HOLD while the XIAO's confidence holds, with COAST while the outputs run under the threshold); each value is written inside its bar
  //     y57  status line, cycling every STATUS_CYCLE_MS: the Link peers (none: empty)
  //   LINK: BPM x4, four wide beat boxes across the width, the same status line at y57
  //   (the mode picks the layout, whatever the source: a line in, USB audio, the XIAO's)
  function drawHome(fb, c, now) {
    // the transport only: silence and low confidence are already on the IN and CF bars
    const stopped = !c.running;
    drawPower(fb, c);
    // the transport: a triangle = playing, a blinking triangle = started and waiting (locking / next bar), a square = stopped
    const tri = !stopped || (c.armed && Math.floor(now / ARMED_BLINK_MS) % 2 === 0);
    if (tri) for (let r = 0; r < 7; r++) rect(fb, 2, r, Math.min(r, 6 - r) + 1, 1, 1);
    else if (!c.armed) {
      if (c.held) rect(fb, 2, 1, 5, 5, 1);
      else { rect(fb, 2, 1, 2, 5, 1); rect(fb, 5, 1, 2, 5, 1); }      // the user's Start, the outputs held by the machine layer: pause
    }
    // the network's name, right-aligned against the network icon : group (JANK), SSID
    // (CLIENT), AP name (AP); SETUP has its own screen. LINK mode: the transport option's icon after the transport icon
    // (LT_ICONS); AUDIO mode leads Link anyway: its place shows the fold (X2 /2 ...) when there is one
    const t = topBar(c);
    if (t.lt) icon(fb, t.lt, 12, 0);
    else if (t.label) text(fb, t.label, 12, 0, 1, 1);
    if (t.name) textR(fb, t.name, t.nameR, 0, 1, 1);
    if (c.link) {                                                    // LINK: big and plain
      text(fb, c.linkBpm == null ? '---' : c.linkBpm.toFixed(1), 2, 9, 4, 1);     // null: no session tempo known yet
      beatBoxes(fb, c, 2, 41, 30, 12, 2);
      statusLine(fb, c, now, 57);
      return;
    }
    text(fb, c.bpm ? c.bpm.toFixed(1) : '---', 2, 9, 3, 1);
    beatBoxes(fb, c, 87, 9, 9, 9, 1);
    barMarks(fb, c.bar, 87, 9, 9, 9, 1);
    const inLabel = 'IN ' + Math.round(c.levelDb) + 'dB';
    valueBar(fb, 31, (c.levelDb - METER_MIN_DB) / -METER_MIN_DB, (SILENCE_DB - METER_MIN_DB) / -METER_MIN_DB, inLabel);
    for (const db of [c.levelMin, c.levelMax]) {                           // min / max lines: not through the label (a line across a letter reads as another letter)
      const x = 3 + Math.round(121 * clamp((db - METER_MIN_DB) / -METER_MIN_DB, 0, 1));
      if (x >= 4 && x <= 6 + textW(inLabel, 1)) { xorCol(fb, x, 32, 32); xorCol(fb, x, 41, 41); } else xorCol(fb, x, 32, 41);
    }
    // CF: hatched, HOLD, while the confidence holds (jank M2k: no evidence; the value shown eases toward the threshold)
    // COAST: the outputs keep running under the threshold (the robust follower holds a tempo through weak patches): hatched too
    const coast = !c.confHold && !!c.running && gateConf(c) * 100 < c.th;
    valueBar(fb, 44, c.conf, c.confGraded ? -1 : c.th / 100, 'CF ' + Math.round(c.conf * 100) + '%' + (c.confHold ? ' HOLD' : coast ? ' COAST' : ''), false, !!c.confHold || coast);
    statusLine(fb, c, now, 57);
  }
  function drawMenu(fb, ui, c) {
    titleBar(fb, 'MENU', (ui.cursor + 1) + '/' + MENU.length);
    const vals = { 'LINK TRANSPORT': LT_SHORT[c.ltMode], 'AUTO BAR': AUTOBAR_SHORT[autoBarIndex(c.autoBar)], NETWORK: NET_SHORT[c.net.mode],
                   USB: EDITS.usbrole.opts[c.usbRole], 'USB MIDI CLOCK OUT': EDITS.usbmidi.opts[c.usbMidi], 'CLK RATE': RATES_SHORT[c.rate], BATTERY: c.fitted ? c.batt + '%' : '-' };
    for (let k = 0; k < VISIBLE; k++) {
      const i = ui.top + k; if (i >= MENU.length) break;
      const y = 11 + k * 9, sel = i === ui.cursor, v = sel ? 0 : 1;
      if (sel) rect(fb, 0, y - 1, W, 9, 1);
      text(fb, MENU[i].name, 4, y, 1, v); textR(fb, vals[MENU[i].name] || '', W - 4, y, 1, v);
    }
  }
  function drawSub(fb, ui, c) {
    const sub = SUBS[ui.sub];
    titleBar(fb, sub.title);
    sub.rows.forEach((f, i) => {
      const y = 14 + i * 11, sel = i === ui.ocur, v = sel ? 0 : 1, sp = EDITS[f];
      if (sel) rect(fb, 0, y - 2, W, 11, 1);
      text(fb, sp.title, 4, y, 1, v); textR(fb, sp.opts ? (f === 'lowconf' ? (sp.get(c) ? 'STOP' : 'RUN') : sp.opts[sp.get(c)]) : editText(sp, sp.get(c)), W - 4, y, 1, v);
    });
  }
  // the ABOUT screen: its rows as { label, val } (label left, value right aligned), "-" for what is not known (null / undefined in the ctx); the repo is shortened on the OLED, the lcd2 layout adds the PSRAM
  const ABOUT_ROWS = 13, ABOUT_VIS_OLED = 5, ABOUT_VIS_LCD2 = 9;
  const known = v => v !== null && v !== undefined;
  const aboutRows = (c, lcd) => {
    const num = (label, v, unit) => ({ label, val: known(v) && v >= 0 ? v + unit : '-' });
    const up = !known(c.uptimeS) || c.uptimeS < 0 ? '-' : c.uptimeS >= 86400 ? Math.floor(c.uptimeS / 86400) + 'd ' + Math.floor(c.uptimeS % 86400 / 3600) + 'h'
      : Math.floor(c.uptimeS / 3600) + ':' + String(Math.floor(c.uptimeS % 3600 / 60)).padStart(2, '0') + ':' + String(c.uptimeS % 60).padStart(2, '0');
    const heap = !known(c.heapKb) || c.heapKb < 0 ? '-' : c.heapKb + 'K' + (lcd && known(c.psramKb) && c.psramKb >= 0 ? ' PSRAM ' + c.psramKb + 'K' : '');
    const flash = !known(c.flashKb) || c.flashKb < 0 ? '-' : Math.trunc(c.flashKb / 1024) + 'M ' + (known(c.appUsedKb) && c.appUsedKb >= 0 && known(c.appKb) && c.appKb >= 0
      ? (c.appUsedKb / 1024).toFixed(1) + '/' + (c.appKb / 1024).toFixed(1) + 'M' : '-');
    // PANIC: "-" when none, else "<task> 0x4200abcd" from "<task>@0x4200abcd"; the task is cut to what fits beside the label (3 characters on the OLED, 7 on the colour layout), the address stays whole
    const pAt = known(c.panic) ? c.panic.lastIndexOf('@') : -1;
    const panic = !known(c.panic) || c.panic === '' ? '-' : pAt < 0 ? c.panic : c.panic.slice(0, Math.min(pAt, lcd ? 7 : 3)) + (pAt > 0 ? ' ' : '') + c.panic.slice(pAt + 1);
    return [{ label: 'VERSION', val: known(c.version) ? c.version : '-' }, { label: 'REPO', val: lcd ? 'github.com/w0ts/jank' : 'w0ts/jank' },
            { label: 'UPTIME', val: up }, { label: 'RESET', val: known(c.reset) ? c.reset : '-' }, { label: 'TEMP', val: known(c.tempC) ? c.tempC + ' C' : '-' },
            { label: 'CPU', val: (known(c.cpuPct) && c.cpuPct >= 0 ? c.cpuPct : '-') + '/' + (known(c.cpu1Pct) && c.cpu1Pct >= 0 ? c.cpu1Pct : '-') + '%' }, { label: 'MEMORY', val: heap }, num('MIN HEAP', c.heapMinKb, 'K'), { label: 'FLASH', val: flash },
            num('CLOCK LATE', c.clkLateUs, ' US'), num('UI LOOP', c.uiLoopUs, ' US'), num('BEAT LOOP', c.beatLoopUs, ' US'), { label: 'PANIC', val: panic }];
  };
  function drawInfo(fb, ui, c) {
    if (ui.page === 'about') { titleBar(fb, 'ABOUT'); aboutRows(c, false).slice(ui.ascroll || 0, (ui.ascroll || 0) + ABOUT_VIS_OLED).forEach((r, k) => { text(fb, r.label, 4, 13 + k * 9, 1, 1); textR(fb, r.val, W - 4, 13 + k * 9, 1, 1); }); return; }
    titleBar(fb, 'BATTERY');
    const rows = [];
    if (!c.fitted) rows.push('NO CELL FITTED', 'POWER ' + (c.usb ? 'USB' : 'NONE'));
    else {
      const volt = 3.3 + 0.9 * c.batt / 100;                        // rough: 3.3 V empty, 4.2 V full
      rows.push('LEVEL ' + c.batt + '%', 'VOLT ' + volt.toFixed(2) + 'V EST', c.usb ? (c.batt < 100 ? 'CHARGING' : 'FULL') : 'DISCHARGING');
      bar(fb, 4, 50, 120, c.batt / 100, null);
    }
    rows.forEach((r, i) => text(fb, r, 4, 14 + i * 11, 1, 1));
  }
  function drawEdit(fb, ui) {
    const e = ui.edit, sp = EDITS[e.field];
    titleBar(fb, sp.title);
    const s = editText(sp, e.val);                                    // the menus' one font size, always
    const w = textW(s, 1) + 12, x = Math.round((W - w) / 2);
    rect(fb, x, 16, w, 13, 1); text(fb, s, x + 6, 19, 1, 0);                 // the value, inverted
    const [h1, h2] = helpOf(sp, e.val);
    textC(fb, h1, 44, 1, 1); textC(fb, h2, 54, 1, 1);                         // what the setting does
  }
  const fit = (s, w) => (s.length * adv('A') > w ? s.slice(0, Math.floor(w / adv('A'))) : s);
  function drawNet(fb, ui, c) {
    titleBar(fb, 'NETWORK');
    netRows(c).forEach((r, i) => {
      const y = 11 + i * 9, sel = i === ui.ncur, v = sel ? 0 : 1;
      if (sel) rect(fb, 0, y - 1, W, 9, 1);
      text(fb, r.name, 4, y, 1, v); textR(fb, fit(r.val, 124 - 8 - textW(r.name, 1)), W - 4, y, 1, v);
      if (sel) r.help.forEach((l, k) => { if (l) text(fb, l, 2, 49 + 8 * k, 1, 1); });     // what the selected row is
    });
  }
  function drawScan(fb, ui, c) {
    titleBar(fb, 'NETWORK', c.net.scan.length ? (ui.scur + 1) + '/' + c.net.scan.length : '');
    if (!c.net.scan.length) { textC(fb, 'NO NETWORKS', 28, 1, 1); return; }
    for (let k = 0; k < 5; k++) {
      const i = ui.stop + k, n = c.net.scan[i]; if (!n) break;
      const y = 11 + k * 10, sel = i === ui.scur, v = sel ? 0 : 1;
      if (sel) rect(fb, 0, y - 1, W, 10, 1);
      text(fb, (n.name === c.net.ssid ? '*' : ' ') + fit(n.name, 90), 2, y, 1, v);
      for (let b = 0; b < 3; b++) { const h = 2 + 2 * b; if (b < n.bars) rect(fb, 112 + 4 * b, y + 7 - h, 3, h, v); else px(fb, 113 + 4 * b, y + 6, v); }
    }
  }
  function drawText(fb, ui, c) {
    const t = ui.txt, n = t.text.length, start = Math.max(0, t.pos - 18);    // the menus' one font size: 19 cells of 6 px
    titleBar(fb, t.field === 'apname' ? 'AP NAME' : 'PASSKEY', String(n));
    for (let k = 0; k < 19; k++) {
      const i = start + k; if (i > n) break;
      const x = 7 + k * 6, sel = i === t.pos, real = i < n ? t.text[i] : ' ';
      if (sel) { rect(fb, x - 1, 15, 7, 11, 1); text(fb, real, x, 17, 1, 0); }
      else text(fb, t.mask && i < n ? '*' : real, x, 17, 1, 1);
    }
    textC(fb, 'YES NEXT  NO DEL', 33, 1, 1);
    textC(fb, 'easier: web UI', 43, 1, 1);
    textC(fb, c.net.mode === 2 ? '192.168.4.1' : 'jank.local', 53, 1, 1);
  }
  function drawNetInfo(fb, ui, c) {
    const n = c.net;
    titleBar(fb, n.mode === 1 ? 'LINK CLIENT' : 'LINK AP');
    const rows = n.mode === 1 ? ['STATE ' + n.status, 'IP ' + n.ip, 'SSID ' + fit(n.ssid || '-', 100)]
      : ['IP 192.168.4.1', 'CLIENTS ' + n.apClients, 'NAME ' + n.apName];
    rows.forEach((r, i) => text(fb, r, 4, 14 + i * 11, 1, 1));
  }
  function drawConfirm(fb, title, line1, line2) {
    titleBar(fb, title); textC(fb, line1, 16, 1, 1); textC(fb, line2, 27, 1, 1); textC(fb, 'YES      NO', 48, 1, 1);
  }
  // setup: the open hotspot's QR code (standard Wi-Fi code, dark modules on a lit box so it scans) with the text beside it
  function drawSetup(fb, ui, c) {
    if (ui.leave) { drawConfirm(fb, 'SETUP', 'LEAVE SETUP?', 'RUN AGAIN'); return; }
    rect(fb, 0, 1, 62, 62, 1);
    const q = c.qr;
    for (let y = 0; y < q.size; y++) for (let x = 0; x < q.size; x++) if (q.mod[y][x]) rect(fb, 2 + 2 * x, 3 + 2 * y, 2, 2, 0);
    const a = c.net.setupName.slice(0, 11), b = c.net.setupName.slice(11);
    text(fb, 'SETUP MODE', 63, 3, 1, 1); text(fb, a, 63, 15, 1, 1); text(fb, b, 63, 24, 1, 1);
    text(fb, 'open', 63, 37, 1, 1); text(fb, '192.168.4.1', 63, 46, 1, 1); text(fb, 'NO: LEAVE', 63, 56, 1, 1);
  }
  function drawOverlays(fb, ui, now) {
    if (ui.hint) {                                                   // NO / YES held on HOME: what UP / DOWN will do
      boxed(fb, 6, 12, 116, 40);
      textC(fb, ui.hint.title, 17, 1, 0); textC(fb, ui.hint.up, 31, 1, 0); textC(fb, ui.hint.down, 42, 1, 0);
      return;
    }
    if (ui.overlay && now < ui.overlay.until) {
      boxed(fb, 14, 12, 100, 40);
      const v = ui.overlay.value, sc = [3, 2, 1].find(k => textW(v, k) <= 96) || 1;   // the largest size that fits the box
      textC(fb, ui.overlay.title, 17, 1, 0); textC(fb, v, 38 - Math.round(7 * sc / 2), sc, 0);
    }
  }

  // ======================================================== LCD2 LAYOUT ========================================================
  // ctx.layout === 'lcd2' (the Waveshare ESP32-S3-LCD-2): drawn at the panel's TRUE 320 x 240 pixels (SIZES.lcd2, 4:3): every pixel is one panel
  // pixel, painted 1:1 (ui/lcd2.js). The small text is the 5 x 7 face doubled and smoothed (Scale2x: 10 x 14, 2 px strokes, a fixed 12 px advance,
  // 14 px rows), so a position of the old 160 x 120 grid simply doubles. Frames, bars and outlines are 1 px (the unlit beat boxes 2 px), the icons are
  // redrawn at 1 px detail. The tempo has bitmap fonts of its own (ui/bigfont.js: NORMAL 52 x 70, LARGE 70 x 94). The OLED boards keep 128 x 64
  // and never run this code. Colour is information only (INK, see tag()); menus and edit screens have none.
  //
  //   HOME (AUDIO)  y 2..23    top bar: transport icon, LINK TRANSPORT icon, fold label (left); GRP n, the chain icon + peers, the plug (only while charging) and the battery (right)
  //                 y 32..101  the TEMPO in the NORMAL digits (70 rows; the digit after the dot is 46 rows), centred on the 320 columns
  //                 y 110..165 four beat boxes 72 x 56 (gap 8, x 4..315)
  //                 then the info rows INPUT (VU meter, notch at the tracker's -50 dBFS), CONFIDENCE (line, notch at the stop threshold), BAR (AUTO BAR's
  //                 state, only with AUTO BAR on), 2 rows apart, as one block centred between the boxes and the state line (GATED / HOLD / COAST /
  //                 STOPPED / SILENT, centred, empty otherwise; its row y 226..239 is always reserved)
  //   HOME (LINK)   the same top bar; the tempo in the LARGE digits (94 rows, y 38..131), centred; four square beat boxes 72 x 72, centred as a row; no audio rows
  //   lists         title bar 22 px, 12 rows of 18 px from y 26; sub menus and info 24 px rows; edit: the value at scale 2
  const SIZES = { oled: { w: OLED_W, h: OLED_H }, lcd2: { w: 320, h: 240 } };
  const sizeOf = layout => (layout === 'lcd2' ? SIZES.lcd2 : SIZES.oled);
  const CONF_WARN_MARGIN = 10;                                       // amber from the stop threshold up to this many points above it
  const CLIP_PEAK_DB = -1, CLIP_RMS_MAX_DB = -3, QUIET_DB = -50;     // clipping: the last second's peak (or, unreported, the RMS maximum); quiet: under the tracker's -50 dBFS sound level
  function levelInk(c) {
    const clip = c.peakDb !== null && c.peakDb !== undefined ? c.peakDb >= CLIP_PEAK_DB : c.levelMax >= CLIP_RMS_MAX_DB;
    if (clip) return INK.BAD;
    return c.levelDb < QUIET_DB ? INK.WARN : INK.NONE;
  }
  // the gate's own confidence: the graded value (confGraded) is what is shown, the colour, COAST and the notch's meaning stay the gate's
  const gateConf = c => (c.confGraded && typeof c.confGate === 'number' ? c.confGate : c.conf);
  function confInk(c, th = c.th) {
    if (c.confKnown === false || c.silent) return INK.DIM;
    const pct = Math.round(gateConf(c) * 100);
    return pct < th ? INK.BAD : pct <= th + CONF_WARN_MARGIN ? INK.WARN : INK.GOOD;
  }
  const battInk = pct => (pct < BATT_BAD_PCT ? INK.BAD : pct < BATT_WARN_PCT ? INK.WARN : INK.NONE);      // neutral while healthy
  const charging = c => !!c.usb && !!c.fitted && c.batt < 100;       // the BATTERY info screen's inference: on USB and not full
  // the VU meter's colour ramp: VU.steps ink ids from VU.ink0 up (ui/lcd2.js has their RGB); a bar column i of barW has the id ink0 + floor(steps * i / barW), so the bar is a continuous gradient
  const VU = { ink0: 6, steps: 32 };

  // the HOME geometry (320 x 240 pixels): one table, the tests read it
  const L2 = {
    topH: 26, iconY: 2, iconH: 22, playX: 4, ltX: 28, foldX: 62, nameXR: 187, nameMaxW: 90, linkX: 195, linkW: 49, peersX: 220, plugX: 246, battX: 268, battW: 48,
    chainW: 19, chainH: 14,
    tempoY: 32, linkTempoY: 38,
    boxGap: 8, boxBorder: 2, boxY: 110, audioBox: { w: 72, h: 56, gap: 8 }, rowGap: 4, stateY: 226,
    linkBoxSide: 72, linkBoxY: 146,
    labelX: 4, valueXR: 316, barX: 128, barW: 124, lineH: 6, vuH: 10,
  };
  const barFillWidth = frac => Math.round(L2.barW * clamp(frac, 0, 1));          // the line's length: the value on 124 px
  const barTickX = tickFrac => L2.barX + Math.round((L2.barW - 2) * tickFrac);   // the notch is 2 px wide
  const dbFrac = db => clamp((db - METER_MIN_DB) / -METER_MIN_DB, 0, 1);
  const vuInk = col => VU.ink0 + Math.floor(VU.steps * col / L2.barW);

  // 1-bit rows ('#' = lit), at the old 160 x 120 size: they are drawn doubled: the plug with Scale2x (icon2), the LINK TRANSPORT icons plain 2 x 2 (icon2p). The USB plug on its cable 9 x 7 (drawn left of the
  // battery while charging), and the LINK TRANSPORT option icons: IGNORE = a crossed circle (own start only), FOLLOW = an arrow coming in, FOLLOW+LEAD = a double arrow
  const PLUG = ['..####...', '..#######', '#########', '######...', '#########', '..#######', '..####...'];
  const LT_ICONS_L2 = [
    ['...#####...', '..##...##..', '.##.....##.', '##.....#.##', '#.....#...#', '#....#....#', '#...#.....#', '##.#.....##', '.##.....##.', '..##...##..', '...#####...'],
    ['.....#.......', '....##.......', '...###.......', '..####.......', '.############', '#############', '.############', '..####.......', '...###.......', '....##.......', '.....#.......'],
    ['.....#.#.....', '....##.##....', '...###.###...', '..####.####..', '.###########.', '#############', '.###########.', '..####.####..', '...###.###...', '....##.##....', '.....#.#.....'],
  ];
  const LT_FL_W = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1];   // FOLLOW+LEAD triangle row widths: max(1, round(12 * (1 - |y - 10.5| / 11)))
  const TRI_W = [1, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 18, 16, 14, 12, 11, 9, 7, 6, 4, 2, 1];   // the play triangle's row widths (18 x 22)
  function icon2(fb, rows, x, y) { icon(fb, rows2x(rows), x, y); }
  const plain2x = rows => rows.flatMap(r => { const d = [...r].map(ch => ch + ch).join(''); return [d, d]; });   // every pixel an exact 2 x 2 block
  const icon2p = (fb, rows, x, y) => icon(fb, plain2x(rows), x, y);
  // the chain (the Optimist's link icon, 19 x 14): two rounded links with 2 px strokes, the second down and right of the first
  function drawChain(fb, x, y) {
    for (let k = 0; k < 2; k++) {
      const lx = x + 7 * k, ly = y + 6 * k;
      rect(fb, lx + 2, ly, 8, 2, 1); rect(fb, lx + 2, ly + 6, 8, 2, 1);
      rect(fb, lx, ly + 2, 2, 4, 1); rect(fb, lx + 10, ly + 2, 2, 4, 1);
      px(fb, lx + 1, ly + 1, 1); px(fb, lx + 10, ly + 1, 1); px(fb, lx + 1, ly + 6, 1); px(fb, lx + 10, ly + 6, 1);
    }
  }
  const xorCols = (fb, x, w, y0, y1) => { for (let i = 0; i < w; i++) xorCol(fb, x + i, y0, y1); };

  function drawTransportLcd2(fb, c, now) {
    const tri = c.running || (c.armed && Math.floor(now / ARMED_BLINK_MS) % 2 === 0);
    if (tri) TRI_W.forEach((w, r) => rect(fb, L2.playX, L2.iconY + r, w, 1, 1));
    else if (!c.armed) {
      if (c.held) rect(fb, L2.playX, L2.iconY + 2, 18, 18, 1);
      else { rect(fb, L2.playX, L2.iconY + 2, 6, 18, 1); rect(fb, L2.playX + 12, L2.iconY + 2, 6, 18, 1); }   // the user's Start, held by the machine layer: pause
    }
  }
  // the left group after the transport icon: the LINK TRANSPORT icon (both modes) at ltX and, in AUDIO mode when the fold is not 1, the fold label beside it
  function drawOptionLcd2(fb, c) {
    if (c.ltMode === 2) LT_FL_W.forEach((w, r) => { rect(fb, L2.ltX + 12 - w, L2.iconY + r, w, 1, 1); rect(fb, L2.ltX + 18, L2.iconY + r, w, 1, 1); });   // FOLLOW+LEAD: two 22 row triangles, 6 px between the bases
    else icon2p(fb, LT_ICONS_L2[c.ltMode], L2.ltX, L2.iconY);
    const fl = !c.link ? foldLabel(c.fold) : '';
    if (!fl) return;
    const w = textW(fl, 1) + 8;
    rect(fb, L2.foldX, L2.iconY, w, L2.iconH, 1);
    for (const [x, y] of [[0, 0], [1, 0], [0, 1], [w - 1, 0], [w - 2, 0], [w - 1, 1], [0, L2.iconH - 1], [1, L2.iconH - 1], [0, L2.iconH - 2], [w - 1, L2.iconH - 1], [w - 2, L2.iconH - 1], [w - 1, L2.iconH - 2]]) px(fb, L2.foldX + x, L2.iconY + y, 0);
    text(fb, fl, L2.foldX + 4, L2.iconY + 4, 1, 0);
  }
  // the battery, 48 x 22: a 1 px outline with a nub, the fill inside; neutral while healthy, amber under BATT_WARN_PCT, red under BATT_BAD_PCT
  function drawBatteryLcd2(fb, c) {
    const x = L2.battX;
    frame(fb, x, L2.iconY, 44, L2.iconH, 1);
    for (const [cx, cy] of [[0, 0], [43, 0], [0, 21], [43, 21]]) px(fb, x + cx, L2.iconY + cy, 0);
    rect(fb, x + 44, 8, 4, 10, 1);
    rect(fb, x + 3, L2.iconY + 3, Math.round(38 * clamp(c.batt, 0, 100) / 100), 16, 1);
    tag(battInk(c.batt), x, L2.iconY, L2.battW, L2.iconH);
  }
  // the top bar. Left: the transport icon (x 4), the LINK TRANSPORT icon (x 28), the fold label (x 62, AUDIO). Right: GRP n (or the Wi-Fi / own network
  // name), right-aligned to x 187, the chain (x 195) with the peer count (x 220), the plug (x 246, only while charging), the battery (x 268..315)
  function drawTopBarLcd2(fb, c, now) {
    drawTransportLcd2(fb, c, now);
    drawOptionLcd2(fb, c);
    const full = fit([c.net.group != null ? 'GRP ' + c.net.group : '', c.net.ssid, c.net.apName][c.net.mode] || '', L2.nameMaxW);
    if (full) text(fb, full, L2.nameXR - textW(full, 1), 6, 1, 1);
    drawChain(fb, L2.linkX, L2.iconY + 4);                              // the chain, and the Link peers beside it
    if ((c.peers || 0) > 0) text(fb, String(Math.min(c.peers, 99)), L2.peersX, 6, 1, 1);
    tag(linkInk(c), L2.linkX, 0, L2.linkW, L2.topH);
    if (charging(c)) icon2(fb, PLUG, L2.plugX, L2.iconY + 4);           // the plug beside the battery, neutral
    if (c.fitted) drawBatteryLcd2(fb, c);
  }
  function drawTempoLcd2(fb, c) {
    const bpm = c.link ? c.linkBpm : c.bpm, s = bpm ? bpm.toFixed(1) : '---', font = c.link ? BigFont.LARGE : BigFont.NORMAL;
    font.draw((x, y) => px(fb, x, y, 1), s, Math.floor((W - font.width(s)) / 2), c.link ? L2.linkTempoY : L2.tempoY);
  }

  function rowText(fb, y, label, value) {
    text(fb, label, L2.labelX, y, 1, 1);
    text(fb, value, L2.valueXR - textW(value, 1), y, 1, 1);
  }
  // the CONFIDENCE row: label, ONE thin line (6 px high, no outline, no track) whose length is the value on 124 px, the value right-aligned
  // at the end of the row. A notch, 2 px wide and 10 tall, is XOR-ed through the line at tickFrac (a gap in the line where it passes, a
  // stub above and below where it does not). The value, the line and the notch carry `ink`; the label stays neutral.
  function lineRow(fb, y, label, value, frac, tickFrac, ink) {
    rowText(fb, y, label, value);
    tag(ink, L2.valueXR - textW(value, 1), y, textW(value, 1), 14);
    const n = barFillWidth(frac);
    rect(fb, L2.barX, y + 4, n, L2.lineH, 1);
    tag(ink, L2.barX, y + 4, n, L2.lineH);
    if (tickFrac < 0) return;                                              // (< 0: no notch: the graded confidence has no threshold)
    const x = barTickX(tickFrac); xorCols(fb, x, 2, y + 2, y + 11); tag(ink, x, y + 2, 2, 10);
  }
  // the INPUT row: a continuous VU meter 10 px high on 124 px: round(124 x level) columns, no segments and no gaps. Every column has the ramp id of its
  // position on the dBFS scale (green quiet, red hot). A notch (14 px tall, 2 wide, XOR-ed) marks the tracker's -50 dBFS. The number carries the level rule's ink.
  function vuRow(fb, y, c) {
    const value = Math.round(c.levelDb) + 'dB', top = y + 2, n = barFillWidth(dbFrac(c.levelDb));
    rowText(fb, y, 'INPUT', value);
    tag(levelInk(c), L2.valueXR - textW(value, 1), y, textW(value, 1), 14);
    rect(fb, L2.barX, top, n, L2.vuH, 1);
    for (let i = 0; i < n; i++) tag(vuInk(i), L2.barX + i, top, 1, L2.vuH);
    xorCols(fb, barTickX(dbFrac(QUIET_DB)), 2, y, y + 13);
  }
  // the BAR row: the label and AUTO BAR's state (the 10 character label column), a 1 px outlined bar (124 x 10, the fill inside, so the fill is
  // round(122 x confidence)); neutral, no tick
  function outlinedRow(fb, y, label, value, frac) {
    text(fb, label, L2.labelX, y, 1, 1);
    text(fb, value, L2.labelX + 116 - textW(value, 1), y, 1, 1);
    frame(fb, L2.barX, y + 2, L2.barW, 10, 1);
    rect(fb, L2.barX + 1, y + 3, Math.round((L2.barW - 2) * clamp(frac, 0, 1)), 8, 1);
  }
  const boxRowX = (w, gap) => Math.floor((W - (4 * w + 3 * gap)) / 2);          // the four boxes as a row, centred
  function beatBoxesLcd2(fb, c, x0, y, w, h, gap) {                           // the beat indicator (no LEDs): the lit box solid, the others 2 px outlines; the first one is amber only while lit
    for (let i = 0; i < 4; i++) {
      const x = x0 + i * (w + gap), on = c.beatN != null && ((c.beatN % 4) + 4) % 4 === i;
      if (on) rect(fb, x, y, w, h, 1);
      else { frame(fb, x, y, w, h, 1); frame(fb, x + 1, y + 1, w - 2, h - 2, 1); }          // an unlit box: a 2 px outline
      if (i === 0 && on) tag(INK.ONE, x, y, w, h);
    }
  }
  const BAR_WORDS = { locked: 'LOCKED', listening: 'LISTEN', manual: 'MANUAL' };
  const inkText = (fb, s, x, y, ink) => { text(fb, s, x, y, 1, 1); tag(ink, x, y, textW(s, 1), 14); };
  // the state line (AUDIO only), the first state that applies: GATED (the outputs stopped for low confidence), HOLD (the confidence holds), COAST
  // (running under the threshold), STOPPED (not running), SILENT (running, nothing heard); null while it is simply running
  function stateWordLcd2(c) {
    const coast = !c.confHold && c.confKnown !== false && !!c.running && gateConf(c) * 100 < c.th;
    if (c.gated) return ['GATED', INK.BAD];
    if (c.confHold) return ['HOLD', INK.DIM];
    if (coast) return ['COAST', INK.WARN];
    if (!c.running) return ['STOPPED', INK.DIM];
    return c.silent ? ['SILENT', INK.DIM] : null;
  }
  function drawStateLcd2(fb, c) {
    const state = stateWordLcd2(c);
    if (state) inkText(fb, state[0], Math.floor((W - textW(state[0], 1)) / 2), L2.stateY, state[1]);
  }
  // the AUDIO info rows (INPUT, CONFIDENCE, BAR when shown), RGAP apart, as one block centred vertically between the bottom of the beat boxes and the
  // state line (y stateY): the empty rows above and below the block differ by at most 1, the odd one goes below
  function audioRows(withBar) {
    const n = withBar ? 3 : 2, block = n * 14 + (n - 1) * L2.rowGap, bottom = L2.boxY + L2.audioBox.h;
    const rowIN = bottom + Math.floor((L2.stateY - bottom - block) / 2), pitch = 14 + L2.rowGap;
    return { rowIN, rowCF: rowIN + pitch, rowBAR: rowIN + 2 * pitch };
  }
  function drawHomeLcd2(fb, c, now) {
    drawTopBarLcd2(fb, c, now);
    drawTempoLcd2(fb, c);
    if (c.link) {
      beatBoxesLcd2(fb, c, boxRowX(L2.linkBoxSide, L2.boxGap), L2.linkBoxY, L2.linkBoxSide, L2.linkBoxSide, L2.boxGap);
      return;
    }
    const withBar = !!(c.bar && BAR_WORDS[c.bar.state]), b = L2.audioBox, r = audioRows(withBar);
    beatBoxesLcd2(fb, c, boxRowX(b.w, b.gap), L2.boxY, b.w, b.h, b.gap);
    vuRow(fb, r.rowIN, c);
    const known = c.confKnown !== false;
    lineRow(fb, r.rowCF, 'CONFIDENCE', known ? Math.round(c.conf * 100) + '%' : '--', known ? c.conf : 0, c.confGraded ? -1 : c.th / 100, confInk(c));
    if (withBar) outlinedRow(fb, r.rowBAR, 'BAR', BAR_WORDS[c.bar.state], c.bar.state === 'manual' ? 0 : c.bar.conf || 0);
    drawStateLcd2(fb, c);
  }

  // ---- the lists: a title bar of 22 px, rows of 19 px from y 26 (11 rows: the whole MENU fits), full width, neutral, the selected row inverted
  const L2_ROW0 = 26, L2_PITCH = 19, L2_ROWS = 11;
  const titleBarLcd2 = (fb, title, right) => { rect(fb, 0, 0, W, 22, 1); text(fb, title, 6, 4, 1, 0); if (right) textR(fb, right, W - 6, 4, 1, 0); };
  const boxedLcd2 = (fb, x, y, w, h) => { rect(fb, x, y, w, h, 1); frame(fb, x + 4, y + 4, w - 8, h - 8, 0); };       // a lit box with a 1 px dark line inside
  const barLcd2 = (fb, x, y, w, frac) => { frame(fb, x, y, w, 14, 1); rect(fb, x + 1, y + 1, Math.round((w - 2) * clamp(frac, 0, 1)), 12, 1); };
  function listRowLcd2(fb, y, h, sel, left, right) {
    const v = sel ? 0 : 1;
    if (sel) rect(fb, 0, y - 2, W, h, 1);
    text(fb, left, 8, y, 1, v);
    if (right) textR(fb, right, W - 8, y, 1, v);
  }
  const menuValues = c => ({ 'LINK TRANSPORT': LT_SHORT[c.ltMode], 'AUTO BAR': AUTOBAR_SHORT[autoBarIndex(c.autoBar)], NETWORK: NET_SHORT[c.net.mode],
    USB: EDITS.usbrole.opts[c.usbRole], 'USB MIDI CLOCK OUT': EDITS.usbmidi.opts[c.usbMidi], 'CLK RATE': RATES_SHORT[c.rate], BATTERY: c.fitted ? c.batt + '%' : '-' });
  function drawMenuLcd2(fb, ui, c) {
    titleBarLcd2(fb, 'MENU', (ui.cursor + 1) + '/' + MENU.length);
    const vals = menuValues(c), top = clamp(ui.cursor - L2_ROWS + 1, 0, Math.max(0, MENU.length - L2_ROWS));
    for (let k = 0; k < L2_ROWS && top + k < MENU.length; k++) listRowLcd2(fb, L2_ROW0 + k * L2_PITCH, 18, top + k === ui.cursor, MENU[top + k].name, vals[MENU[top + k].name] || '');
  }
  function drawSubLcd2(fb, ui, c) {
    const sub = SUBS[ui.sub];
    titleBarLcd2(fb, sub.title);
    sub.rows.forEach((f, i) => {
      const sp = EDITS[f];
      listRowLcd2(fb, 36 + i * 24, 22, i === ui.ocur, sp.title, sp.opts ? (f === 'lowconf' ? (sp.get(c) ? 'STOP' : 'RUN') : sp.opts[sp.get(c)]) : editText(sp, sp.get(c)));
    });
  }
  function drawInfoLcd2(fb, ui, c) {
    if (ui.page === 'about') { titleBarLcd2(fb, 'ABOUT'); aboutRows(c, true).slice(ui.ascroll || 0, (ui.ascroll || 0) + ABOUT_VIS_LCD2).forEach((r, k) => { text(fb, r.label, 8, 36 + k * 22, 1, 1); textR(fb, r.val, W - 8, 36 + k * 22, 1, 1); }); return; }
    titleBarLcd2(fb, 'BATTERY');
    const rows = [];
    if (!c.fitted) rows.push('NO CELL FITTED', 'POWER ' + (c.usb ? 'USB' : 'NONE'));
    else {
      const volt = 3.3 + 0.9 * c.batt / 100;
      rows.push('LEVEL ' + c.batt + '%', 'VOLT ' + volt.toFixed(2) + 'V EST', c.usb ? (c.batt < 100 ? 'CHARGING' : 'FULL') : 'DISCHARGING');
      barLcd2(fb, 8, 184, W - 16, c.batt / 100);
    }
    rows.forEach((r, i) => text(fb, r, 8, 36 + i * 24, 1, 1));
  }
  // THRESHOLD: the CONFIDENCE line with the notch at the threshold being edited (no number on the row), then NOW (the live confidence, its ink judged
  // against the edited threshold) and THRESHOLD (the edited value, neutral), rows of 24 px like the sub menus; no help lines
  function drawThresholdEditLcd2(fb, ui, c) {
    const e = ui.edit, known = c.confKnown !== false, ink = confInk(c, e.val), y = 60;
    titleBarLcd2(fb, EDITS.threshold.title);
    lineRow(fb, y, 'CONFIDENCE', '', known ? c.conf : 0, e.val / 100, ink);
    rowText(fb, y + 24, 'NOW', known ? Math.round(c.conf * 100) + '%' : '-');
    if (known) { const w = textW(Math.round(c.conf * 100) + '%', 1); tag(ink, L2.valueXR - w, y + 24, w, 14); }
    rowText(fb, y + 48, 'THRESHOLD', editText(EDITS.threshold, e.val));
  }
  function drawEditLcd2(fb, ui, c) {
    const e = ui.edit, sp = EDITS[e.field];
    if (e.field === 'threshold') { drawThresholdEditLcd2(fb, ui, c); return; }
    titleBarLcd2(fb, sp.title);
    const s = editText(sp, e.val), sc = textW(s, 2) + 32 <= W - 8 ? 2 : 1;      // the value in the biggest of two sizes that fits
    const w = textW(s, sc) + 32, h = 14 * sc + 16, x = Math.round((W - w) / 2);
    rect(fb, x, 48, w, h, 1); text(fb, s, x + 16, 56, sc, 0);
    const [h1, h2] = helpOf(sp, e.val);
    textC(fb, h1, 140, 1, 1); textC(fb, h2, 164, 1, 1);
  }
  // the read-only SIGNAL row under the NETWORK rows: the dBm (the best jank peer, or the router in Wi-Fi mode) and a word coloured like the bars were
  const SIGNAL_WORD = { good: 'GOOD', fair: 'FAIR', poor: 'POOR', none: 'NONE' };
  function drawSignalRowLcd2(fb, y, rssi) {
    text(fb, 'SIGNAL', 8, y, 1, 1);
    if (rssi === null || rssi === undefined) { textR(fb, '-', W - 8, y, 1, 1); return; }
    const q = signalQuality(rssi), word = SIGNAL_WORD[q], wx = W - 8 - textW(word, 1);
    inkText(fb, word, wx, y, SIGNAL_INK[q]);
    if (rssi !== 0) textR(fb, rssi + 'dBm', wx - 12, y, 1, 1);
  }
  // the read-only air rows of MODE JANK under SIGNAL, from the peers' frames of the last 10 s window: LOST % (sequence numbers missing), LEVEL (weakest / strongest
  // frame), RX RETRY % (the peers' own retries seen here, not this device's). null / no level: '-'
  function drawAirRowsLcd2(fb, y, c) {
    const known = p => p !== null && p !== undefined, pct = p => p + '%';
    const level = c.rssiMin && c.rssiMax ? c.rssiMin + '/' + c.rssiMax + ' dBm' : '-';
    text(fb, 'LOST', 8, y, 1, 1);
    if (known(c.airLost)) inkText(fb, pct(c.airLost), W - 8 - textW(pct(c.airLost), 1), y, lostInk(c.airLost)); else textR(fb, '-', W - 8, y, 1, 1);
    text(fb, 'LEVEL', 8, y + L2_PITCH, 1, 1);
    textR(fb, level, W - 8, y + L2_PITCH, 1, 1);
    text(fb, 'RX RETRY', 8, y + 2 * L2_PITCH, 1, 1);
    if (known(c.airRetry)) inkText(fb, pct(c.airRetry), W - 8 - textW(pct(c.airRetry), 1), y + 2 * L2_PITCH, c.airRetry >= RETRY_BAD_PCT ? INK.BAD : INK.NONE);
    else textR(fb, '-', W - 8, y + 2 * L2_PITCH, 1, 1);
  }
  function drawNetLcd2(fb, ui, c) {
    titleBarLcd2(fb, 'NETWORK');
    netRows(c).forEach((r, i) => {
      const y = L2_ROW0 + i * L2_PITCH, sel = i === ui.ncur;
      listRowLcd2(fb, y, 18, sel, r.name, fit(r.val, W - 16 - 12 - textW(r.name, 1)));
      if (sel) r.help.forEach((l, k) => { if (l) text(fb, l, 6, 202 + 18 * k, 1, 1); });
    });
    drawSignalRowLcd2(fb, L2_ROW0 + netRows(c).length * L2_PITCH, c.rssi);
    if (c.net.mode === 0) drawAirRowsLcd2(fb, L2_ROW0 + (netRows(c).length + 1) * L2_PITCH, c);
  }
  function drawScanLcd2(fb, ui, c) {
    const list = c.net.scan, rows = L2_ROWS - 1;
    titleBarLcd2(fb, 'NETWORK', list.length ? (ui.scur + 1) + '/' + list.length : '');
    if (!list.length) { textC(fb, 'NO NETWORKS', 112, 1, 1); return; }
    const top = clamp(ui.scur - Math.floor(rows / 2), 0, Math.max(0, list.length - rows));
    for (let k = 0; k < rows && top + k < list.length; k++) {
      const i = top + k, n = list[i], y = L2_ROW0 + k * L2_PITCH, sel = i === ui.scur, v = sel ? 0 : 1;
      if (sel) rect(fb, 0, y - 2, W, 18, 1);
      text(fb, (n.name === c.net.ssid ? '*' : ' ') + fit(n.name, W - 80), 6, y, 1, v);
      for (let b = 0; b < 3; b++) { const h = 4 + 4 * b; if (b < n.bars) rect(fb, W - 32 + 8 * b, y + 14 - h, 6, h, v); else rect(fb, W - 32 + 8 * b, y + 12, 6, 2, v); }
    }
  }
  function drawTextLcd2(fb, ui, c) {
    const t = ui.txt, n = t.text.length, cells = 25, start = Math.max(0, t.pos - (cells - 1));
    titleBarLcd2(fb, t.field === 'apname' ? 'AP NAME' : 'PASSKEY', String(n));
    for (let k = 0; k < cells; k++) {
      const i = start + k; if (i > n) break;
      const x = 10 + k * 12, sel = i === t.pos, real = i < n ? t.text[i] : ' ';
      if (sel) { rect(fb, x - 2, 34, 14, 22, 1); text(fb, real, x, 38, 1, 0); }
      else text(fb, t.mask && i < n ? '*' : real, x, 38, 1, 1);
    }
    textC(fb, 'YES NEXT  NO DEL', 92, 1, 1);
    textC(fb, 'easier: web UI', 116, 1, 1);
    textC(fb, c.net.mode === 2 ? '192.168.4.1' : 'jank.local', 140, 1, 1);
  }
  function drawNetInfoLcd2(fb, ui, c) {
    const n = c.net;
    titleBarLcd2(fb, n.mode === 1 ? 'LINK CLIENT' : 'LINK AP');
    const rows = n.mode === 1 ? ['STATE ' + n.status, 'IP ' + n.ip, 'SSID ' + fit(n.ssid || '-', W - 72)] : ['IP 192.168.4.1', 'CLIENTS ' + n.apClients, 'NAME ' + n.apName];
    rows.forEach((r, i) => text(fb, r, 8, 36 + i * 24, 1, 1));
  }
  function drawConfirmLcd2(fb, title, line1, line2) {
    titleBarLcd2(fb, title); textC(fb, line1, 68, 1, 1); textC(fb, line2, 92, 1, 1); textC(fb, 'YES      NO', 168, 2, 1);
  }
  // setup: the hotspot's QR code on a lit box (dark modules, so it scans), at the biggest module size that leaves the text its room
  function drawSetupLcd2(fb, ui, c) {
    if (ui.leave) { drawConfirmLcd2(fb, 'SETUP', 'LEAVE SETUP?', 'RUN AGAIN'); return; }
    const q = c.qr, textMin = textW('192.168.4.1', 1), pad = 6;
    const sc = [8, 6, 4].find(k => k * q.size + 2 * pad <= H - 2 && k * q.size + 2 * pad + 6 + textMin <= W) || 4, box = sc * q.size + 2 * pad;
    rect(fb, 0, 2, box, box, 1);
    for (let y = 0; y < q.size; y++) for (let x = 0; x < q.size; x++) if (q.mod[y][x]) rect(fb, pad + sc * x, 2 + pad + sc * y, sc, sc, 0);
    const tx = box + 6, a = c.net.setupName.slice(0, 11), b = c.net.setupName.slice(11);
    text(fb, 'SETUP MODE', tx, 8, 1, 1); text(fb, a, tx, 36, 1, 1); text(fb, b, tx, 56, 1, 1);
    text(fb, 'open', tx, 92, 1, 1); text(fb, '192.168.4.1', tx, 116, 1, 1); text(fb, 'NO: LEAVE', tx, 200, 1, 1);
  }
  const OVERLAY_BOX = { x: 30, y: 60, w: 260, h: 120 };
  function drawOverlaysLcd2(fb, ui, now) {
    const o = OVERLAY_BOX;
    if (ui.hint) {
      boxedLcd2(fb, o.x, o.y, o.w, o.h); untag(o.x, o.y, o.w, o.h);
      textC(fb, ui.hint.title, 76, 1, 0); textC(fb, ui.hint.up, 110, 1, 0); textC(fb, ui.hint.down, 138, 1, 0);
      return;
    }
    if (ui.overlay && now < ui.overlay.until) {
      boxedLcd2(fb, o.x, o.y, o.w, o.h); untag(o.x, o.y, o.w, o.h);
      const v = ui.overlay.value, sc = [4, 3, 2, 1].find(k => textW(v, k) <= o.w - 24) || 1;
      textC(fb, ui.overlay.title, 76, 1, 0); textC(fb, v, 132 - 7 * sc, sc, 0);
    }
  }
  function drawScreenLcd2(fb, ui, c, now) {
    fb.fill(0);
    if (c.boot) { textC(fb, 'BOOT', 72, 5, 1); textC(fb, 'BOOTLOADER', 180, 1, 1); return; }
    if (c.splash) { textC(fb, 'JANK', 44, 5, 1); textC(fb, 'MINI 2.2', 132, 3, 1); textC(fb, 'FW EMU', 200, 1, 1); return; }
    if (c.setup) drawSetupLcd2(fb, ui, c);
    else if (ui.screen === 'home') drawHomeLcd2(fb, c, now);
    else if (ui.screen === 'net') drawNetLcd2(fb, ui, c);
    else if (ui.screen === 'scan') drawScanLcd2(fb, ui, c);
    else if (ui.screen === 'text') drawTextLcd2(fb, ui, c);
    else if (ui.screen === 'netinfo') drawNetInfoLcd2(fb, ui, c);
    else if (ui.screen === 'confirm') drawConfirmLcd2(fb, 'SETUP', 'STOP AND START', 'SETUP?');
    else if (ui.screen === 'menu') drawMenuLcd2(fb, ui, c);
    else if (ui.screen === 'sub') drawSubLcd2(fb, ui, c);
    else if (ui.screen === 'info') drawInfoLcd2(fb, ui, c);
    else if (ui.screen === 'edit') drawEditLcd2(fb, ui, c);
    drawOverlaysLcd2(fb, ui, now);
    if (c.saved) { boxedLcd2(fb, 70, 84, 180, 68); untag(70, 84, 180, 68); textC(fb, 'SAVED', 104, 2, 0); }
  }

  // The grid is the layout's: SIZES[layout] (OLED.size(c.layout) tells a caller how big fb, and ink, must be: 128 x 64, or 320 x 240 for
  // 'lcd2'). ink: an optional Uint8Array of the same size, filled only for the lcd2 layout (see INK); the OLED draws without it.
  function draw(fb, ui, c, now, ink) {
    const lcd = c.layout === 'lcd2', size = sizeOf(c.layout);
    if (lcd && fb.length !== size.w * size.h) throw new Error('OLED.draw: the lcd2 layout needs a ' + size.w + ' x ' + size.h + ' frame buffer');
    if (lcd && ink && ink.length !== fb.length) throw new Error('OLED.draw: the ink plane must be as big as the frame buffer');
    W = size.w; H = size.h; fine = lcd; inkFb = fb; inkOut = ink && lcd ? ink : null;
    if (inkOut) inkOut.fill(0);
    try {
      if (lcd) drawScreenLcd2(fb, ui, c, now); else drawScreen(fb, ui, c, now);
      if (inkOut) for (let i = 0; i < fb.length; i++) if (fb[i] !== 1) inkOut[i] = 0;          // ink only on lit pixels (a tick XOR-ed through a fill unlights some)
    } finally { W = OLED_W; H = OLED_H; fine = false; inkFb = null; inkOut = null; }
  }
  function drawScreen(fb, ui, c, now) {
    fb.fill(0);
    if (c.boot) { textC(fb, 'BOOT', 14, 4, 1); textC(fb, 'BOOTLOADER', 48, 1, 1); return; }
    if (c.splash) { textC(fb, 'JANK', 8, 3, 1); textC(fb, 'MINI 2.2', 34, 2, 1); textC(fb, 'FW EMU', 54, 1, 1); return; }
    if (c.setup) drawSetup(fb, ui, c);
    else if (ui.screen === 'home') drawHome(fb, c, now);
    else if (ui.screen === 'net') drawNet(fb, ui, c);
    else if (ui.screen === 'scan') drawScan(fb, ui, c);
    else if (ui.screen === 'text') drawText(fb, ui, c);
    else if (ui.screen === 'netinfo') drawNetInfo(fb, ui, c);
    else if (ui.screen === 'confirm') drawConfirm(fb, 'SETUP', 'STOP AND START', 'SETUP?');
    else if (ui.screen === 'menu') drawMenu(fb, ui, c);
    else if (ui.screen === 'sub') drawSub(fb, ui, c);
    else if (ui.screen === 'info') drawInfo(fb, ui, c);
    else if (ui.screen === 'edit') drawEdit(fb, ui);
    drawOverlays(fb, ui, now);
    if (c.saved) { boxed(fb, 30, 22, 68, 22); textC(fb, 'SAVED', 29, 2, 0); }
  }

  return { W, H, SIZES, size: sizeOf, LCD2: L2, INK, VU, create, press, draw, DEFAULT_RATE, topBar, foldLabel, autoBarIndex, AUTOBAR_NAMES };
})();
