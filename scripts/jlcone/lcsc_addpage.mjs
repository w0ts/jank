// LCSC: add one part to the cart from its product page (visible Chrome), for parts the cart API refuses.
// node lcsc_addpage.mjs CODE QTY   Never pays.
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

const [code, qty = "1"] = process.argv.slice(2);
const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: false, viewport: null });
const page = ctx.pages()[0] || (await ctx.newPage());
try {
  await page.goto(`https://www.lcsc.com/product-detail/${code}.html`, { waitUntil: "domcontentloaded", timeout: 90000 });
  await page.waitForTimeout(10000);
  const box = page.locator("input[type=text], input[type=number]").filter({ hasNot: page.locator("[placeholder*=earch]") });
  let set = false;
  for (const b of await box.all()) {
    const ph = (await b.getAttribute("placeholder")) || "";
    if (/search/i.test(ph) || !(await b.isVisible())) continue;
    await b.fill(qty); set = true; break;
  }
  console.log(set ? "quantity set to " + qty : "no quantity box found");
  const add = page.getByRole("button", { name: /add to cart/i }).first();
  if (await add.count()) { await add.click(); await page.waitForTimeout(4000); console.log("clicked Add to Cart"); }
  else console.log("no Add to Cart button");
  const msg = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter((l) => /added|cart|minimum|multiple|error/i.test(l) && l.length < 120);
  console.log(msg.slice(0, 8).join(" | "));
  await page.screenshot({ path: `lcsc-add-${code}.png` }).catch(() => {});
} catch (e) { console.log("STOP: " + e.message.split("\n")[0]); }
await ctx.close();
