// LCSC cart in a VISIBLE Chrome (LCSC refuses headless browsers; mots, 2026-11-19: "use a real not headless browser"),
// on the quote profile (out/chrome-profile), through LCSC's own cart API from an lcsc.com page. Never pays.
//   node lcsc_cart.mjs login                 a window at LCSC; mots signs in, then closes the tab (the sign-in is kept)
//   node lcsc_cart.mjs list
//   node lcsc_cart.mjs add CODE:QTY [CODE:QTY ...]
//   node lcsc_cart.mjs del CODE [CODE ...]
// Endpoints (from LCSC's page scripts, 2026-11-15): /wmsc/cart/product/index (GET), /wmsc/cart/add/batch (POST JSON
// [{productCode, quantity}]), /wmsc/cart/delete (POST form uuid=<uuid,uuid>).
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

const [cmd = "list", ...args] = process.argv.slice(2);
const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: false, viewport: null });
const page = ctx.pages()[0] || (await ctx.newPage());
try {
  await page.goto(cmd === "login" ? "https://www.lcsc.com/login" : "https://www.lcsc.com/cart", { waitUntil: "domcontentloaded", timeout: 90000 });
  if (cmd === "login") {
    console.log("window open: sign in to LCSC, then close the tab");
    await new Promise((done) => page.on("close", done));
  } else {
    await page.waitForTimeout(5000);
    const out = await page.evaluate(async ({ cmd, args }) => {
      const base = "https://wmsc.lcsc.com/wmsc";
      const H = { Accept: "application/json" };
      const get = async () => (await fetch(`${base}/cart/product/index`, { credentials: "include", headers: H })).json();
      const log = [];
      const first = await get();
      if (!first || !first.result) return "no cart from LCSC (signed in?): " + JSON.stringify(first).slice(0, 200);
      if (cmd === "add") {
        const body = args.map((a) => { const [productCode, q] = a.split(":"); return { productCode, quantity: Number(q) }; });
        const r = await fetch(`${base}/cart/add/batch`, { method: "POST", credentials: "include", headers: { ...H, "Content-Type": "application/json" }, body: JSON.stringify(body) });
        log.push(`add: HTTP ${r.status} code ${(await r.json()).code}`);
      }
      if (cmd === "del") {
        const L = first.result.cartProductList || [];
        const uuids = args.map((c) => (L.find((i) => i.productCode === c) || {}).uuid).filter(Boolean);
        if (uuids.length !== args.length) log.push("not in the cart: " + args.filter((c) => !L.some((i) => i.productCode === c)).join(" "));
        if (uuids.length) {
          const r = await fetch(`${base}/cart/delete`, { method: "POST", credentials: "include", headers: { ...H, "Content-Type": "application/x-www-form-urlencoded" }, body: "uuid=" + uuids.join(",") });
          log.push(`del: HTTP ${r.status} code ${(await r.json()).code}`);
        }
      }
      const L = ((await get()).result || {}).cartProductList || [];
      log.push(...L.map((i) => `${i.productCode} x${i.productQuantity} ${i.productModel || i.productMpn || ""}`));
      return log.join("\n");
    }, { cmd, args });
    console.log(out);
  }
} catch (e) {
  console.log("STOP: " + e.message.split("\n")[0]);
} finally {
  await ctx.close().catch(() => {});
}
