// Is the headless quote profile signed in at LCSC, and what is in its LCSC cart? (HEADLESS=1). Never pays.
import { open } from "./browser.mjs";

const browser = await open();
const page = browser.page;
try {
  await page.goto("https://www.lcsc.com/cart", { waitUntil: "domcontentloaded", timeout: 90000 });
  await page.waitForTimeout(9000);
  const lines = (await page.evaluate(() => document.body.innerText)).split("\n").map((l) => l.trim()).filter(Boolean);
  console.log("url: " + page.url());
  console.log(lines.filter((l) => /sign in|log in|login|my account|hi,|cart|C\d{4,}|\$|empty|total/i.test(l) && l.length < 140).slice(0, 40).join("\n"));
} catch (e) { console.log("STOP: " + e.message); }
await browser.close();
