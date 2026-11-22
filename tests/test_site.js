// SPDX-License-Identifier: MIT
// The site's pages (page/index.html About, page/install.html Install, page/jank.html Device): the same header on each, tabs in
// order, the open one marked, the built files equal their templates, every relative link and image resolves.
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const load = require('./load_html.js');
const root = path.join(__dirname, '..');
const ok = (c, m) => { assert.ok(c, m); n++; };
let n = 0;
const PAGES = [['index.html', 'About', 'src/index.html'], ['install.html', 'Install', 'src/install.html'], ['jank.html', 'Device', 'src/jank.html']];
const published = { 'index.html': 'index.html', 'install.html': 'install.html', 'device.html': 'jank.html' };
const navOf = h => h.match(/<nav[^>]*aria-label="Jank pages"[^>]*>([\s\S]*?)<\/nav>/)[1];
const tabsOf = h => [...navOf(h).matchAll(/<a href="([^"]+)"([^>]*)>([\s\S]*?)<\/a>/g)].map(m => ({ href: m[1], cur: /aria-current="page"/.test(m[2]), text: m[3] }));
for (const [file, name, src] of PAGES) {
  const html = fs.readFileSync(path.join(root, 'page', file), 'utf8');
  ok(html === load(path.join(root, 'page', src)), `page/${file} is the expansion of page/${src}: run make page`);
  ok(html.includes(`<title>Jank · ${name}</title>`), `${file}: title`);
  ok(/<div class="titlebar"><h1><a href="https:\/\/w0ts.github.io\/">w0ts<\/a> \/ Jank<\/h1><\/div>/.test(html), `${file}: title bar`);
  const tabs = tabsOf(html);
  ok(tabs.map(t => t.href).join() === './,install.html,device.html,https://github.com/w0ts/jank', `${file}: tab links, GitHub last`);
  ok(tabs.filter(t => t.cur).map(t => t.text).join() === name, `${file}: only ${name} is the current tab`);
  ok(/--accent: #9770c6/.test(html), `${file}: Jank's accent`);
  ok(!/<a [^>]*href="[^"]*"[^>]*target=/.test(navOf(html)), `${file}: tabs open in the same tab`);
}
// what is published: every relative link and image of each page names a file the publish script puts there
for (const [pub, file] of Object.entries(published)) {
  const html = fs.readFileSync(path.join(root, 'page', file), 'utf8');
  for (const m of html.matchAll(/(?:href|src)="([^"#:]*)(?:#[^"]*)?"/g)) {
    const t = m[1];
    if (!t || t.startsWith('data') ) continue;
    const there = t === './' ? 'index.html' : t;
    ok(Object.keys(published).includes(there) || fs.existsSync(path.join(root, 'page', there)), `${pub}: ${t} resolves`);
  }
}
const script = fs.readFileSync(path.join(root, 'scripts', 'publish_page.sh'), 'utf8');
for (const [pub, file] of Object.entries(published)) ok(script.includes(`${pub}:page/${file}`), `publish_page.sh puts page/${file} at ${pub}`);
console.log(`site: ${n} checks passed`);
