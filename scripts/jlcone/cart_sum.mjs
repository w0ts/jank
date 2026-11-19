import { chromium } from "playwright";
const browser = await chromium.connectOverCDP("http://127.0.0.1:9222");
const page = browser.contexts()[0].pages().find((p) => /jlcpcb\.com/.test(p.url()));
await page.goto("https://cart.jlcpcb.com/shopcart/cart/", { waitUntil: "domcontentloaded" });
await page.waitForTimeout(7000);
const all = page.locator("label, span, div").filter({ hasText: /^\s*All \(\d+\)\s*$/ }).first();
const box = page.locator("input[type=checkbox]").first();
if (!(await box.isChecked().catch(() => false))) { await box.check({ force: true }).catch(() => {}); await page.waitForTimeout(5000); }
const lines = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter(Boolean);
const i = lines.findIndex((l) => /^SUMMARY$/i.test(l));
console.log(lines.slice(i, i + 16).join(" | "));
const c = page.getByText(/^Coupons$/).first();
if (await c.count()) { await c.click().catch(() => {}); await page.waitForTimeout(3000);
  const l2 = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter((l) => /\$\d|coupon|over|PCBA|PCB|Use|Available|Unavailable/i.test(l));
  console.log("coupon panel: " + l2.slice(-30).join(" | ")); }
await page.screenshot({ path: "cart-sum.png", fullPage: true });
await browser.close();
