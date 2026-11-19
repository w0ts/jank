// LCSC search in a VISIBLE Chrome (LCSC refuses headless): prints the result rows' text. node lcsc_search.mjs "QUERY" [...]
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: false, viewport: null });
const page = ctx.pages()[0] || (await ctx.newPage());
try {
  for (const q of process.argv.slice(2)) {
    await page.goto("https://www.lcsc.com/search?q=" + encodeURIComponent(q), { waitUntil: "domcontentloaded", timeout: 90000 });
    await page.waitForTimeout(15000); await page.screenshot({ path: "lcsc-search.png", fullPage: false }).catch(() => {});
    const text = await page.evaluate(() => document.body.innerText);
    const lines = text.split("\n").map((l) => l.trim()).filter(Boolean);
    const rows = [];
    lines.forEach((l, i) => { if (/^C\d{4,}$/.test(l)) rows.push(lines.slice(Math.max(0, i - 3), i + 12).join(" | ")); });
    if (!rows.length) console.log("  (page start) " + lines.slice(0, 40).join(" | ").slice(0, 900));
    console.log("== " + q + ": " + rows.length + " rows");
    for (const r of rows.slice(0, 25)) console.log("  " + r.slice(0, 420));
  }
} catch (e) { console.log("STOP: " + e.message.split("\n")[0]); }
await ctx.close();
