// One visible Chrome window on the headless quotes' profile (out/chrome-profile), at JLC's sign-in page: mots signs in
// and closes the window; the sign-in is kept for HEADLESS=1 runs. Claude never types the credentials.
// node signin.mjs
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: false, viewport: null });
const page = ctx.pages()[0] || (await ctx.newPage());
await page.goto("https://jlcpcb.com/", { waitUntil: "domcontentloaded" }).catch(() => {});
console.log("window open: sign in to JLCPCB, then close the window");
await new Promise((done) => ctx.on("close", done));
console.log("closed: the sign-in is saved in " + dir);
