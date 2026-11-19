// JLCONE (CDP 9222): quote a bare board (no assembly), up to the price lines. Never pays, never brings the window forward.
// node jlcone_pcbonly.mjs ZIP - - NAME top PCBQTY

const [zip, bom, cpl, name, sides = "top", pcbQty = "5", pcbaQty = "5", save = ""] = process.argv.slice(2);
import { open } from "./browser.mjs";
const browser = await open();   // (JLCONE on 9222, or HEADLESS=1: Chrome with no window)
const page = browser.page;
page.setDefaultTimeout(30000);
const say = (s) => console.log(s);
const lines = async () => (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter(Boolean);
const shot = (s) => page.screenshot({ path: `${name}-${s}.png`, fullPage: true, timeout: 8000 }).catch((e) => say("no screenshot " + s + " (window hidden?)"));
const pick = async (re, what) => {      // a button whose name matches
  const el = page.getByRole("button", { name: re }).first();
  if (await el.count()) { await el.click({ force: true }); await page.waitForTimeout(1500); say("picked " + what); return true; }
  say("not found: " + what); return false;
};
const chooseFile = async (buttonRe, file, what) => {
  const btn = page.getByText(buttonRe).first();
  await btn.waitFor({ state: "visible" });
  const [fc] = await Promise.all([page.waitForEvent("filechooser", { timeout: 20000 }), btn.click()]);
  await fc.setFiles(file);
  await page.waitForTimeout(3000);
  say("uploaded " + what);
};
try {
  await page.goto("https://cart.jlcpcb.com/quote", { waitUntil: "domcontentloaded" });
  await page.waitForTimeout(6000);
  await page.locator('input[type="file"]').first().setInputFiles(zip);
  for (let i = 0; i < 90; i++) { await page.waitForTimeout(1000); if ((await lines()).some((l) => /^Detected/.test(l))) break; }
  say("== " + name + ": " + ((await lines()).find((l) => /^Detected/.test(l)) || "no detection"));
  await pick(/^LeadFree HASL$/, "LeadFree HASL");
  // PCB quantity: the "PCB Qty" row opens a popover of buttons (5, 10, 15, ...)
  const qtyRow = page.locator(".main-btn-group").filter({ has: page.locator("label", { hasText: /^\s*PCB Qty/ }) }).first();
  if (pcbQty !== "5" && (await qtyRow.count())) {
    say("qty: opening");
    await qtyRow.evaluate((row) => { const els = [...row.querySelectorAll("*")].filter((e) => e.offsetParent !== null && e.children.length === 0 && e.textContent.trim() === "5" && !e.closest(".qty-select-list")); if (els[0]) els[0].click(); else row.querySelector(".formgroup").click(); });
    await page.waitForTimeout(1500);
    const b = page.locator(".qty-select-list button:visible").filter({ hasText: new RegExp("^\\s*" + pcbQty + "\\s*$") }).first();
    if (await b.count()) { await b.click({ timeout: 10000 }); await page.waitForTimeout(2500); say("PCB Qty " + pcbQty); } else say("PCB Qty option not found (popover closed?)");
  }
  // assembly on
  const L = await lines();
  const i = L.findIndex((l) => /^Detected/.test(l));
  say("price lines: " + L.filter((l) => /\$|Layers|Build Time|Total|Weight/i.test(l)).slice(0, 30).join(" | "));
  await shot("pcbonly");
  if (process.env.SAVE) {   // (SAVE=1: the bare board into the cart; DESC "category>item" if JLC asks for a description)
    const rej = page.getByRole("button", { name: /^\s*Reject All\s*$/i }).first();
    if (await rej.isVisible().catch(() => false)) await rej.click();
    const sv = page.locator("button, div, span, a").filter({ hasText: /^\s*SAVE TO CART\s*$/i }).last();
    await sv.click(); await page.waitForTimeout(4000);
    const dsel = page.locator("input.el-input__inner[placeholder='Select']:visible").first();
    if (process.env.DESC && await dsel.count()) {
      const [catRe, itemRe] = process.env.DESC.split(">").map((x) => new RegExp(x.trim(), "i"));
      await dsel.click(); await page.waitForTimeout(1500);
      const c = page.locator("li:visible").filter({ hasText: catRe }).first(); await c.hover(); await c.click().catch(() => {}); await page.waitForTimeout(1200);
      const it = page.locator("li:visible").filter({ hasText: itemRe }).first(); if (await it.count()) await it.click();
      await page.waitForTimeout(1200); say("description now: '" + (await dsel.inputValue().catch(() => "")) + "'");
      const ok = page.getByRole("button", { name: /^\s*(Confirm|OK|SAVE TO CART)\s*$/i }).last();
      if (await ok.isVisible().catch(() => false)) { await ok.click(); await page.waitForTimeout(4000); }
    }
    say("after save: " + page.url());
    const warn = (await lines()).filter((l) => /Please select|required|error/i.test(l)); say(warn.length ? "warnings: " + warn.join(" | ") : "no warning shown");
    await shot("cart");
  }
} catch (e) { say("STOP: " + e.message); }
await browser.close().catch(() => {});
