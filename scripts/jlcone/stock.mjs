// JLC's stock and library type for LCSC codes, from jlcpcb.com/partdetail (headless Chrome, no window).
// node stock.mjs C2906290 C318884 ...
import { chromium } from "playwright";

const browser = await chromium.launch({ channel: "chrome", headless: true });
const page = await browser.newPage();
for (const code of process.argv.slice(2)) {
  await page.goto(`https://jlcpcb.com/partdetail/${code}`, { waitUntil: "domcontentloaded", timeout: 60000 }).catch(() => {});
  await page.waitForTimeout(6000);
  const lines = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter(Boolean);
  const i = lines.findIndex((l) => /Unit Price/i.test(l));
  const at = (re) => { const k = lines.findIndex((l) => re.test(l)); return k >= 0 ? lines.slice(k, k + 2).join(" ") : ""; };
  const hit = [at(/^MFR\.Part/i), at(/^Package/i), at(/^Assembly Type/i), at(/^Description/i), lines.find((l) => /In Stock:|Out of Stock/i.test(l)) || "no stock line",
    ...(i >= 0 ? lines.slice(i + 1, i + 5) : ["no price table"])].filter(Boolean);
  console.log(`== ${code}: ${hit.slice(0, 18).join(" | ")}`);
}
await browser.close();
