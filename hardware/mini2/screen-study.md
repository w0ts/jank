# jank mini 2.2: a bigger screen? (study, 2026-11-14; no board file changed)

The user loved the emulator's 2.5x screen, about 54 x 27 mm. **Only a 2.42" panel is that size (55.0 x 27.5 mm).** A
1.3" is half of it in each direction.

Picture: `case/renders/screen-study.png`. Left: today's 66 x 64 board with the 0.96", 1.3" and 1.54" sitting on J10,
and the 2.42" dashed for scale. Right: the 2.42" on a 72 x 72 square board.

A second picture, `case/renders/screen-sizes-closed.png`, shows the closed case from above for the 0.96", 1.3" and
2.42", to scale. Each window shows the emulator's real HOME frame, scaled to that panel's active area. The frame comes
from the OLED code in `live/player.html`, run under node.

**V** = verified this session from the maker's drawing or sheet (sources at the end). **T** = typical, or an estimate
(not a quote, not measured).

## Numbers

| | 0.96" (now) | 1.3" | 1.54" | 2.42" |
|---|---|---|---|---|
| LCSC part (HanSheng) | HS96L03W2C03 C5248080 | HS13L03W2C01 C7465997 | HS154L03W2C01 C7465999 | HS242L03B2C01 C7466000 |
| controller | SSD1315 | SH1106 | SSD1309 | SSD1309 |
| module PCB, mm (V) | 27.3 x 27.8 x 2.65 | 35.4 x 33.5 x 2.6 | 42.4 x 38.0 x 2.65 | **72.0 x 43.0 x 4.2** (the drawing; its table says 68.00, which is the hole pitch) |
| active area, mm (V) | 21.74 x 10.86 | 29.42 x 14.70 | 35.05 x 17.52 | 55.01 x 27.49 |
| size vs the emulator (linear) | 0.40 | 0.54 | 0.64 | 1.0 |
| header (V) | 4 pins along the top edge, centred, 1.44 mm in | same, 2.0 mm in | same, ~2.5 mm in | **4 pins down the LEFT short side**, 2.5 mm in, pins 17.7 .. 25.3 mm from the top |
| pin order (V) | GND VCC SCL SDA | GND VCC SCL SDA | GND VCC SCL SDA | GND VCC SCL SDA (pin 1 at the bottom) |
| holes (V) | 4 at 23.3 x 23.8 | 4 x 3.0 at 30.4 x 28.5 | 4 x 3.0 at 37.4 x 33.0 | 4 x 3.0 .. 3.1 at 68 x 39 |
| module supply (V) | 2.8 .. 3.3 V | 3.0 .. 3.3 V | 3 .. 5 V (has its own boost) | 3 .. 5 V (has its own boost) |
| current with every pixel on (V) | 25.6 mA typ, 32 max | 45 mA typ, 50 max | panel 35.6 mA at 12.5 V | panel 31.7 mA at 13 V |
| ... drawn from 3.3 V | 25.6 mA | 45 mA | ~160 mA (T: boost assumed 85 % efficient) | ~150 mA (T); **Waveshare measured 196 mA** on, 7.3 mA all off (V) |
| typical screen, ~15 % of pixels lit (T) | ~5 mA | ~8 mA | ~28 mA | ~36 mA (25 % lit: ~54 mA) |
| one 128 x 64 frame over I2C at 400 kHz | ~24 ms, so at most ~41 frames/s for all four. Every controller tops out at 400 kHz (V: SH1106 fSCL, SSD1309 tcycle 2.5 us) | | | |
| LCSC price, 1 / 10 pieces (V, 2026-11-14) | 2.34 / 1.95 USD, 4933 in stock | 4.86 / 4.20, 895 | 6.97 / 5.99, 724 | 13.14 / 11.29, 114 |
| per board vs the 0.96" from LCSC | 0 | **+2.52 / +2.25** | +4.63 / +4.04 | **+10.80 / +9.34** |
| marketplace (T: typical, not quotes) | ~1.5 .. 2.5 USD | ~2.5 .. 4 | ~5 .. 7 | ~8 .. 13 (AliExpress); 20 .. 40 EUR in shops (Funduino 23.52 EUR: a search listing, not opened) |
| colours | white, blue (also yellow/blue split) | white, blue | white (LCSC) | LCSC I2C version: **colour unclear** (listing: blue; sheet: white; its CIE point 0.29 / 0.66 is green). LCSC white is SPI only (C5139769, 4 in stock). Marketplace: white, blue or yellow (QDtech MC242GW/B/Y, V); Waveshare: white, or yellow as the "(C)" version (V) |

## Battery (1S cell, 450 .. 550 mAh)

The device itself is taken as ~100 mA with Wi-Fi on (the README's assumed figure). Without a screen: 4.5 .. 5.5 h.

| screen content | 0.96" | 1.3" | 1.54" | 2.42" |
|---|---|---|---|---|
| typical UI (~15 % lit) | 4.3 .. 5.3 h | 4.2 .. 5.1 h | 3.5 .. 4.3 h | 3.3 .. 4.0 h (-26 %); 2.9 .. 3.6 h at 25 % lit |
| all white | 3.6 .. 4.4 h | 3.1 .. 3.8 h | ~1.7 .. 2.1 h | **1.5 .. 1.9 h** (Waveshare's 196 mA) |

The **HT7833 is rated 500 mA**. A Wi-Fi peak (340 mA, assumed) plus an all-white 2.42" (196 mA) is 536 mA, which is
over. So the firmware must cap the lit area times the contrast at about 100 mA. A dark UI (light text on black) and a
dimmer contrast both save current and battery life. The AMS1117 (USB order, 1 A) has room.

## CPU and driver

- **CPU cost (T, not measured):** the ESP32-S3's I2C controller works from a 32-byte FIFO (recall), so a full frame
  takes ~33 interrupts, roughly 0.2 ms of CPU. At 20 frames/s that is under 0.5 % of one core. The task waits for the
  bus without using the CPU. Run the display task at low priority on the core that does not run audio, and send only
  the 8-row pages that changed. All four modules cost the same: 1 KB per frame.
- **SH1106 (1.3"):** its RAM is 132 columns wide and the panel shows columns 2 .. 129, so every page starts at column
  2. It has page addressing only (no horizontal auto-increment), so the frame goes out as 8 page writes, each with its
  own address commands. Its charge-pump command differs from the SSD1306's.
- **SSD1309 (1.54", 2.42"):** uses the SSD1306 command set but has **no charge pump**. It needs 7 .. 16 V on VCC (V),
  which the module's own boost makes (QDtech uses an SX1308, Waveshare an AP3012: V, from their schematics). Before
  initialising, unlock with 0xFD 0x12 and set the VCOMH level. The 0x8D charge-pump command is not used.
- **SSD1315 (0.96"):** SSD1306-compatible.
- No driver exists in `firmware/` yet, so nothing has to be ported.
- **Pull-ups:** the modules carry their own 4.7 k pull-ups (QDtech, Waveshare: V). With R60 / R61 (10 k) in
  parallel the bus sees ~3.2 k, which is fine at 400 kHz.
- **Pin order:** SDA and SCL swapped on a module (Waveshare: DIN, CLK) is a firmware setting. The S3's GPIO matrix can
  put either signal on either pin, and both pull-ups are 10 k to +3V3.

## Fit

**Today's board (66 x 64, lid 11 mm inside), modules on J10:**
- **0.96":** as today.
- **1.3": fits as it is.**
  - Outline: x 17.3 .. 52.7, y 13.9 .. 47.4.
  - Its PCB stands 2.5 mm up on the header plastic. Everything under it is low: SOT-23, 0805, the QFN, the jacks'
    legs at 1.8 mm. Parts on the module's back are not drawn by the maker.
  - Clearances: 0.7 mm in plan to the toggle's body (that body starts at x 53.4 inside the lid); 1.55 mm to the UP
    boss; the D-pad's flange is 9 mm up, far above the glass at 5.1 mm.
  - The lid's window grows to ~31 x 16.5 mm.
- **1.54":** on J10 it hits the toggle's body and the UP boss. Moving J10 4 mm left (x 31.0) clears both, by 1.2 and
  2.05 mm.
- **2.42":** does not fit. It is wider than the case's inside (66.8 mm) and covers the D-pad.

**2.42" on a square board (the user allows 70 .. 72; picture, right):**
- **72 x 72 is needed; 70 x 70 is not enough.** The module is 72 mm wide (68 mm is its hole pitch, not its width),
  and the case's inside is the board + 0.8 mm. On 70 x 70 only Waveshare's narrow 2.42" fits (61.5 x 39.5 mm, V).
  But that one ships SPI with a 7-pin right-angle header. Changing it to I2C means moving two 0-ohm resistors, and its
  RES has no reset circuit on the module, so it needs a GPIO or an RC. Not recommended.
- **Module position:** across the top, y 1 .. 44, at full width. The active area spans x 8.5 .. 63.5, y 6.1 .. 33.6.
- **D-pad:** keeps its distance from the bottom-right corner; the cross's middle goes to (61.5, 62.2). Its hole starts
  at y 52.8, 8.8 mm below the module. A 20 x 20 mm cross fits with room to spare.
- **Height:** the module crosses the USB-C (3.2 mm tall), and parts on its back hang down up to 2.2 mm (V, QDtech).
  - Hung from the lid's roof, the glass sits 0.5 mm under the roof (10.5 mm over the board).
  - The bottom of its back parts is then 4.25 mm over the board: 1.05 mm over the USB-C.
  - The lid stays 11 mm deep inside.
- **Header vs DIN:** the module's header column lands at x 2.5, y 18.7 .. 26.3. That is right on top of J5's DIN body
  (J6's, if the module is turned round). So the header cannot plug into the board when the DINs are fitted. A 4-wire
  lead from the module to **J10 where it is today** solves this, and it takes any pin order.
- **What else must move:**
  - The **power toggle's body** (inside the lid, x 53.4 .. 66.4, y 13.6 .. 26.4, z 0.4 .. 7.2) sits under the module.
    Either move it to the left wall below the module (y ~46 .. 59, raised clear of the parts there), or make the
    screen lid 3.5 mm taller.
  - The lid's **two right-hand feet** (y 28.3, 37.2) sit under the module: move them below y 45.
  - The edge parts (jacks, DINs) move out with the new edges. A re-layout (routing) is needed.
- **Price:** the PCB costs the same at JLC (under 100 x 100). The case grows to ~75.6 x 75.6 mm, a little more PETG.

**One footprint for 4-pin and 7-pin modules:** possible. A 1 x 7 row wired as follows:

| pin | connection |
|---|---|
| 1 | GND or VCC, chosen by JP1 |
| 2 | VCC or GND, chosen by JP2 |
| 3 | SCL |
| 4 | SDA |
| 5 | RES: 10 k to +3V3 and 100 nF to GND, a power-on reset |
| 6 | DC to GND (I2C address 0x3C) |
| 7 | CS to GND |

It matches the common 7-pin order GND VCC SCL SDA RES DC CS (LCDWiki MSP242X, V; switched to I2C by fitting R4 and
R9 instead of R5). **It is not worth it:**
- a 7-pin module's header is centred on its 7 pins, so a header-mounted 1.3" 7-pin module sits 3.8 mm further right
  and hits the toggle;
- every LCSC module above is 4-pin I2C;
- the 2.42" goes on a lead anyway.

## Recommendation

**The 2.42" on a 72 x 72 board, the module held by the lid on a 4-wire lead to J10.** It is the only size the user
saw and liked. It costs **+9.34 .. +10.80 USD a board** (LCSC, against the 0.96") and a re-layout. The PCB price stays
the same. Battery life drops ~26 % with a typical screen.

Exact changes:
1. **Board 72 x 72.** The D-pad's middle goes to (61.5, 62.2), the same distance from the corner as now. The jacks and
   DINs follow the edges.
2. **J10, JP1, JP2, R60, R61, C60, C61: no change.** The lead runs from the module's GND VCC SCL SDA to J10, and the
   jumpers are set to GND-VCC as for the 0.96". The lead is a part the user supplies (~10 cm, 4 wires).
3. **Screen lid:**
   - window ~57.5 x 29.5 mm (the module's metal frame opening), centred on the active area at board y ~19.9;
   - the inner frame comes down to 0.5 mm over the glass, as now;
   - four bosses on the 68 x 39 hole pattern (holes 3.0 .. 3.1 mm) hang the module from the roof;
   - the two right-hand feet move below y 45;
   - the toggle moves to the left wall, y ~46 .. 59 (or this lid gets 3.5 mm taller).
4. **Firmware:** an SSD1309 driver, and a cap on lit area x contrast (~100 mA) to stay under the HT7833's 500 mA.
5. **Buying:** the colour of LCSC's C7466000 is unclear and only 114 are in stock. For white, take a 4- or 5-pin I2C
   QDtech-style module (MC242GW: 72 x 43, the same holes, a reset circuit on the board, V). Check its drawing before
   cutting the lid.

**If the board stays 66 x 64:** the **1.3" SH1106 (C7465997)** fits on J10 as it is, for +2.25 .. +2.52 USD a board.
Only the lid's window grows (~31 x 16.5 mm), and the firmware needs the SH1106 driver. It is about half the size the
user liked.

## Sources

- LCSC / HanSheng sheets with their mechanical drawings (page 6): C5248080 `wmsc.lcsc.com/.../2306271450_HS-HS96L03W2C03_C5248080.pdf`,
  C7465997 `.../2307201018_HS-HS13L03W2C01_C7465997.pdf`, C7465999 `.../2307201018_HS-HS154L03W2C01_C7465999.pdf`,
  C7466000 `.../2307201018_HS-HS242L03B2C01_C7466000.pdf`, C5139769 `.../2208221800_HS-HS242L01WZ01_C5139769.pdf`
  (all under `https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/`). Prices and stock: JLC's parts API, 2026-11-14.
- QDtech MC242GX (2.42" I2C): https://lcdwiki.com/2.42inch_IIC_OLED_Module_SKU:MC242GX, its drawing
  https://www.lcdwiki.com/res/MC242GX/2.42inch_IIC_Module_MC242GX_Size.pdf and schematic
  https://www.lcdwiki.com/res/MC242GX/2.42inch_IIC_Module_MC242GX_Schematic.pdf
- LCDWiki MSP242X (2.42" 7-pin SPI): https://www.lcdwiki.com/2.42inch_SPI_OLED_Module_SKU:MSP242X
- Waveshare 2.42" (61.5 x 39.5, 196 mA / 7.3 mA at 3.3 V): https://www.waveshare.com/wiki/2.42inch_OLED_Module, its
  drawing https://files.waveshare.com/wiki/2.42inch%20OLED%20Module/2_42inch_oled_module-sku25742.pdf and schematic
  https://files.waveshare.com/wiki/2.42inch-OLED-Module/2.42inch-OLED-Module-Schematic.pdf
- SSD1309 datasheet (Solomon Systech, VCC 7 .. 16 V, tcycle 2.5 us): https://www.lcdwiki.com/res/MC242GX/SSD1309.pdf
- Shop price: https://funduinoshop.com/en/electronic-modules/displays/oled/oled-lcd-display-2.42-inch-128x64-ssd1309-7-pin-spi/i2c
- The board: `mini2.kicad_pcb` (J10, J5 / J6 courtyards, the switches), `case/mini_case.scad` (the lid, the D-pad, the
  toggle's body, the feet), `parts.py` (the LDOs), `README.md` (Wi-Fi ~100 mA / 340 mA peak, both assumed).
