// SPDX-License-Identifier: GPL-3.0-only
// node scripts/beat_weights.js <beats_lstm_1.pkl> <out.bin>
// The beat network's weights blob (core/beat/beat_weights.h) from madmom's model, with the Install page's own converter
// (ui/beatw.js). For the host tests and the bench; madmom's weights (CC BY-NC-SA 4.0) are never committed or shipped.
'use strict';
const fs = require('fs');
const path = require('path');
const BeatW = require(path.join(__dirname, '..', 'ui', 'beatw.js'));

const [pkl, out] = process.argv.slice(2);
if (!pkl || !out) {
  console.error('usage: node scripts/beat_weights.js <beats_lstm_1.pkl> <out.bin>');
  process.exit(2);
}
try {
  const blob = BeatW.fromMadmom(new Uint8Array(fs.readFileSync(pkl)));
  fs.writeFileSync(out, blob);
  console.log(`${out}: ${BeatW.COUNT} weights, ${blob.length} bytes`);
} catch (e) {
  console.error(`beat_weights: ${pkl}: ${e.message}`);
  process.exit(1);
}
