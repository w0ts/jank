// The simulator page with its `<!--@include ui/...-->` markers expanded, as player.py serves it. Tests read the page through this.
'use strict';
const { execFileSync } = require('child_process');
const path = require('path');
const INLINE_UI = path.join(__dirname, '..', 'scripts', 'inline_ui.py');
module.exports = file => execFileSync('python3', [INLINE_UI, file], { encoding: 'utf8', maxBuffer: 1 << 26 });
