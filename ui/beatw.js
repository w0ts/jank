// SPDX-License-Identifier: GPL-3.0-only
// The beat network's weights blob (core/beat/beat_weights.h) from madmom's beat model, in the browser or in node, no Python:
// BeatW.fromMadmom(bytes of beats/2016/beats_lstm_1.pkl) -> Uint8Array "JBW1" + count + crc32 + 0 + float32[29251].
// madmom's model is a Python 2 pickle (protocol 2); this reads it as data only (a fixed list of class names, nothing
// run), and lays the weights out exactly as beat/scripts/export_beat_c.py does (checked bit for bit, tests/run.sh).
// The weights are madmom's, CC BY-NC-SA 4.0: the Install page fetches them at the user's click, for the user's own
// device; the site never hosts them or an image that holds them.
const BeatW = (() => {
  'use strict';
  const OP = {
    PROTO: 0x80, STOP: 0x2e, GLOBAL: 0x63, BINPUT: 0x71, LONG_BINPUT: 0x72, BINGET: 0x68, LONG_BINGET: 0x6a,
    EMPTY_TUPLE: 0x29, EMPTY_DICT: 0x7d, EMPTY_LIST: 0x5d, MARK: 0x28, SHORT_BINSTRING: 0x55, BINSTRING: 0x54,
    BININT: 0x4a, BININT1: 0x4b, BININT2: 0x4d, TUPLE: 0x74, TUPLE1: 0x85, TUPLE2: 0x86, TUPLE3: 0x87, NONE: 0x4e,
    NEWTRUE: 0x88, NEWFALSE: 0x89, NEWOBJ: 0x81, REDUCE: 0x52, BUILD: 0x62, SETITEMS: 0x75, SETITEM: 0x73,
    APPENDS: 0x65, APPEND: 0x61,
  };
  const MARK = Symbol('mark');
  // the only classes the beat model may name: anything else is refused (a pickle is code in Python; here it is data only)
  const ALLOWED = new Set(['numpy.core.multiarray _reconstruct', 'numpy ndarray', 'numpy dtype', 'madmom.ml.nn NeuralNetwork',
    'madmom.ml.nn.layers LSTMLayer', 'madmom.ml.nn.layers Gate', 'madmom.ml.nn.layers Cell',
    'madmom.ml.nn.layers FeedForwardLayer', 'madmom.ml.nn.activations sigmoid', 'madmom.ml.nn.activations tanh']);

  /** @param {Uint8Array} bytes  @returns {any} the unpickled tree: objects as {cls, state}, arrays as {shape, data: Float32Array} */
  function unpickle(bytes) {
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const stack = [], memo = new Map();
    let i = 0;
    const str = (n) => { const s = bytes.subarray(i, i + n); i += n; return s; };
    const line = () => { const e = bytes.indexOf(0x0a, i); if (e < 0) throw new Error('pickle: no newline'); const s = new TextDecoder().decode(bytes.subarray(i, e)); i = e + 1; return s; };
    const popMark = () => { const k = stack.lastIndexOf(MARK); if (k < 0) throw new Error('pickle: no mark'); const items = stack.splice(k); items.shift(); return items; };
    for (;;) {
      if (i >= bytes.length) throw new Error('pickle: ends before STOP');
      const op = bytes[i++];
      switch (op) {
        case OP.PROTO: if (bytes[i++] !== 2) throw new Error('pickle: protocol 2 expected'); break;
        case OP.STOP: return stack.pop();
        case OP.GLOBAL: { const name = line() + ' ' + line(); if (!ALLOWED.has(name)) throw new Error('pickle: refused class ' + name); stack.push({ global: name }); break; }
        case OP.BINPUT: memo.set(bytes[i++], stack[stack.length - 1]); break;
        case OP.LONG_BINPUT: memo.set(dv.getUint32(i, true), stack[stack.length - 1]); i += 4; break;
        case OP.BINGET: stack.push(memo.get(bytes[i++])); break;
        case OP.LONG_BINGET: stack.push(memo.get(dv.getUint32(i, true))); i += 4; break;
        case OP.EMPTY_TUPLE: stack.push([]); break;
        case OP.EMPTY_LIST: stack.push([]); break;
        case OP.EMPTY_DICT: stack.push({}); break;
        case OP.MARK: stack.push(MARK); break;
        case OP.SHORT_BINSTRING: { const n = bytes[i++]; stack.push(str(n)); break; }
        case OP.BINSTRING: { const n = dv.getInt32(i, true); i += 4; stack.push(str(n)); break; }
        case OP.BININT: stack.push(dv.getInt32(i, true)); i += 4; break;
        case OP.BININT1: stack.push(bytes[i++]); break;
        case OP.BININT2: stack.push(dv.getUint16(i, true)); i += 2; break;
        case OP.TUPLE: stack.push(popMark()); break;
        case OP.TUPLE1: stack.push(stack.splice(-1)); break;
        case OP.TUPLE2: stack.push(stack.splice(-2)); break;
        case OP.TUPLE3: stack.push(stack.splice(-3)); break;
        case OP.NONE: stack.push(null); break;
        case OP.NEWTRUE: stack.push(true); break;
        case OP.NEWFALSE: stack.push(false); break;
        case OP.NEWOBJ: { const args = stack.pop(), cls = stack.pop(); stack.push({ cls: cls.global, args, state: null }); break; }
        case OP.REDUCE: { const args = stack.pop(), fn = stack.pop(); stack.push(reduce(fn.global, args)); break; }
        case OP.BUILD: { const state = stack.pop(); build(stack[stack.length - 1], state); break; }
        case OP.SETITEMS: { const items = popMark(), d = stack[stack.length - 1]; for (let k = 0; k < items.length; k += 2) d[latin1(items[k])] = items[k + 1]; break; }
        case OP.SETITEM: { const v = stack.pop(), k = stack.pop(); stack[stack.length - 1][latin1(k)] = v; break; }
        case OP.APPENDS: { const items = popMark(); stack[stack.length - 1].push(...items); break; }
        case OP.APPEND: { const v = stack.pop(); stack[stack.length - 1].push(v); break; }
        default: throw new Error('pickle: opcode 0x' + op.toString(16) + ' at ' + (i - 1) + ' not supported');
      }
    }
  }
  const latin1 = (b) => (b instanceof Uint8Array ? String.fromCharCode(...b) : b);
  function reduce(name, args) {
    if (name === 'numpy.core.multiarray _reconstruct') return { cls: 'ndarray', state: null };
    if (name === 'numpy dtype') return { cls: 'dtype', name: latin1(args[0]), state: null };
    throw new Error('pickle: REDUCE of ' + name + ' not supported');
  }
  function build(obj, state) {
    if (obj.cls === 'dtype') { obj.state = state; return; }
    if (obj.cls === 'ndarray') {
      const [, shape, dtype, fortran, raw] = state;          // (version, shape, dtype, is_fortran, raw bytes)
      if (dtype.name !== 'f4' || latin1(dtype.state[1]) !== '<') throw new Error('ndarray: float32 little endian expected, got ' + dtype.name);
      const n = shape.reduce((a, b) => a * b, 1);
      if (raw.length !== 4 * n) throw new Error('ndarray: ' + raw.length + ' bytes for ' + n + ' floats');
      const flat = new Float32Array(raw.slice().buffer);
      obj.shape = shape;
      obj.data = fortran ? toC(flat, shape) : flat;
      return;
    }
    obj.state = state;
  }
  function toC(flat, shape) {                                // Fortran (column-major) order to C (row-major); 2-D is all madmom has
    if (shape.length < 2) return flat;
    if (shape.length !== 2) throw new Error('ndarray: Fortran order above 2-D not supported');
    const [r, c] = shape, out = new Float32Array(flat.length);
    for (let y = 0; y < r; y++) for (let x = 0; x < c; x++) out[y * c + x] = flat[x * r + y];
    return out;
  }

  const GATES = ['input_gate', 'forget_gate', 'cell', 'output_gate'];
  /** @param {Uint8Array} bytes the .pkl file  @returns {Float32Array} the weights in beat_model.c's order */
  function madmomBeatWeights(bytes) {
    const net = unpickle(bytes);
    if (!net || net.cls !== 'madmom.ml.nn NeuralNetwork') throw new Error('not a madmom NeuralNetwork');
    const layers = net.state.layers;
    if (layers.length !== 4) throw new Error('expected 3 LSTM layers and 1 output layer, got ' + layers.length);
    const parts = [];
    const arr = (o, k, len) => { const a = o.state[k]; if (!a || !a.data) throw new Error('missing ' + k); if (len !== undefined && a.data.length !== len) throw new Error(k + ': ' + a.data.length + ' values, ' + len + ' expected'); return a.data; };
    for (let li = 0; li < 3; li++) {
      const L = layers[li];
      if (L.cls !== 'madmom.ml.nn.layers LSTMLayer') throw new Error('layer ' + li + ' is not an LSTM');
      const g = GATES.map((n) => L.state[n]);
      const nIn = g[0].state.weights.shape[0];
      for (const x of g) parts.push(arr(x, 'weights', nIn * 25));
      for (const x of g) parts.push(arr(x, 'recurrent_weights', 625));
      for (const x of g) parts.push(arr(x, 'bias', 25));
      for (const k of [0, 1, 3]) parts.push(arr(g[k], 'peephole_weights', 25));
    }
    const ff = layers[3];
    parts.push(arr(ff, 'weights', 25), arr(ff, 'bias', 1));
    const out = new Float32Array(parts.reduce((a, p) => a + p.length, 0));
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
  }

  const MADMOM_URL = 'https://raw.githubusercontent.com/CPJKU/madmom_models/master/beats/2016/beats_lstm_1.pkl';
  const COUNT = 29251;                                     // beat_weights.h BEAT_WEIGHTS_COUNT
  const crcTable = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
  /** @param {Uint8Array} b @returns {number} zlib's CRC-32 */
  const crc32 = (b) => { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  /** @param {Float32Array} w @returns {Uint8Array} the blob */
  function blob(w) {
    if (w.length !== COUNT) throw new Error('beat weights: ' + w.length + ' values, ' + COUNT + ' expected');
    const data = new Uint8Array(w.buffer, w.byteOffset, w.byteLength);
    if (new Uint8Array(new Uint16Array([1]).buffer)[0] !== 1) throw new Error('beat weights: a little-endian machine is needed');
    const out = new Uint8Array(16 + data.length), dv = new DataView(out.buffer);
    out.set([0x4a, 0x42, 0x57, 0x31]);                    // "JBW1"
    dv.setUint32(4, COUNT, true);
    dv.setUint32(8, crc32(data), true);
    dv.setUint32(12, 0, true);
    out.set(data, 16);
    return out;
  }
  /** @param {Uint8Array} pkl madmom's .pkl @returns {Uint8Array} the blob */
  const fromMadmom = (pkl) => blob(madmomBeatWeights(pkl));
  return { MADMOM_URL, COUNT, unpickle, madmomBeatWeights, crc32, blob, fromMadmom };
})();
if (typeof module !== 'undefined') module.exports = BeatW;
