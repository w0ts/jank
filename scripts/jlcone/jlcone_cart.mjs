// JLC cart (JLCONE on 9222, or HEADLESS=1): list the items, optionally remove the ones whose text matches a pattern.
// node jlcone_cart.mjs [REMOVE_REGEX]   -> cart.png and the items' text; with a regex, clicks that item's delete and confirms
import { open } from "./browser.mjs";
const [removeRe] = process.argv.slice(2);
const browser = await open();   // (JLCONE on 9222, or HEADLESS=1: Chrome with no window)
const page = browser.page;
page.setDefaultTimeout(60000);
try {
  await page.goto("https://cart.jlcpcb.com/shopcart/cart/", { waitUntil: "domcontentloaded" });
  await page.waitForTimeout(7000);
  const dump = async (label) => {
    const text = await page.evaluate(() => document.body.innerText);
    const lines = text.split("\n").map((l) => l.trim()).filter(Boolean);
    console.log("== " + label); console.log(lines.filter((l) => /mm|pcs|\$|Layer|Thickness|HASL|OSP|Qty|Delete|Remove|Total|Checkout|Gerber|\.zip/i.test(l)).slice(0, 60).join(" | "));
  };
  await dump("cart");
  await page.screenshot({ path: "cart.png", fullPage: true });
  if (removeRe) {
    // find the cart row holding the pattern, then its delete control
    const re = new RegExp(removeRe, "i");
    const rows = page.locator("[class*=cart], [class*=item], tr, li").filter({ hasText: re });
    const n = await rows.count();
    console.log("rows matching: " + n);
    let done = false;
    for (let i = n - 1; i >= 0 && !done; i--) {
      const row = rows.nth(i);
      const txt = (await row.innerText().catch(() => "")).replace(/\s+/g, " ");
      if (txt.length > 2000) continue;                   // a container, not a row
      const del = row.locator("button, a, i, span, [role=button]").filter({ hasText: /^(Delete|Remove|删除)$/i }).first();
      const icon = row.locator("[class*=delete], [class*=remove], [class*=trash], [title*=Delete], [aria-label*=Delete]").first();
      const target = (await del.count()) ? del : (await icon.count()) ? icon : null;
      if (!target) continue;
      console.log("removing row: " + txt.slice(0, 160));
      await target.click();
      await page.waitForTimeout(1500);
      const ok = page.getByRole("button", { name: /^(OK|Confirm|Yes|Delete)$/i }).first();
      if (await ok.isVisible().catch(() => false)) await ok.click();
      await page.waitForTimeout(4000);
      done = true;
    }
    if (!done) console.log("no delete control found for the pattern");
    await dump("cart after");
    await page.screenshot({ path: "cart-after.png", fullPage: true });
  }
} catch (e) {
  console.log("FAILED: " + e.message.split("\n")[0]);
  await page.screenshot({ path: "cart-fail.png", fullPage: true }).catch(() => {});
} finally {
  await browser.close();
}
