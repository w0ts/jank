// LCSC: pictures of a part (visible Chrome): its product photo area and its datasheet viewer. node lcsc_show.mjs CODE
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

const [code] = process.argv.slice(2);
const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: false, viewport: { width: 1400, height: 1000 } });
const page = ctx.pages()[0] || (await ctx.newPage());
try {
  await page.goto(`https://www.lcsc.com/product-detail/${code}.html`, { waitUntil: "domcontentloaded", timeout: 90000 });
  await page.waitForTimeout(10000);
  await page.evaluate(() => window.scrollTo(0, 0));
  await page.waitForTimeout(1000);
  await page.screenshot({ path: `show-${code}-top.png` });
  const img = page.locator("img").filter({ has: page.locator("xpath=.") }).first();
  const big = await page.evaluate(() => { const i = [...document.images].filter((x) => x.naturalWidth >= 300).map((x) => x.src); return i.slice(0, 6); });
  console.log("images: " + big.join(" "));
  const ds = page.getByText(/^Datasheet$/).first();
  if (await ds.count()) { await ds.scrollIntoViewIfNeeded(); await page.waitForTimeout(6000); await page.screenshot({ path: `show-${code}-datasheet.png` }); }
  console.log("saved show-" + code + "-top.png / -datasheet.png");
} catch (e) { console.log("STOP: " + e.message.split("\n")[0]); }
await ctx.close();
