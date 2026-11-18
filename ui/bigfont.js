/* The bitmaps of the colour layout (ui/oled.js, ctx.layout 'lcd2') drawn at the panel's own 320 x 240 pixels: the big tempo digits and the 2 x
   smoothing of the small 5 x 7 face and of the icons. No scaling at paint time: every pixel is one panel pixel.
   The digits are a rounded 7-segment face, 0-9, '.' and '-'. A text is digits, '.' and '-'; the digit right after a '.' is a smaller copy,
   bottom-aligned with the big ones. Two fonts of one face, made from a documented construction (the tables are built when this file loads, and
   scripts/gen_bigfont.js writes the same rows as const tables for the firmware, core/jank_bigfont_tables.c):
     NORMAL (AUDIO): big digits 52 columns x 70 rows (R 5 = a 10 px stroke), the point 10 x 10 on the bottom rows, the small digit 34 x 46 (R 3), GAP 6.
     LARGE (LINK): big digits 70 x 94 (R 7), the point 14 x 14, the small digit 46 x 62 (R 5), GAP 8. '120.0' is 3 x 70 + 14 + 46 + 4 x 8 = 302 wide.
   Every digit is made from the seven segments a-g (1 = b c, 7 = a b c, ...) as lines on a square skeleton one stroke-radius inside the cell
   (x R and W - R, y R, (H - 1) / 2 and H - R): a pixel is lit when its centre is within R of a line of the digit in the metric
   max(|dx|, |dy|, K (|dx| + |dy|)) with K 0.9, whose corner cut removes the pixels with |dx| + |dy| > R / K at a line's end.
   scale2x(rows) doubles a bitmap ('#' / '.' rows) with Scale2x (EPX): every pixel becomes 2 x 2, the diagonal steps are rounded. */
const BigFont = (() => {
  const SEG = { '0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc', '8': 'abcdefg', '9': 'abcdfg', '-': 'g' };
  const K = 0.9;
  function glyph(ch, W, H, R) {
    const xl = R, xr = W - R, yt = R, ym = (H - 1) / 2, yb = H - R;
    const L = { a: [xl, yt, xr, yt], g: [xl, ym, xr, ym], d: [xl, yb, xr, yb], f: [xl, yt, xl, ym], b: [xr, yt, xr, ym], e: [xl, ym, xl, yb], c: [xr, ym, xr, yb] };
    const rows = [];
    for (let y = 0; y < H; y++) {
      let s = '';
      for (let x = 0; x < W; x++) {
        const px = x + 0.5, py = y + 0.5;
        let lit = false;
        for (const k of SEG[ch]) {
          const [x0, y0, x1, y1] = L[k];
          const cx = Math.min(Math.max(px, Math.min(x0, x1)), Math.max(x0, x1)), cy = Math.min(Math.max(py, Math.min(y0, y1)), Math.max(y0, y1));
          const dx = Math.abs(px - cx), dy = Math.abs(py - cy);
          if (Math.max(dx, dy, K * (dx + dy)) <= R) lit = true;
        }
        s += lit ? '#' : '.';
      }
      rows.push(s);
    }
    return rows;
  }
  function dot(d, H) {                                              // a round-cornered square on the bottom rows
    const h = d / 2, rows = [];
    for (let y = 0; y < H; y++) {
      let r = '';
      for (let x = 0; x < d; x++) { const dx = Math.abs(x + 0.5 - h), dy = Math.abs(y + 0.5 - (H - h)); r += dy <= h && Math.max(dx, dy, K * (dx + dy)) <= h ? '#' : '.'; }
      rows.push(r);
    }
    return rows;
  }
  const font = (p) => {
    const glyphs = {}, small = {}, rows = p.H, smallRows = p.sH, gap = p.gap;
    for (const ch of '0123456789-') { glyphs[ch] = glyph(ch, p.W, p.H, p.R); if (ch !== '-') small[ch] = glyph(ch, p.sW, p.sH, p.sR); }
    glyphs['.'] = dot(p.dot, p.H);
    // the glyphs of a text with their top rows: a digit right after the '.' is the small one, on the big digits' baseline
    const layout = s => [...s].map((ch, i) => (i > 0 && s[i - 1] === '.' && small[ch] ? { rows: small[ch], dy: rows - smallRows } : { rows: glyphs[ch] || glyphs['-'], dy: 0 }));
    // the width of a text (digits, '.', '-'): the glyphs' widths and the gaps between them
    const width = s => Math.max(0, layout(s).reduce((w, g) => w + g.rows[0].length + gap, 0) - gap);
    // calls plot(x, y) for every lit pixel of the text with its top left at (x, y)
    function draw(plot, s, x, y) {
      let cx = x;
      for (const g of layout(s)) {
        g.rows.forEach((row, r) => { for (let c = 0; c < row.length; c++) if (row[c] === '#') plot(cx + c, y + g.dy + r); });
        cx += g.rows[0].length + gap;
      }
    }
    return { ROWS: rows, SMALL_ROWS: smallRows, GAP: gap, GLYPHS: glyphs, SMALL: small, width, draw };
  };
  // Scale2x (EPX) of rows of '#' / '.': 2w x 2h, every pixel a 2 x 2 block with the diagonal steps rounded
  function scale2x(rows) {
    const h = rows.length, w = rows[0].length, at = (c, r) => r >= 0 && r < h && c >= 0 && c < w && rows[r][c] === '#';
    const out = Array.from({ length: 2 * h }, () => Array(2 * w).fill('.'));
    for (let r = 0; r < h; r++) for (let c = 0; c < w; c++) {
      const P = at(c, r), A = at(c, r - 1), B = at(c + 1, r), C = at(c - 1, r), D = at(c, r + 1);
      const o = [P, P, P, P];                                         // 1 2 / 3 4
      if (C === A && C !== D && A !== B) o[0] = A;
      if (A === B && A !== C && B !== D) o[1] = B;
      if (D === C && D !== B && C !== A) o[2] = C;
      if (B === D && B !== A && D !== C) o[3] = D;
      [[0, 0], [1, 0], [0, 1], [1, 1]].forEach(([dx, dy], k) => { if (o[k]) out[2 * r + dy][2 * c + dx] = '#'; });
    }
    return out.map(r => r.join(''));
  }
  return { NORMAL: font({ W: 52, H: 70, R: 5, sW: 34, sH: 46, sR: 3, gap: 6, dot: 10 }), LARGE: font({ W: 70, H: 94, R: 7, sW: 46, sH: 62, sR: 5, gap: 8, dot: 14 }), scale2x };
})();
