// SPDX-License-Identifier: GPL-3.0-only
// The palette the firmware draws with (core/jank_lcd.h: JLC_PALETTE, JLC_INK_MAP and JLC_FLASH_FIELD) against the one the design preview draws
// with (ui/lcd2.js, approved by the owner): the same names, the same exact RGB values, the same colour for each ink id, the same two flash
// fields, and the flash numbers of the driver equal the core's. Exit status 1 on any difference.
'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const read = f => fs.readFileSync(path.join(__dirname, '..', f), 'utf8');
const { OLED, Lcd2 } = new Function(read('ui/oled.js') + read('ui/lcd2.js') + '; return { OLED, Lcd2 };')();
const header = read('core/jank_lcd.h');
let checks = 0;
const eq = (a, b, what) => { checks++; assert.deepStrictEqual(a, b, what); };

const block = name => {
  const m = header.match(new RegExp('#define ' + name + '\\(X\\)([^\\n]*)'));
  assert.ok(m, name + ' is in core/jank_lcd.h');
  return m[1];
};
const palette = {};
for (const m of block('JLC_PALETTE').matchAll(/X\((\w+),\s*(\d+),\s*(\d+),\s*(\d+)\)/g)) palette[m[1]] = [+m[2], +m[3], +m[4]];
const inkMap = [...block('JLC_INK_MAP').matchAll(/X\((\w+)\)/g)].map(m => m[1]);

const fields = {};
for (const m of block('JLC_FLASH_FIELD').matchAll(/X\((\w+),\s*(\d+),\s*(\d+),\s*(\d+)\)/g)) fields[m[1]] = [+m[2], +m[3], +m[4]];
eq(fields, Lcd2.FLASH_FIELD, 'the two flash fields (an ordinary beat, the bar\'s first beat) have the preview\'s exact RGB values');
// the driver's flash numbers are the core's (jk_flash_at answers them), and its window macros are the ones the JS port reads
const enumOf = (text, name) => Object.fromEntries([...text.match(new RegExp('enum ' + name + ' \\{([^}]*)\\}'))[1].matchAll(/(\w+)\s*=\s*(\d+)/g)].map(m => [m[1], +m[2]]));
const jlc = enumOf(header, 'jlc_flash'), jk = enumOf(read('core/jank_core.h'), 'jk_flash');
eq([jlc.JLC_FLASH_NONE, jlc.JLC_FLASH_BEAT, jlc.JLC_FLASH_BAR], [jk.JK_FLASH_NONE, jk.JK_FLASH_BEAT, jk.JK_FLASH_BAR], 'the driver\'s flash numbers are core/jank_core.h\'s enum jk_flash');
eq(Object.keys(fields), ['beat', 'bar'], 'the field table is in the order of the flash numbers 1 and 2');
eq(Object.keys(palette), Object.keys(Lcd2.PALETTE), 'the firmware palette has the preview palette\'s colour names, in its order');
eq(palette, Lcd2.PALETTE, 'every colour has the preview\'s exact RGB values');
eq(inkMap, Lcd2.INK_PALETTE, 'each ink id has the preview\'s colour');
eq(inkMap.length, Object.keys(OLED.INK).length, 'one colour for each ink id of ui/oled.js');
for (const [name, id] of Object.entries(OLED.INK)) eq(inkMap[id] in palette, true, 'ink id ' + name + ' (' + id + ') names a colour of the palette');
const ids = fs.readFileSync(path.join(__dirname, '..', 'core', 'jank_oled.h'), 'utf8').match(/enum jo_ink \{([^}]*)\}/);
assert.ok(ids, 'enum jo_ink is in core/jank_oled.h'); checks++;
eq(ids[1].match(/JO_INK_(\w+)/g).slice(0, -1).map(s => s.slice(7)), Object.keys(OLED.INK), 'the C ink ids are ui/oled.js\'s, in order');
console.log('lcd palette: ' + checks + ' checks passed');
