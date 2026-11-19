// The JLC cart's items and totals as text (HEADLESS=1: Chrome with no window, the quote profile). Never pays.
// node cart_read.mjs
import { open } from "./browser.mjs";

const browser = await open();
const page = browser.page;
try {
  await page.goto("https://cart.jlcpcb.com/shopcart/cart", { waitUntil: "domcontentloaded", timeout: 90000 });
  await page.waitForTimeout(9000);
  const lines = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter(Boolean);
  const keep = lines.filter((l) => /\$|PCB|PCBA|Qty|Quantity|gerber|sd-board|lcd2|strip|Total|Subtotal|Order|item|Layers|x\d/i.test(l) && l.length < 160);
  console.log(keep.slice(0, 80).join("\n"));
  await page.screenshot({ path: "cart-now.png", fullPage: true, timeout: 15000 }).catch(() => {});
} catch (e) { console.log("STOP: " + e.message); }
await browser.close();
