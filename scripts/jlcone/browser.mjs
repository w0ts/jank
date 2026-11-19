// Where the quote scripts run: JLCONE over its debugging port (default), or, with HEADLESS=1, Google Chrome itself
// started headless (no window at all, so nothing comes to the front; mots, 2026-11-19). The headless profile lives in
// out/chrome-profile, so a JLC sign-in done there once is kept.
import { chromium } from "playwright";
import path from "node:path";
import { fileURLToPath } from "node:url";

export async function open() {
  if (process.env.HEADLESS) {
    const dir = path.join(path.dirname(fileURLToPath(import.meta.url)), "out", "chrome-profile");
    const ctx = await chromium.launchPersistentContext(dir, { channel: "chrome", headless: true, viewport: { width: 1400, height: 1000 } });
    const page = ctx.pages()[0] || (await ctx.newPage());
    return { page, close: () => ctx.close() };
  }
  const browser = await chromium.connectOverCDP("http://127.0.0.1:9222");
  const ctx = browser.contexts()[0];
  const page = ctx.pages().find((p) => /jlcpcb\.com/.test(p.url())) || ctx.pages()[0];
  return { page, close: () => browser.close() };
}
