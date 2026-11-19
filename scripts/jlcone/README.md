# JLC quote and cart scripts

Quote a board at JLCPCB, save it to the cart, read or tidy the cart. They drive **Google Chrome headless**
(`HEADLESS=1`, no window: nothing comes to the front) or JLCONE over its debugging port 9222. **They never pay**:
mots checks the previews, picks shipping and pays.

Moved here from `worktrees/jank-scratch/quote` (2026-11-19). JLCONE pulls itself to the front while driven (mots:
"stop stealing focus"), so use `HEADLESS=1`; `start.sh` is only for the JLCONE route.

```sh
cd scripts/jlcone && npm install          # once (Playwright; it uses the installed Google Chrome)
node signin.mjs                           # once: a Chrome window on the quote profile; mots signs in to JLCPCB, closes it
mkdir -p out && cd out                    # screenshots land in the current folder
HEADLESS=1 node ../jlcone_pcbonly.mjs ZIP - - NAME top PCBQTY                       # bare board (SAVE=1: into the cart)
HEADLESS=1 node ../jlcone_pcba.mjs ZIP BOM CPL NAME top|both PCBQTY PCBAQTY [save]  # with assembly
HEADLESS=1 node ../cart_read.mjs          # the cart's items
HEADLESS=1 node ../jlcone_cart.mjs REGEX  # remove the cart item matching REGEX (e.g. its "Y17-..." order number)
node ../stock.mjs C7095263 ...            # JLC's stock, package, assembly type and prices for LCSC codes
```

Options (environment) for `jlcone_pcba.mjs`:
- `both` = Standard PCBA, both sides; `STANDARD=1` = Standard on a top-side board (a "Standard only" part).
- `SELECT_ALL=1` ticks the BOM rows JLC leaves for the customer (e.g. "difficult to process" parts).
- `DESC="musical instruments>Electronic Music Instrument"`: the customs description JLC needs before saving.
- `CONFIRM_FILE=1`: "Confirm Production file: Yes" (costs ~$1; JLC then waits up to 48 h for your confirmation).
- A part JLC would drop ("Do not place") stops the run and prints the reason (`WHY:`); `ALLOW_DNP=1` accepts it.

JLC traps the scripts handle: the cookie banner covers the PCBA type buttons (declined first); one BOM line per LCSC
code (JLC leaves two lines matched to one part unselected); JLC assembles nothing under 10 x 10 mm; the quote page
is sometimes slow (a timeout: run again). LCSC refuses headless browsers (`lcsc_probe.mjs` shows it): for LCSC, paste
a `code,qty` list into its Quick Order (e.g. `hardware/jank-mini-3/fab/default/lcsc-order.txt`). `cart_sum.mjs` still
needs JLCONE on 9222.
