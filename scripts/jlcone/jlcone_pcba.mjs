// JLCONE (CDP 9222): quote a board WITH assembly, up to the quote page (and the cart on request). Never pays.
// node jlcone_pcba.mjs ZIP BOM CPL NAME SIDES(top|both) PCBQTY PCBAQTY [save]

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

  await page.goto("https://cart.jlcpcb.com/quote", { waitUntil: "domcontentloaded", timeout: 90000 });
  await page.waitForTimeout(6000);
  // the cookie banner sits over the PCBA type buttons: decline the non-essential cookies first (2026-11-19)
  const rej = page.getByRole("button", { name: /^\s*Reject All\s*$/i }).first();
  if (await rej.isVisible().catch(() => false)) { await rej.click(); await page.waitForTimeout(1000); say("cookies: Reject All"); }
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
  const toggle = page.locator("text=PCB Assembly").locator("xpath=ancestor::*[self::div or self::section][1]").locator("[role=switch], .el-switch, input[type=checkbox], [class*=switch]").first();
  if (await toggle.count()) { await toggle.click(); await page.waitForTimeout(3000); say("assembly on"); }
  if (process.env.CONFIRM_FILE) {   // ("Confirm Production file: Yes": mots sees JLC's CAM result before production)
    const row = page.locator(".main-btn-group").filter({ has: page.locator("label", { hasText: /Confirm Production file/i }) }).first();
    const yes = row.getByRole("button", { name: /^\s*Yes\s*$/ }).first();
    if (await yes.count()) { await yes.click({ force: true }); await page.waitForTimeout(1500); say("Confirm Production file: Yes"); } else say("Confirm Production file control not found");
  }
  if (sides === "both") { await pick(/^Standard$/, "Standard PCBA"); await pick(/^Both Sides$/, "Both Sides"); }
  else if (process.env.STANDARD) await pick(/^Standard$/, "Standard PCBA (top side)");   // Standard-only parts on a top-side board
  if (pcbaQty !== "5") {
    const row = page.locator(".main-btn-group").filter({ has: page.locator("label", { hasText: /^\s*PCBA Qty/ }) }).first();
    const b = row.getByRole("button", { name: new RegExp("^\\s*" + pcbaQty + "\\s*$") }).first();
    if (await b.count()) { await b.click(); await page.waitForTimeout(1500); say("PCBA qty " + pcbaQty); }
    else { const custom = row.locator("input").first(); if (await custom.count()) { await custom.fill(pcbaQty); await custom.press("Enter").catch(() => {}); await page.waitForTimeout(1500); say("PCBA custom qty " + pcbaQty); } else say("PCBA qty control not found"); }
  }
  const agree = page.locator("text=I agree to the Terms").first();
  if (await agree.count()) { const box = agree.locator("xpath=preceding::input[@type='checkbox'][1]"); if (await box.count() && !(await box.isChecked().catch(() => true))) await box.check({ force: true }).catch(async () => { await agree.click(); }); }
  const L1 = await lines(); const a = L1.findIndex((l) => /^PCBA Type/i.test(l)); say("assembly: " + L1.slice(a, a + 12).join(" | "));
  say("price panel: " + ((await lines()).find((l) => /(Economic|Standard) PCBA Price/i.test(l)) || "?"));
  await shot("s1");
  await page.locator("button:visible").filter({ hasText: /^\s*NEXT\s*$/i }).first().click(); await page.waitForTimeout(8000);
  say("url: " + page.url());
  // PCB tab -> NEXT -> Bill of Materials tab
  const next2 = page.getByRole("button", { name: /^NEXT$/i }).first();
  if (await next2.isVisible().catch(() => false)) { await next2.click(); await page.waitForTimeout(5000); }
  await chooseFile(/Add BOM File/i, bom, "BOM");
  await chooseFile(/Add CPL File/i, cpl, "CPL");
  await shot("s2");
  const proc = page.getByRole("button", { name: /Process BOM/i }).first();
  await proc.waitFor({ state: "visible" }); await proc.click(); await page.waitForTimeout(20000);
  const L3 = await lines(); say("parts page: " + L3.filter((l) => /Extended|Basic|Preferred|Selected|Total|\$|Unit|Not found|No Part|missing|match|Confirm|NEXT/i.test(l)).slice(0, 40).join(" | "));
  await shot("s3");
  // SELECT_ALL=1: tick the rows JLC left unselected (it does so for some parts it can place), then report what it says
  if (process.env.SELECT_ALL) {
    const boxes = await page.locator("tr input[type=checkbox]:not(:checked)").all();
    for (const bx of boxes) {
      const row = (await bx.locator("xpath=ancestor::tr[1]").innerText().catch(() => "")).replace(/\s+/g, " ").slice(0, 30);
      const vis = bx.locator("xpath=ancestor::label[1]//span[contains(@class,'el-checkbox__inner')]").first();   // (the styled box, not the hidden input)
      if (await vis.count()) await vis.click({ force: true }).catch(() => {}); else await bx.check({ force: true }).catch(() => {});
      await page.waitForTimeout(1500);
      const msg = (await lines()).filter((l) => /not (be )?(placed|available|support)|unable|cannot|Standard only|stock|warning|confirm/i.test(l) && l.length < 200).slice(0, 4);
      say("ticked " + row + (msg.length ? " -> " + msg.join(" / ") : ""));
      const ok = page.getByRole("button", { name: /^(OK|Confirm|Yes|Continue|Got it)$/i }).first();
      if (await ok.isVisible().catch(() => false)) { await ok.click(); await page.waitForTimeout(1000); }
    }
    say("still unselected: " + (await page.locator("tr input[type=checkbox]:not(:checked)").count()));
  }
  // any confirmation dialog, then NEXT to placements, NEXT to quote
  for (let k = 0; k < 3; k++) {
    const dnp = page.getByRole("button", { name: /^Do not place$/i }).first();
    if (await dnp.isVisible().catch(() => false) && !process.env.ALLOW_DNP) {
      // never drop a part silently (the review, 2026-11-09): report the dialog and the refused lines, then stop
      const dlg = await page.evaluate(() => { const b = [...document.querySelectorAll("button")].find((e) => /Do not place/i.test(e.textContent));
        let r = b; for (let k = 0; k < 6 && r; k++) r = r.parentElement; return r ? r.innerText.replace(/\s+/g, " ").slice(0, 1500) : ""; });
      say("REFUSED (dialog): " + dlg);
      const rows = await page.evaluate(() => [...document.querySelectorAll("tr")].map((t) => t.innerText.replace(/\s+/g, " "))
        .filter((t) => /out of stock|shortfall|not.{0,12}(available|found|match)|insufficient|discontinued|unavailable|0\s*$|warn/i.test(t)).slice(0, 30));
      say("REFUSED rows: " + rows.join(" || "));
      // the rows JLC left unselected (their checkbox off) and the warning text next to them (2026-11-19)
      const off = await page.evaluate(() => [...document.querySelectorAll("tr")].filter((t) => { const c = t.querySelector("input[type=checkbox]"); return c && !c.checked && t.querySelectorAll("td").length > 4; })
        .map((t) => { const w = [...t.querySelectorAll("[class*=warn], [class*=tip], [title]")].map((e) => e.getAttribute("title") || e.getAttribute("data-content") || "").filter(Boolean).join(" ");
          return t.innerText.replace(/\s+/g, " ").slice(0, 200) + (w ? " [" + w + "]" : ""); }));
      say("UNSELECTED: " + off.join(" || "));
      for (const t of await page.locator("tr").filter({ has: page.locator("input[type=checkbox]:not(:checked)") }).all()) {
        const name = (await t.innerText()).replace(/\s+/g, " ").slice(0, 24);
        const icons = await t.locator("svg, i, img, [class*=icon], [class*=warn]").all();
        const seen = new Set();
        for (const ic of icons) {
          await ic.hover({ force: true, timeout: 2000 }).catch(() => {}); await page.waitForTimeout(700);
          const tip = await page.evaluate(() => [...document.querySelectorAll("[role=tooltip], [class*=tooltip], [class*=popper], [class*=popover]")].filter((e) => e.offsetParent !== null && (e.innerText || "").trim()).map((e) => (e.innerText || "").replace(/\s+/g, " ").trim()).join(" / "));
          if (tip && !seen.has(tip)) { seen.add(tip); say("  why " + name + ": " + tip.slice(0, 400)); }
        }
      }
      const fs = await import("node:fs");
      const why = await page.evaluate(() => [...document.querySelectorAll("tr")].filter((t) => { const c = t.querySelector("input[type=checkbox]"); return c && !c.checked && t.querySelectorAll("td").length > 4; })
        .map((t) => t.innerText.replace(/\s+/g, " ").slice(0, 14) + ": " + [...t.querySelectorAll(".el-popover__reference")].map((r) => { const id = r.getAttribute("aria-describedby"); const p = id && document.getElementById(id); return p ? (p.textContent || "").replace(/\s+/g, " ").trim() : ""; }).filter(Boolean).join(" / ")));
      say("WHY: " + why.join(" || "));
      fs.writeFileSync(name + "-unselected.html", await page.evaluate(() => [...document.querySelectorAll("tr")].filter((t) => { const c = t.querySelector("input[type=checkbox]"); return c && !c.checked && t.querySelectorAll("td").length > 4; }).map((t) => t.outerHTML).join("\n\n")));
      await shot("refused");
      throw new Error("a part would be dropped ('Do not place'): see REFUSED above and " + name + "-refused.png; ALLOW_DNP=1 to accept");
    }
    const ok = page.getByRole("button", { name: /^(OK|Confirm|Yes|Continue|Got it)$/i }).first();
    if (await ok.isVisible().catch(() => false)) { say("dialog: " + (await ok.innerText())); await ok.click(); await page.waitForTimeout(3000); }
    const nx = page.getByRole("button", { name: /^NEXT$/i }).first();
    if (!(await nx.isVisible().catch(() => false))) break;
    await nx.click(); await page.waitForTimeout(12000);
    const Lk = await lines();
    say("tab " + k + ": " + Lk.filter((l) => /\$|Total|Components|Setup|Assembly|PCB|Quantity|Qty|Save to Cart|SAVE TO CART|Extended|Hand|fee/i.test(l)).slice(0, 40).join(" | "));
    await shot("s4-" + k);
    if (Lk.some((l) => /Save to Cart|SAVE TO CART/i.test(l)) && /Quote/i.test(Lk.join(" "))) break;
  }
  say("final url: " + page.url());
  // the customs "Product Description" select, required before Save to Cart
  const dsel = page.locator("input.el-input__inner[placeholder='Select']").first();
  if (await dsel.count()) {
    await dsel.click(); await page.waitForTimeout(2500);
    await shot("desc");
    const opts = await page.evaluate(() => [...document.querySelectorAll("li, .el-cascader-node, [class*=option], [class*=select-dropdown] span")].filter((e) => e.offsetParent !== null && e.textContent.trim().length > 0 && e.textContent.trim().length < 60).map((e) => e.textContent.trim()).filter((t, i, a) => a.indexOf(t) === i));
    say("descriptions: " + opts.join(" | "));
    if (process.env.LISTDESC) {
      for (const cat of ["Audio and Video Appliance", "musical instruments", "Reserch", "Smart Product"]) {
        const c = page.locator("li:visible").filter({ hasText: new RegExp(cat, "i") }).first();
        if (!(await c.count())) { say("no category " + cat); continue; }
        await c.hover(); await page.waitForTimeout(600); await c.click().catch(() => {}); await page.waitForTimeout(1200);
        const subs = await page.evaluate(() => [...document.querySelectorAll("li, [class*=item], [class*=node]")].filter((e) => e.offsetParent !== null && /HS Code/.test(e.textContent) && e.textContent.length < 90).map((e) => e.textContent.trim()).filter((t, i, a) => a.indexOf(t) === i));
        say("[" + cat + "] " + subs.join(" | "));
      }
    }
    if (process.env.DESC) {
      const [catRe, itemRe] = process.env.DESC.split(">").map((x) => new RegExp(x.trim(), "i"));
      if (itemRe) { const c = page.locator("li:visible").filter({ hasText: catRe }).first(); await c.hover(); await page.waitForTimeout(600); await c.click().catch(() => {}); await page.waitForTimeout(1200); const it = page.locator("li:visible, [class*=item]:visible, [class*=node]:visible").filter({ hasText: itemRe }).filter({ hasText: /HS Code/ }).first(); if (await it.count()) { await it.click(); await page.waitForTimeout(1500); say("picked item"); } else say("item not found: " + itemRe); }
      const re = itemRe || catRe;
      const val = async () => (await dsel.inputValue().catch(() => "")) || "";
      const it = page.locator("li:visible").filter({ hasText: re }).filter({ hasNot: page.locator("li") }).first();
      if (await it.count()) { await it.click(); await page.waitForTimeout(1500); }
      if (!re.test(await val())) { await page.keyboard.press("Escape"); await dsel.click(); await page.waitForTimeout(1200); const it2 = page.locator("li:visible").filter({ hasText: re }).first(); if (await it2.count()) { await it2.evaluate((e) => { e.dispatchEvent(new MouseEvent("mousedown", { bubbles: true })); e.dispatchEvent(new MouseEvent("mouseup", { bubbles: true })); e.click(); }); await page.waitForTimeout(1500); } }
      if (!re.test(await val())) { await dsel.fill(process.env.DESC.split(" ")[0]).catch(() => {}); await page.waitForTimeout(1200); await page.keyboard.press("ArrowDown"); await page.keyboard.press("Enter"); await page.waitForTimeout(1500); }
      say("description now: '" + (await val()) + "'");
      await shot("desc2");
    }
    else await page.keyboard.press("Escape");
  } else {
    say("no description select");
    const html = await page.evaluate(() => {
      const ins = [...document.querySelectorAll("input")].filter((e) => e.offsetParent !== null).map((e) => `<input placeholder="${e.placeholder}" class="${e.className}" value="${e.value}">`);
      const err = [...document.querySelectorAll("*")].find((e) => e.children.length === 0 && /Please select a production description/.test(e.textContent));
      let r = err; for (let k = 0; k < 2 && r; k++) r = r.parentElement;
      return "inputs: " + ins.join(" ") + "\nerr parent: " + (r ? r.outerHTML.replace(/\s+/g, " ").slice(0, 1500) : "no error text (maybe not yet shown)");
    });
    say("desc html: " + html);
  }
  if (save) { await page.keyboard.press("Escape").catch(() => {}); await page.waitForTimeout(800); const sv = page.locator("button, div, span, a").filter({ hasText: /^\s*SAVE TO CART\s*$/i }).last(); if (await sv.count()) { await sv.click(); await page.waitForTimeout(5000);
      if (process.env.BAKE_OK) { const bk = page.locator(".el-dialog:visible, [role=dialog]:visible").filter({ hasText: /Baking Required/ }).getByRole("button", { name: /^\s*Confirm\s*$/ }).first(); if (await bk.count()) { say("baking: " + ((await lines()).find((l) => /baking cost/i.test(l)) || "?")); await bk.click(); await page.waitForTimeout(2000); await sv.click(); await page.waitForTimeout(3000); } }
      await page.waitForTimeout(5000); say("after save: " + page.url()); const warn = (await lines()).filter((l) => /Please select|required|error/i.test(l)); say(warn.length ? "warnings: " + warn.join(" | ") : "no warning shown"); await shot("cart"); } }
} catch (e) {
  say("STOP: " + e.message.split("\n")[0]);
  await shot("stop").catch(() => {});
} finally {
  await browser.close();
}
