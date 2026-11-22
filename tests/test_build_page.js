// SPDX-License-Identifier: MIT
// The Build page (page/build.html, published as build.html): build the jank mini 3. Its tab in the site's header, the
// work-in-progress warning at the top, no noindex, the two ways to have the board made (two in-page tabs), every picture
// shown exists and every picture in page/img/mini3/ is shown, alt texts, the download links name files that exist on
// main, the hand-solder table equals fab/default/hand.csv, and the photos still to come are listed.
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const load = require('./load_html.js');
const root = path.join(__dirname, '..');
let n = 0;
const ok = (c, m) => { assert.ok(c, m); n++; };
const html = fs.readFileSync(path.join(root, 'page', 'build.html'), 'utf8');
ok(html === load(path.join(root, 'page', 'src', 'build.html')), 'page/build.html is the expansion of page/src/build.html: run make page');
ok(html.includes('<title>Jank · Build</title>'), 'title');
ok(!/name="robots"/.test(html) && !/noindex/.test(html), 'no noindex: the page is in the menu');

// the site's header: the Build tab, current, after Device and before GitHub (test_site.js checks the other pages carry it)
const nav = html.match(/<nav[^>]*aria-label="Jank pages"[^>]*>([\s\S]*?)<\/nav>/)[1];
const tabs = [...nav.matchAll(/<a href="([^"]+)"([^>]*)>([\s\S]*?)<\/a>/g)].map(m => ({ href: m[1], cur: /aria-current="page"/.test(m[2]), text: m[3] }));
ok(tabs.map(t => t.href).join() === './,install.html,device.html,build.html,https://github.com/w0ts/jank', 'tab links');
ok(tabs.filter(t => t.cur).map(t => t.text).join() === 'Build', 'Build is the current tab');

// the warning: the first thing after the header
const main = html.slice(html.indexOf('</header>', html.indexOf('<body>')) + 9);
const first = main.match(/<(\w+)([^>]*)>([\s\S]*?)<\/\1>/);
ok(/class="[^"]*\bwip\b/.test(first[2]) && /role="alert"/.test(first[2]), 'the work-in-progress box comes first, role alert');
ok(/work in progress/i.test(first[3]) && /not (been )?ordered/i.test(first[3]) && /not (been )?tested/i.test(first[3]) && /own risk/i.test(first[3]), 'the warning says: work in progress, not ordered, not tested, own risk');

// pictures
const IMG = path.join(root, 'page', 'img', 'mini3');
const imgs = [...html.matchAll(/<img\b([^>]*)>/g)].map(m => m[1]);
const shown = new Set();
for (const a of imgs) {
  const src = (a.match(/\bsrc="([^"]+)"/) || [])[1];
  const alt = (a.match(/\balt="([^"]*)"/) || [])[1];
  ok(src && alt && alt.length > 15, `img ${src}: an alt text`);
  if (!src.startsWith('img/mini3/')) continue;
  ok(fs.existsSync(path.join(root, 'page', src)), `${src} exists`);
  ok(/\bwidth="\d+"/.test(a) && /\bheight="\d+"/.test(a) && /loading="lazy"/.test(a), `${src}: width, height, lazy`);
  if (/\/jlc-/.test(src)) ok(/class="shot"/.test(a), `${src}: framed as a screenshot`);
  shown.add(path.basename(src));
}
for (const f of fs.readdirSync(IMG)) ok(shown.has(f), `page/img/mini3/${f} is shown on the page`);

// download links: raw / blob / tree on main name paths that exist in the repo
const GH = 'https://github.com/w0ts/jank/';
const links = [...html.matchAll(/href="(https:\/\/github\.com\/w0ts\/jank\/(?:raw|blob|tree)\/main\/[^"#]*)"/g)].map(m => m[1]);
ok(links.length > 20, `links to the repo's files (${links.length})`);
for (const l of links) {
  const rel = decodeURIComponent(l.slice(GH.length).replace(/^(raw|blob|tree)\/main\//, ''));
  ok(fs.existsSync(path.join(root, rel)), `${l} names a file on main`);
}
const raw = new Set(links.filter(l => l.includes('/raw/main/')).map(l => l.slice((GH + 'raw/main/').length)));
for (const f of ['fab/full/gerbers.zip', 'fab/full/bom.csv', 'fab/full/cpl.csv', 'fab/default/gerbers.zip', 'fab/default/bom.csv',
  'fab/default/cpl.csv', 'fab/default/hand.csv', 'fab/default/lcsc-order.txt', 'strip/fab/bare/gerbers.zip',
  'strip/fab/assembled/gerbers.zip', 'strip/fab/assembled/bom.csv', 'strip/fab/assembled/cpl.csv',
  'case/box2-top-0.2.3mf', 'case/box2-bottom-cap-0.4.3mf']) ok(raw.has('hardware/jank-mini-3/' + f), `a download link to ${f}`);
// the case is box2 (top and bottom parts, screwed from below): its source is linked, the old box's files are not
ok(html.includes('hardware/jank-mini-3/case/box2.scad'), 'the case section links box2.scad');
ok(!/box-shell|box-lid|box\.scad/.test(html), 'no file of the old box (box.scad: shell and lid) is named');
ok(html.includes('href="https://www.waveshare.com/esp32-s3-lcd-2.htm"'), "the LCD-2 links Waveshare's own page");

// the two ways: an in-page tab list with two tabs, each with its panel and its folder's files
const tablist = html.match(/<div[^>]*role="tablist"[^>]*>([\s\S]*?)<\/div>/);
ok(tablist, 'an in-page tab list');
const ways = [...tablist[1].matchAll(/<button[^>]*role="tab"[^>]*aria-controls="([^"]+)"[^>]*>([\s\S]*?)<\/button>/g)].map(m => ({ id: m[1], text: m[2] }));
ok(ways.map(w => w.text).join('|') === 'Fully assembled by JLCPCB|USB-C + through-hole by hand', 'the two tabs, in order');
const panel = id => { const i = html.indexOf(`id="${id}"`); ok(i > 0 && /role="tabpanel"/.test(html.slice(html.lastIndexOf('<', i), i + 200)), `panel ${id}`); return html.slice(i, html.indexOf('<!-- /panel -->', i)); };
const [full, hand] = ways.map(w => panel(w.id));
ok(full.includes('fab/full/gerbers.zip') && !full.includes('fab/default/'), 'the full panel uses fab/full/ only');
ok(hand.includes('fab/default/gerbers.zip') && hand.includes('fab/default/hand.csv') && hand.includes('lcsc-order.txt') && !hand.includes('fab/full/'), 'the hand panel uses fab/default/, hand.csv and lcsc-order.txt');
ok(/Standard/.test(full) && /Both Sides/.test(full) && full.includes('jlc-assembly-full.webp'), 'full: Standard, both sides');
ok(/Economic/.test(hand) && /Top Side/.test(hand) && hand.includes('jlc-assembly-default.webp'), 'hand: Economic, top side');

// the hand-solder table equals fab/default/hand.csv (its part lines)
const split = line => {   // one CSV line: fields, quotes removed
  const out = []; let cur = '', q = false;
  for (const ch of line) {
    if (ch === '"') q = !q; else if (ch === ',' && !q) { out.push(cur); cur = ''; } else cur += ch;
  }
  return [...out, cur];
};
const csv = fs.readFileSync(path.join(root, 'hardware/jank-mini-3/fab/default/hand.csv'), 'utf8').trim().split(/\r?\n/).slice(1).map(split);
const parts = csv.filter(r => r[3]).map(r => [r[0], r[3], r[4]].join(' '));
const table = hand.match(/<table class="hand">([\s\S]*?)<\/table>/);
ok(table, 'the hand panel has the hand-solder table');
const rows = [...table[1].matchAll(/<tr><td>([^<]*)<\/td><td>[^<]*<\/td><td>([^<]*)<\/td><td>([^<]*)<\/td><\/tr>/g)].map(m => [m[1], m[2], m[3]].join(' '));
ok(rows.join('|') === parts.join('|'), `the table lists hand.csv's parts: ${rows.length} rows`);

// tabs work by script: the second panel starts hidden, the first selected
ok(/aria-selected="true"[^>]*aria-controls="way-full"/.test(tablist[1]) || /aria-controls="way-full"[^>]*aria-selected="true"/.test(tablist[1]), 'the full tab starts selected');
ok(new RegExp(`id="${ways[1].id}"[^>]*hidden`).test(html), 'the hand panel starts hidden');

// photos still to come: marked boxes, listed by scripts/photo_todo.py (make page prints them)
const todo = [...html.matchAll(/class="msg PHOTO-TODO"[^>]*>Photo to come: ([^<]*)</g)].map(m => m[1]);
ok(todo.length >= 8, `photos to come: ${todo.length}`);
const listed = execFileSync('python3', [path.join(root, 'scripts', 'photo_todo.py'), path.join(root, 'page', 'build.html')], { encoding: 'utf8' }).trim().split('\n').slice(1).map(l => l.replace(/^\s*-\s*/, ''));
ok(listed.join('|') === todo.join('|'), 'scripts/photo_todo.py lists the same photos');

// published
const script = fs.readFileSync(path.join(root, 'scripts', 'publish_page.sh'), 'utf8');
ok(script.includes('build.html:page/build.html'), 'publish_page.sh puts page/build.html at build.html');
console.log(`build page: ${n} checks passed (${todo.length} photos to come)`);
