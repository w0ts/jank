/* ====================================================================================================
   OLED UI for jank mini 2.2, screen version. 128 x 64, 1 bit per pixel, 5x7 pixel font (scaled x2/x3 for big text).
   Pure logic: no DOM. Input: press(ui, button, now_ms, ctx) with button 'up' | 'down' | 'no' (left arm) | 'yes'
   (right arm); output: draw(fb, ui, ctx, now_ms) fills a 128*64 Uint8Array (1 = white pixel). Written so it can be
   ported to the firmware as it is: one state struct, one input function, one draw function, a tiny font.

   ctx, read by the UI (the host fills it from /api/status and its own emulated state): link (mode is LINK; the host derives it from the line-in plug detect: plugged = AUDIO, unplugged = LINK), bpm
   (audio mode, after the half/double fold), linkBpm, mode, conf 0..1, th percent, levelDb (dBFS), silent, gated, hold,
   playing (Link transport), running / armed (the transport icon), fold (AUDIO: 1 2 4 0.5 0.25, the label X2 /2 ... top left; --xiao from the status's fold= / the home line's mode), foldAuto (AUTO RANGE set it), xiao (--xiao: YES sends play / stop in both modes), ble { mode 0 PAIR NOW | 1 ON START | 2 OFF, left (seconds of the pair window left, null = closed), phone (connected) }, peers, wifi ('good'|'weak'|'none'), beatN (integer beat or null), beatMs (ms since the beat), usb, fitted, batt percent,
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
                TRANSPORT ICON  triangle = running, square = stopped, blinking triangle = armed: a start asked for and
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
              NETWORK, USB, BLUETOOTH (PAIR NOW / ON START / OFF; YES on PAIR NOW opens the 2 minute pair window, which HOME's
              status line counts down), OFFSET, CLK RATE, DISPLAY, BATTERY (UP/DOWN move; the mode is not a setting: plugging in the line = AUDIO)
                YES on AUDIO / OFFSET -> SUB                    YES on NETWORK -> NET
                YES on CLK RATE / AUTO BAR / USB / BLUETOOTH -> EDIT              YES on BATTERY -> INFO       NO -> HOME
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
     INFO     read-only (battery: %, charging/discharging, estimated volt)  NO or YES -> MENU
     EDIT     (choices cycle round, numbers stop at their ends) the value drawn inverted (white box, black text). UP/DOWN change it (applied live), YES keeps it and
              returns to where EDIT was opened (MENU or SUB), NO restores the old value and returns.
   ==================================================================================================== */
const OLED = (() => {
  const OLED_W = 128, OLED_H = 64;                                   // the OLED boards' grid; the lcd2 layout has its own (SIZES)
  let W = OLED_W, H = OLED_H;                                        // the grid of the screen being drawn (set by draw(), per layout)
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
    '*': '.....|#.#.#|.###.|#####|.###.|#.#.#|.....',
  };
  const FONT = {};
  for (const k of Object.keys(FONT_SRC)) FONT[k] = FONT_SRC[k].split('|');
  const NARROW = { '.': 1, ':': 1, ',': 1 };                 // glyphs drawn from column 1, two columns wide
  const adv = ch => (NARROW[ch] ? 3 : ch === ' ' ? 4 : 6);

  const px = (fb, x, y, v) => { if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] = v; };
  const rect = (fb, x, y, w, h, v) => { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) px(fb, x + i, y + j, v); };
  const frame = (fb, x, y, w, h, v) => { rect(fb, x, y, w, 1, v); rect(fb, x, y + h - 1, w, 1, v); rect(fb, x, y, 1, h, v); rect(fb, x + w - 1, y, 1, h, v); };
  const xorCol = (fb, x, y0, y1) => { for (let y = y0; y <= y1; y++) if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] ^= 1; };
  const textW = (s, sc) => { let w = 0; for (const ch of s) w += adv(ch) * sc; return Math.max(0, w - sc); };
  function text(fb, s, x, y, sc, v) {
    let cx = x;
    for (const ch of String(s)) {
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
  const LT_SHORT = ['IGNORE', 'FOLLOW', 'LEAD'];
  // the HOME top bar's LINK-mode transport icons, 1-bit rows ('#' = lit): IGNORE = a crossed circle (our own start only),
  // FOLLOW = an arrow coming in (the session's start / stop), FOLLOW+LEAD = a double arrow (in and out)
  const LT_ICONS = [
    ['..###..', '.#...##', '#...#.#', '#..#..#', '#.#...#', '##...#.', '..###..'],
    ['...#.....', '..##.....', '.#######.', '########.', '.#######.', '..##.....', '...#.....'],
    ['.........', '..#...#..', '.##...##.', '#########', '.##...##.', '..#...#..', '.........'],
  ];
  const NET_ICON_XR = 86;                                            // the network icon's right edge: a gap before the plug
  const NET_ICON_XR_BLE = 82;                                        // ... with the Bluetooth rune beside it (4 columns further left)
  const BLE_ICON_X = 86, PLUG_X = 92, BATT_X = 107;                  // the rune (5 wide) sits between the network icon and the plug
  // the Bluetooth rune, 5 x 7: hidden while no window is open and no phone is connected, blinking (ARMED_BLINK_MS) while the
  // pair window is open, solid while a phone is connected
  const BLE_ICON = ['..#..', '..##.', '#.#.#', '.###.', '#.#.#', '..##.', '..#..'];
  const BLE_SHORT = ['PAIR', 'START', 'OFF'];
  const AUTOBAR_NAMES = ['OFF', 'AT START', 'ALWAYS'], AUTOBAR_SHORT = ['OFF', 'START', 'ALWAYS'];   // index = dev.autoBar (the old true / false = 1 / 0)
  const autoBarIndex = v => (v === true ? 1 : v === false || v == null ? 0 : v);
  // the fold's label on HOME (AUDIO): x2 x4 /2 /4, nothing at 1
  const foldLabel = k => (k === 2 ? 'X2' : k === 4 ? 'X4' : k === 0.5 ? '/2' : k === 0.25 ? '/4' : '');
  const BLE_WINDOW_S = 120;                                          // the pair window: 2 minutes
  const mmss = s => { const t = Math.max(0, Math.ceil(s)); return Math.floor(t / 60) + ':' + String(t % 60).padStart(2, '0'); };
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
    // the phone's Bluetooth: PAIR NOW = on only for a pair window (YES opens it, even unchanged), ON START = a window at
    // every start too, OFF = never. The triple NO opens the window as well (HOME, no screen needed)
    ble: { title: 'BLUETOOTH', opts: ['PAIR NOW', 'ON START', 'OFF'], get: c => c.ble.mode, live: false, repick: true,
           help: [['YES OPENS A 2 MIN', 'WINDOW FOR A PHONE'], ['A 2 MIN WINDOW AT', 'EVERY POWER ON'], ['NO BLUETOOTH', 'AT ALL']] },
    // the USB port's role; MIDI and USB audio work in both (host: battery units, the gear has its own power)
    usbrole: { title: 'USB', opts: ['DEVICE', 'HOST'], get: c => c.usbRole,
               help: [['TO A COMPUTER', 'MIDI + AUDIO'], ['TO SELF-POWERED', 'MIDI + AUDIO GEAR']] },
    // the MIDI clock over USB-MIDI (to the computer, or to the gear in HOST), M2d's usbmidi; the MIDI outs always have it
    usbmidi: { title: 'USB MIDI CLOCK', opts: ['ON', 'OFF'], get: c => c.usbMidi,
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
    { name: 'AUDIO', sub: 'audio' }, { name: 'LINK TRANSPORT', edit: 'ltmode' }, { name: 'AUTO BAR', edit: 'autobar' }, { name: 'NETWORK', net: true }, { name: 'USB', edit: 'usbrole' }, { name: 'BLUETOOTH', edit: 'ble' }, { name: 'USB MIDI CLOCK', edit: 'usbmidi' }, { name: 'OFFSET', sub: 'offsets' },
    { name: 'CLK RATE', edit: 'rate' }, { name: 'DISPLAY', sub: 'display' }, { name: 'BATTERY', info: 'battery' },
  ];
  // sub-menus: a title and the EDITS they list
  const SUBS = { audio: { title: 'AUDIO', rows: ['insrc', 'gain', 'lowconf', 'threshold'] }, offsets: { title: 'OFFSET', rows: ['inoff', 'out0', 'out1', 'out2'] },
                 display: { title: 'DISPLAY', rows: ['bright', 'sleep'] } };
  const VISIBLE = 6;
  const OVERLAY_MS = 1000;
  const METER_MIN_DB = -90, SILENCE_DB = -60;
  const sgn = v => (v > 0 ? '+' : '') + v;
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

  const create = () => ({ screen: 'home', cursor: 0, top: 0, ocur: 0, ncur: 0, scur: 0, stop: 0, edit: null, txt: null, leave: false, overlay: null });
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
        ui.overlay = { until: now + OVERLAY_MS, title: c.link ? 'LINK TEMPO' : (d > 0 ? 'DOUBLE' : 'HALF'), value: v == null ? '---' : v.toFixed(1) };
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
        else { ui.screen = 'info'; ui.page = it.info; }
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
    xorCol(fb, 2 + Math.round(123 * tickFrac), y - 1, y + 11);
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
  const SIGNAL_INK = { good: INK.GOOD, fair: INK.WARN, poor: INK.BAD, none: INK.DIM }, SIGNAL_BARS = { good: 3, fair: 2, poor: 1, none: 0 };
  // joined = green; alone = dim in AUDIO mode (a lone leader is normal), red in LINK mode (it has no tempo to follow)
  const linkInk = c => ((c.peers || 0) >= LINK_JOINED_MIN ? INK.GOOD : c.link ? INK.BAD : INK.DIM);
  // the HOME top bar's boxes (x ranges, inclusive; null = not drawn), so that nothing can overlap: the transport icon, in LINK
  // mode the transport option's icon, in AUDIO mode the fold label (X2 /2 ...), the network's name (right-aligned, truncated
  // to what is left), the network icon, the Bluetooth rune, the USB plug, the battery
  function topBar(c) {
    const m = c.net.mode, bleOn = !!(c.ble && (c.ble.phone || c.ble.left != null));
    const xr = bleOn ? NET_ICON_XR_BLE : NET_ICON_XR, netL = xr - [10, 7, 20][m];
    const lt = c.link ? LT_ICONS[c.ltMode] : null, fl = !c.link ? foldLabel(c.fold) : '';
    const left = lt ? { l: 12, r: 12 + lt[0].length - 1 } : fl ? { l: 12, r: 12 + textW(fl, 1) - 1 } : null;
    const nameLeft = left ? left.r + 5 : 10;
    const full = [c.net.group != null ? 'GRP ' + c.net.group : '', c.net.ssid, c.net.apName][m] || '';
    const name = full ? fit(full, netL - 2 - nameLeft) : '';
    return { tri: { l: 2, r: 8 }, left, label: fl, lt, name, nameBox: name ? { l: netL - 2 - textW(name, 1), r: netL - 3 } : null, nameR: netL - 2,
             net: { l: netL, r: xr }, xr, ble: bleOn ? { l: BLE_ICON_X, r: BLE_ICON_X + 4, solid: !!c.ble.phone } : null,
             usb: c.usb ? { l: PLUG_X, r: PLUG_X + USB_ICON[0].length - 1 } : null, batt: c.fitted ? { l: BATT_X, r: BATT_X + 16 } : null };
  }
  const BATT_BAD_PCT = 15, BATT_WARN_PCT = 30;                       // the battery icon: red under 15 %, amber under 30 %, else green
  function drawPower(fb, c, now) {
    const t = topBar(c);
    drawNetIcon(fb, c, t.xr);
    if (t.ble && (t.ble.solid || Math.floor(now / ARMED_BLINK_MS) % 2 === 0)) icon(fb, BLE_ICON, t.ble.l, 0);
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
    if (c.ble && c.ble.phone) items.push({ t: 'PHONE', bad: false });
    if (c.ble && c.ble.left != null && !c.ble.phone) { text(fb, 'BT PAIR ' + mmss(c.ble.left), 2, y, 1, 1); return; }   // the open window: shown alone, its count runs
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
    drawPower(fb, c, now);
    // the transport: a triangle = playing, a blinking triangle = started and waiting (locking / next bar), a square = stopped
    const tri = !stopped || (c.armed && Math.floor(now / ARMED_BLINK_MS) % 2 === 0);
    if (tri) for (let r = 0; r < 7; r++) rect(fb, 2, r, Math.min(r, 6 - r) + 1, 1, 1);
    else if (!c.armed) rect(fb, 2, 1, 5, 5, 1);
    // the network's name, right-aligned against the network icon (the Bluetooth rune moves both left): group (JANK), SSID
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
    const coast = !c.confHold && !!c.running && c.conf * 100 < c.th;
    valueBar(fb, 44, c.conf, c.th / 100, 'CF ' + Math.round(c.conf * 100) + '%' + (c.confHold ? ' HOLD' : coast ? ' COAST' : ''), false, !!c.confHold || coast);
    statusLine(fb, c, now, 57);
  }
  function drawMenu(fb, ui, c) {
    titleBar(fb, 'MENU', (ui.cursor + 1) + '/' + MENU.length);
    const vals = { 'LINK TRANSPORT': LT_SHORT[c.ltMode], 'AUTO BAR': AUTOBAR_SHORT[autoBarIndex(c.autoBar)], NETWORK: NET_SHORT[c.net.mode],
                   USB: EDITS.usbrole.opts[c.usbRole], 'USB MIDI CLOCK': EDITS.usbmidi.opts[c.usbMidi], BLUETOOTH: BLE_SHORT[c.ble.mode], 'CLK RATE': RATES_SHORT[c.rate], BATTERY: c.fitted ? c.batt + '%' : '-' };
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
  function drawInfo(fb, ui, c) {
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
  const fit = (s, w) => (s.length * 6 > w ? s.slice(0, Math.floor(w / 6)) : s);
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
  // the triple NO's pair window: a box with its countdown (the window's own seconds come from ctx.ble.left)
  const PAIR_OVERLAY_MS = 3000;
  function pairOverlay(ui, now) { ui.hint = null; ui.overlay = { until: now + PAIR_OVERLAY_MS, title: 'BLUETOOTH PAIR', value: '', live: 'pair' }; }
  function drawOverlays(fb, ui, now, c) {
    if (ui.hint) {                                                   // NO / YES held on HOME: what UP / DOWN will do
      boxed(fb, 6, 12, 116, 40);
      textC(fb, ui.hint.title, 17, 1, 0); textC(fb, ui.hint.up, 31, 1, 0); textC(fb, ui.hint.down, 42, 1, 0);
      return;
    }
    if (ui.overlay && now < ui.overlay.until) {
      boxed(fb, 14, 12, 100, 40);
      const v = ui.overlay.live === 'pair' ? mmss(c.ble && c.ble.left != null ? c.ble.left : BLE_WINDOW_S) : ui.overlay.value, sc = [3, 2, 1].find(k => textW(v, k) <= 96) || 1;   // the largest size that fits the box
      textC(fb, ui.overlay.title, 17, 1, 0); textC(fb, v, 38 - Math.round(7 * sc / 2), sc, 0);
    }
  }

  // ======================================================== LCD2 LAYOUT ========================================================
  // ctx.layout === 'lcd2' (the Waveshare ESP32-S3-LCD-2): its OWN logical grid of 160 x 120 pixels (4:3, SIZES.lcd2), the same 5 x 7 font
  // and primitives at integer scales, painted EXACTLY x2 on the 320 x 240 panel (ui/lcd2.js): no bands, no centring, square pixels.
  // The OLED boards keep 128 x 64 and never run this code. Colour is information only (INK, see tag()); menus and edit screens have none.
  //
  //   HOME (AUDIO)  y 0..12   top bar: transport icon, fold / LINK option icon, GRP n, link glyph + peers, signal bars, BLE rune, USB plug, battery
  //                 y 15..49  the TEMPO, font scale 5 (130 px wide for '120.0'), 'BPM' at its right
  //                 y 52..77  four big beat boxes, 36 x 26, gap 4, the beat indicator; lit = solid, unlit = 2 px outline; the first is amber only while lit
  //                 y 81      CF  87%  [line, notch at the stop threshold]   number and line coloured by the threshold rule
  //                 y 91      IN -20dB [line, notch at the tracker's -50 dBFS] neutral; red clipping, amber too quiet
  //                 y 101     BAR LOCKED [outlined bar]                      AUTO BAR state and its confidence, neutral; only with AUTO BAR on
  //                 y 111     status line: GATED / HOLD / COAST (left), STOP 70 / MIN 70 (right); or BT PAIR, PHONE, the Wi-Fi line
  //   HOME (LINK)   the same top bar, tempo and status line; the beat boxes 36 x 52 at y 54..105; no audio bars
  //   lists         title bar 11 px, 12 rows of 9 px from y 13; sub menus and info 12 px rows; edit: the value at scale 2
  const SIZES = { oled: { w: OLED_W, h: OLED_H }, lcd2: { w: 160, h: 120 } };
  const sizeOf = layout => (layout === 'lcd2' ? SIZES.lcd2 : SIZES.oled);
  const CONF_WARN_MARGIN = 10;                                       // amber from the stop threshold up to this many points above it
  const CLIP_PEAK_DB = -1, CLIP_RMS_MAX_DB = -3, QUIET_DB = -50;     // clipping: the last second's peak (or, unreported, the RMS maximum); quiet: under the tracker's -50 dBFS sound level
  function levelInk(c) {
    const clip = c.peakDb !== null && c.peakDb !== undefined ? c.peakDb >= CLIP_PEAK_DB : c.levelMax >= CLIP_RMS_MAX_DB;
    if (clip) return INK.BAD;
    return c.levelDb < QUIET_DB ? INK.WARN : INK.NONE;
  }
  function confInk(c) {
    if (c.confKnown === false || c.silent) return INK.DIM;
    const pct = Math.round(c.conf * 100);
    return pct < c.th ? INK.BAD : pct <= c.th + CONF_WARN_MARGIN ? INK.WARN : INK.GOOD;
  }
  const battInk = pct => (pct < BATT_BAD_PCT ? INK.BAD : pct < BATT_WARN_PCT ? INK.WARN : INK.GOOD);

  // the HOME geometry: one table, the tests read it
  const L2 = {
    topH: 13, left: 14, nameRight: 66, linkX: 70, signalX: 98, bleX: 112, plugX: 119, battX: 134, battW: 21,
    tempoY: 15, tempoScale: 5, bpmX: 138, bpmY: 43,
    boxX: 2, boxW: 36, boxGap: 4, boxY: 52, boxH: 26, linkBoxY: 54, linkBoxH: 52, boxBorder: 2,
    rowCF: 81, rowIN: 91, rowBAR: 101, labelX: 2, valueXR: 60, barX: 64, barW: 94, lineH: 3,
    statusY: 111,
  };
  const barFillWidth = frac => Math.round(L2.barW * clamp(frac, 0, 1));          // the line's length: the value on 94 px
  const barTickX = tickFrac => L2.barX + Math.round((L2.barW - 1) * tickFrac);

  function drawTopBarLcd2(fb, c, now) {
    const tri = c.running || (c.armed && Math.floor(now / ARMED_BLINK_MS) % 2 === 0);
    if (tri) for (let r = 0; r < 9; r++) rect(fb, 2, 2 + r, Math.ceil((Math.min(r, 8 - r) + 1) * 1.6), 1, 1);
    else if (!c.armed) rect(fb, 2, 3, 7, 7, 1);
    const lt = c.link ? LT_ICONS[c.ltMode] : null, fl = !c.link ? foldLabel(c.fold) : '';
    if (lt) icon(fb, lt, L2.left, 3); else if (fl) text(fb, fl, L2.left, 3, 1, 1);
    const nameLeft = lt ? L2.left + lt[0].length + 4 : fl ? L2.left + textW(fl, 1) + 4 : L2.left;
    const full = [c.net.group != null ? 'GRP ' + c.net.group : '', c.net.ssid, c.net.apName][c.net.mode] || '';
    if (full) textR(fb, fit(full, L2.nameRight - nameLeft), L2.nameRight, 3, 1, 1);
    drawJankGlyph(fb, L2.linkX, 2);                                      // the link glyph, and the Link peers beside it
    if ((c.peers || 0) > 0) text(fb, String(c.peers), L2.linkX + 12, 3, 1, 1);
    tag(linkInk(c), L2.linkX, 0, 24, L2.topH);
    const q = signalQuality(c.rssi), bars = SIGNAL_BARS[q];            // the air: three bars, 3 px wide, 3 / 6 / 9 px high
    for (let i = 0; i < 3; i++) { const h = 3 * (i + 1); if (i < bars) rect(fb, L2.signalX + 4 * i, 12 - h, 3, h, 1); else rect(fb, L2.signalX + 4 * i, 11, 3, 1, 1); }
    tag(SIGNAL_INK[q], L2.signalX, 0, 11, L2.topH);
    const bleOn = !!(c.ble && (c.ble.phone || c.ble.left != null));
    if (bleOn && (c.ble.phone || Math.floor(now / ARMED_BLINK_MS) % 2 === 0)) icon(fb, BLE_ICON, L2.bleX, 3);
    if (c.usb) icon(fb, USB_ICON, L2.plugX, 4);
    if (c.fitted) {
      frame(fb, L2.battX, 2, L2.battW, 9, 1); rect(fb, L2.battX + L2.battW, 4, 2, 5, 1);
      rect(fb, L2.battX + 2, 4, Math.round(17 * clamp(c.batt, 0, 100) / 100), 5, 1);
      tag(battInk(c.batt), L2.battX, 2, L2.battW + 2, 9);
    }
  }

  // a small progress row: label, the value right-aligned, and ONE thin line (3 px high, no outline, no track): its length is the value on
  // 94 px, the rest is background. A notch, 1 px wide and 5 px tall, is XOR-ed through the line at tickFrac when given (a gap in the line
  // where it passes, a stub above and below where it does not). The value, the line and the notch carry `ink`; the label stays neutral.
  function progressRow(fb, y, label, value, frac, tickFrac, ink) {
    text(fb, label, L2.labelX, y, 1, 1);
    text(fb, value, L2.valueXR - textW(value, 1), y, 1, 1);
    tag(ink, L2.valueXR - textW(value, 1), y, textW(value, 1), 7);
    const n = barFillWidth(frac);
    rect(fb, L2.barX, y + 2, n, L2.lineH, 1);
    tag(ink, L2.barX, y + 2, n, L2.lineH);
    if (tickFrac !== null && tickFrac !== undefined) { const x = barTickX(tickFrac); xorCol(fb, x, y + 1, y + 5); tag(ink, x, y + 1, 1, 5); }
  }
  // the BAR row keeps v4's outlined bar (94 x 5, a 3 px fill inside, so the fill is round(92 x value)); neutral, no tick
  function outlinedRow(fb, y, label, value, frac) {
    text(fb, label, L2.labelX, y, 1, 1);
    text(fb, value, L2.valueXR - textW(value, 1), y, 1, 1);
    frame(fb, L2.barX, y + 1, L2.barW, 5, 1);
    rect(fb, L2.barX + 1, y + 2, Math.round((L2.barW - 2) * clamp(frac, 0, 1)), 3, 1);
  }
  function beatBoxesLcd2(fb, c, y, h) {                                  // the beat indicator (no LEDs): the lit box solid, the others 2 px outlines; the first one is amber only while lit
    for (let i = 0; i < 4; i++) {
      const x = L2.boxX + i * (L2.boxW + L2.boxGap), on = c.beatN != null && ((c.beatN % 4) + 4) % 4 === i;
      if (on) rect(fb, x, y, L2.boxW, h, 1);
      else { frame(fb, x, y, L2.boxW, h, 1); frame(fb, x + 1, y + 1, L2.boxW - 2, h - 2, 1); }          // an unlit box: a 2 px outline
      if (i === 0 && on) tag(INK.ONE, x, y, L2.boxW, h);
    }
  }
  const BAR_WORDS = { locked: 'LOCKED', listening: 'LISTEN', manual: 'MANUAL' };
  // the bottom line: the confidence words on the left (AUDIO), else the Bluetooth window, the phone or the Wi-Fi line; the threshold on the right
  function drawStatusLcd2(fb, c, now) {
    const y = L2.statusY, coast = !c.confHold && c.confKnown !== false && !!c.running && c.conf * 100 < c.th;
    const word = c.link ? null : c.gated ? ['GATED', INK.BAD] : c.confHold ? ['HOLD', INK.DIM] : coast ? ['COAST', INK.WARN] : null;
    if (word) { text(fb, word[0], 2, y, 1, 1); tag(word[1], 2, y, textW(word[0], 1), 7); }
    else if (c.ble && c.ble.left != null && !c.ble.phone) text(fb, 'BT PAIR ' + mmss(c.ble.left), 2, y, 1, 1);
    else if (c.ble && c.ble.phone) text(fb, 'PHONE', 2, y, 1, 1);
    else {
      const item = wifiItems(c.wifiQ)[0];
      if (item) {
        let x = 2;
        for (const s of item.segs) {
          const w = textW(s.t, 1);
          if (s.bad) { rect(fb, x - 1, y - 1, w + 2, 9, 1); text(fb, s.t, x, y, 1, 0); tag(INK.BAD, x - 1, y - 1, w + 2, 9); } else text(fb, s.t, x, y, 1, 1);
          x += w + 6;
        }
      }
    }
    if (!c.link) textR(fb, c.confStop ? 'STOP ' + c.th : 'MIN ' + c.th, W - 3, y, 1, 1);
  }
  function drawHomeLcd2(fb, c, now) {
    drawTopBarLcd2(fb, c, now);
    const bpm = c.link ? c.linkBpm : c.bpm;
    text(fb, bpm ? bpm.toFixed(1) : '---', 2, L2.tempoY, L2.tempoScale, 1);
    text(fb, 'BPM', L2.bpmX, L2.bpmY, 1, 1);
    if (c.link) { beatBoxesLcd2(fb, c, L2.linkBoxY, L2.linkBoxH); drawStatusLcd2(fb, c, now); return; }
    beatBoxesLcd2(fb, c, L2.boxY, L2.boxH);
    const known = c.confKnown !== false, ci = confInk(c);
    progressRow(fb, L2.rowCF, 'CF', known ? Math.round(c.conf * 100) + '%' : '--', known ? c.conf : 0, c.th / 100, ci);
    progressRow(fb, L2.rowIN, 'IN', Math.round(c.levelDb) + 'dB', (c.levelDb - METER_MIN_DB) / -METER_MIN_DB, (QUIET_DB - METER_MIN_DB) / -METER_MIN_DB, levelInk(c));
    if (c.bar && BAR_WORDS[c.bar.state]) outlinedRow(fb, L2.rowBAR, 'BAR', BAR_WORDS[c.bar.state], c.bar.state === 'manual' ? 0 : c.bar.conf || 0);
    drawStatusLcd2(fb, c, now);
  }

  // ---- the lists: a title bar of 11 px, rows of 9 px from y 13 (12 rows: the whole MENU fits), full width, neutral, the selected row inverted
  const L2_ROW0 = 13, L2_PITCH = 9, L2_ROWS = 12;
  const titleBarLcd2 = (fb, title, right) => { rect(fb, 0, 0, W, 11, 1); text(fb, title, 3, 2, 1, 0); if (right) textR(fb, right, W - 3, 2, 1, 0); };
  function listRowLcd2(fb, y, h, sel, left, right) {
    const v = sel ? 0 : 1;
    if (sel) rect(fb, 0, y - 1, W, h, 1);
    text(fb, left, 4, y, 1, v);
    if (right) textR(fb, right, W - 4, y, 1, v);
  }
  const menuValues = c => ({ 'LINK TRANSPORT': LT_SHORT[c.ltMode], 'AUTO BAR': AUTOBAR_SHORT[autoBarIndex(c.autoBar)], NETWORK: NET_SHORT[c.net.mode],
    USB: EDITS.usbrole.opts[c.usbRole], 'USB MIDI CLOCK': EDITS.usbmidi.opts[c.usbMidi], BLUETOOTH: BLE_SHORT[c.ble.mode], 'CLK RATE': RATES_SHORT[c.rate], BATTERY: c.fitted ? c.batt + '%' : '-' });
  function drawMenuLcd2(fb, ui, c) {
    titleBarLcd2(fb, 'MENU', (ui.cursor + 1) + '/' + MENU.length);
    const vals = menuValues(c), top = clamp(ui.cursor - L2_ROWS + 1, 0, Math.max(0, MENU.length - L2_ROWS));
    for (let k = 0; k < L2_ROWS && top + k < MENU.length; k++) listRowLcd2(fb, L2_ROW0 + k * L2_PITCH, 9, top + k === ui.cursor, MENU[top + k].name, vals[MENU[top + k].name] || '');
  }
  function drawSubLcd2(fb, ui, c) {
    const sub = SUBS[ui.sub];
    titleBarLcd2(fb, sub.title);
    sub.rows.forEach((f, i) => {
      const sp = EDITS[f];
      listRowLcd2(fb, 18 + i * 12, 11, i === ui.ocur, sp.title, sp.opts ? (f === 'lowconf' ? (sp.get(c) ? 'STOP' : 'RUN') : sp.opts[sp.get(c)]) : editText(sp, sp.get(c)));
    });
  }
  function drawInfoLcd2(fb, ui, c) {
    titleBarLcd2(fb, 'BATTERY');
    const rows = [];
    if (!c.fitted) rows.push('NO CELL FITTED', 'POWER ' + (c.usb ? 'USB' : 'NONE'));
    else {
      const volt = 3.3 + 0.9 * c.batt / 100;
      rows.push('LEVEL ' + c.batt + '%', 'VOLT ' + volt.toFixed(2) + 'V EST', c.usb ? (c.batt < 100 ? 'CHARGING' : 'FULL') : 'DISCHARGING');
      bar(fb, 4, 92, W - 8, c.batt / 100, null);
    }
    rows.forEach((r, i) => text(fb, r, 4, 18 + i * 12, 1, 1));
  }
  function drawEditLcd2(fb, ui) {
    const e = ui.edit, sp = EDITS[e.field];
    titleBarLcd2(fb, sp.title);
    const s = editText(sp, e.val), sc = textW(s, 2) + 16 <= W - 4 ? 2 : 1;      // the value in the biggest of two sizes that fits
    const w = textW(s, sc) + 16, h = 7 * sc + 8, x = Math.round((W - w) / 2);
    rect(fb, x, 24, w, h, 1); text(fb, s, x + 8, 28, sc, 0);
    const [h1, h2] = helpOf(sp, e.val);
    textC(fb, h1, 70, 1, 1); textC(fb, h2, 82, 1, 1);
  }
  function drawNetLcd2(fb, ui, c) {
    titleBarLcd2(fb, 'NETWORK');
    netRows(c).forEach((r, i) => {
      const y = L2_ROW0 + i * L2_PITCH, sel = i === ui.ncur;
      listRowLcd2(fb, y, 9, sel, r.name, fit(r.val, W - 8 - 6 - textW(r.name, 1)));
      if (sel) r.help.forEach((l, k) => { if (l) text(fb, l, 3, 101 + 9 * k, 1, 1); });
    });
  }
  function drawScanLcd2(fb, ui, c) {
    const list = c.net.scan, rows = L2_ROWS - 1;
    titleBarLcd2(fb, 'NETWORK', list.length ? (ui.scur + 1) + '/' + list.length : '');
    if (!list.length) { textC(fb, 'NO NETWORKS', 56, 1, 1); return; }
    const top = clamp(ui.scur - Math.floor(rows / 2), 0, Math.max(0, list.length - rows));
    for (let k = 0; k < rows && top + k < list.length; k++) {
      const i = top + k, n = list[i], y = L2_ROW0 + k * L2_PITCH, sel = i === ui.scur, v = sel ? 0 : 1;
      if (sel) rect(fb, 0, y - 1, W, 9, 1);
      text(fb, (n.name === c.net.ssid ? '*' : ' ') + fit(n.name, W - 40), 3, y, 1, v);
      for (let b = 0; b < 3; b++) { const h = 2 + 2 * b; if (b < n.bars) rect(fb, W - 16 + 4 * b, y + 7 - h, 3, h, v); else px(fb, W - 15 + 4 * b, y + 6, v); }
    }
  }
  function drawTextLcd2(fb, ui, c) {
    const t = ui.txt, n = t.text.length, cells = 25, start = Math.max(0, t.pos - (cells - 1));
    titleBarLcd2(fb, t.field === 'apname' ? 'AP NAME' : 'PASSKEY', String(n));
    for (let k = 0; k < cells; k++) {
      const i = start + k; if (i > n) break;
      const x = 5 + k * 6, sel = i === t.pos, real = i < n ? t.text[i] : ' ';
      if (sel) { rect(fb, x - 1, 17, 7, 11, 1); text(fb, real, x, 19, 1, 0); }
      else text(fb, t.mask && i < n ? '*' : real, x, 19, 1, 1);
    }
    textC(fb, 'YES NEXT  NO DEL', 46, 1, 1);
    textC(fb, 'easier: web UI', 58, 1, 1);
    textC(fb, c.net.mode === 2 ? '192.168.4.1' : 'jank.local', 70, 1, 1);
  }
  function drawNetInfoLcd2(fb, ui, c) {
    const n = c.net;
    titleBarLcd2(fb, n.mode === 1 ? 'LINK CLIENT' : 'LINK AP');
    const rows = n.mode === 1 ? ['STATE ' + n.status, 'IP ' + n.ip, 'SSID ' + fit(n.ssid || '-', W - 36)] : ['IP 192.168.4.1', 'CLIENTS ' + n.apClients, 'NAME ' + n.apName];
    rows.forEach((r, i) => text(fb, r, 4, 18 + i * 12, 1, 1));
  }
  function drawConfirmLcd2(fb, title, line1, line2) {
    titleBarLcd2(fb, title); textC(fb, line1, 34, 1, 1); textC(fb, line2, 46, 1, 1); textC(fb, 'YES      NO', 84, 2, 1);
  }
  // setup: the hotspot's QR code on a lit box (dark modules, so it scans), at the biggest module size that leaves the text its room
  function drawSetupLcd2(fb, ui, c) {
    if (ui.leave) { drawConfirmLcd2(fb, 'SETUP', 'LEAVE SETUP?', 'RUN AGAIN'); return; }
    const q = c.qr, textMin = textW('192.168.4.1', 1), pad = 3;
    const sc = [4, 3, 2].find(k => k * q.size + 2 * pad <= H - 2 && k * q.size + 2 * pad + 3 + textMin <= W) || 2, box = sc * q.size + 2 * pad;
    rect(fb, 0, 1, box, box, 1);
    for (let y = 0; y < q.size; y++) for (let x = 0; x < q.size; x++) if (q.mod[y][x]) rect(fb, pad + sc * x, 1 + pad + sc * y, sc, sc, 0);
    const tx = box + 3, a = c.net.setupName.slice(0, 11), b = c.net.setupName.slice(11);
    text(fb, 'SETUP MODE', tx, 4, 1, 1); text(fb, a, tx, 18, 1, 1); text(fb, b, tx, 28, 1, 1);
    text(fb, 'open', tx, 46, 1, 1); text(fb, '192.168.4.1', tx, 58, 1, 1); text(fb, 'NO: LEAVE', tx, 100, 1, 1);
  }
  const OVERLAY_BOX = { x: 15, y: 30, w: 130, h: 60 };
  function drawOverlaysLcd2(fb, ui, now, c) {
    const o = OVERLAY_BOX;
    if (ui.hint) {
      boxed(fb, o.x, o.y, o.w, o.h); untag(o.x, o.y, o.w, o.h);
      textC(fb, ui.hint.title, 38, 1, 0); textC(fb, ui.hint.up, 55, 1, 0); textC(fb, ui.hint.down, 69, 1, 0);
      return;
    }
    if (ui.overlay && now < ui.overlay.until) {
      boxed(fb, o.x, o.y, o.w, o.h); untag(o.x, o.y, o.w, o.h);
      const v = ui.overlay.live === 'pair' ? mmss(c.ble && c.ble.left != null ? c.ble.left : BLE_WINDOW_S) : ui.overlay.value, sc = [4, 3, 2, 1].find(k => textW(v, k) <= o.w - 12) || 1;
      textC(fb, ui.overlay.title, 38, 1, 0); textC(fb, v, 66 - Math.round(7 * sc / 2), sc, 0);
    }
  }
  function drawScreenLcd2(fb, ui, c, now) {
    fb.fill(0);
    if (c.boot) { textC(fb, 'BOOT', 36, 5, 1); textC(fb, 'BOOTLOADER', 90, 1, 1); return; }
    if (c.splash) { textC(fb, 'JANK', 22, 5, 1); textC(fb, 'MINI 2.2', 66, 3, 1); textC(fb, 'FW EMU', 100, 1, 1); return; }
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
    else if (ui.screen === 'edit') drawEditLcd2(fb, ui);
    drawOverlaysLcd2(fb, ui, now, c);
    if (c.saved) { boxed(fb, 35, 42, 90, 34); untag(35, 42, 90, 34); textC(fb, 'SAVED', 52, 2, 0); }
  }

  // The grid is the layout's: SIZES[layout] (OLED.size(c.layout) tells a caller how big fb, and ink, must be: 128 x 64, or 160 x 120 for
  // 'lcd2'). ink: an optional Uint8Array of the same size, filled only for the lcd2 layout (see INK); the OLED draws without it.
  function draw(fb, ui, c, now, ink) {
    const lcd = c.layout === 'lcd2', size = sizeOf(c.layout);
    if (lcd && fb.length !== size.w * size.h) throw new Error('OLED.draw: the lcd2 layout needs a ' + size.w + ' x ' + size.h + ' frame buffer');
    if (lcd && ink && ink.length !== fb.length) throw new Error('OLED.draw: the ink plane must be as big as the frame buffer');
    W = size.w; H = size.h; inkFb = fb; inkOut = ink && lcd ? ink : null;
    if (inkOut) inkOut.fill(0);
    try {
      if (lcd) drawScreenLcd2(fb, ui, c, now); else drawScreen(fb, ui, c, now);
      if (inkOut) for (let i = 0; i < fb.length; i++) if (fb[i] !== 1) inkOut[i] = 0;          // ink only on lit pixels (a tick XOR-ed through a fill unlights some)
    } finally { W = OLED_W; H = OLED_H; inkFb = null; inkOut = null; }
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
    drawOverlays(fb, ui, now, c);
    if (c.saved) { boxed(fb, 30, 22, 68, 22); textC(fb, 'SAVED', 29, 2, 0); }
  }

  return { W, H, SIZES, size: sizeOf, LCD2: L2, INK, create, press, draw, pairOverlay, mmss, BLE_WINDOW_S, DEFAULT_RATE, topBar, foldLabel, autoBarIndex, AUTOBAR_NAMES };
})();
