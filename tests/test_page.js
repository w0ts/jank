// SPDX-License-Identifier: MIT
// The control page's logic, with no browser and no stick. Exit status: non-zero on a failure.
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const assert = require("assert");

const html = fs.readFileSync(path.join(__dirname, "..", "page", "jank.html"), "utf8");
const logic = html.match(/<script id="logic">([\s\S]*?)<\/script>/)[1];
const ui = html.match(/<script id="ui">([\s\S]*?)<\/script>/)[1];
const box = {};
vm.createContext(box);
vm.runInContext(logic, box);
new vm.Script(ui);                       // the DOM part at least parses

let checks = 0;
function eq(a, b) {
  checks++;
  assert.deepStrictEqual(JSON.parse(JSON.stringify(a)), b);
}

// nothing external: no script, style, font or fetch from anywhere
checks++;
assert.ok(!/https?:\/\//.test(html), "the page loads nothing external");
assert.ok(!/<script[^>]+src=/.test(html) && !/<link/.test(html));
checks++;
assert.ok(/<title>jank<\/title>/.test(html));
checks++;
assert.ok(!/innerHTML/.test(html), "device text goes in through textContent only");

// a stream cut anywhere
eq(box.splitLines("", "ab"), { lines: [], rest: "ab" });
eq(box.splitLines("ab", "c\nd"), { lines: ["abc"], rest: "d" });
eq(box.splitLines("", "a\r\nb\n"), { lines: ["a", "b"], rest: "" });

// an answer, as jank_ctl.c writes it
const a = box.parseAnswer(["group=3", "name=front of house", "peer=8c:aa:bb:cc:dd:ee,-44,100,5", "peer=8c:aa:bb:cc:dd:ef,-60,20,1", "not a pair"]);
eq(a.values, { group: "3", name: "front of house" });
eq(a.units[0], { mac: "8c:aa:bb:cc:dd:ee", rssi: -44, age: 100, frames: 5 });
eq(a.units.length, 2);
// "peer=on" in `get` is a setting, not a unit: the two share a key
const g = box.parseAnswer(["peer=on"]);
eq(g.units.length, 0);
eq(g.values.peer, "on");
// the LOLIN32's log line is not part of an answer
const l = box.parseAnswer(["# up=3 group=1 heard=1,0,0,0,0,0,0,0,0", "group=1"]);
eq(Object.keys(l.values), ["group"]);

// a scan's answer: group,channel,frames,units,janks,rssi
const s = box.parseAnswer(["scan=3,11,4,3,1,-50", "scan=7,1,1,1,1,-65"]);
eq(s.scans[0], { group: 3, channel: 11, frames: 4, units: 3, janks: 1, rssi: -50 });
eq(s.scans.length, 2);
eq(box.parseAnswer(["scan=none"]).scans, []);
eq(box.unitText({ units: 3, janks: 1 }), "3, of which 1 jank");
eq(box.unitText({ units: 2, janks: 0 }), "2");

eq(box.isEnd("ok"), true);
eq(box.isEnd("err bad setting"), true);
eq(box.isEnd("okay"), false);

eq(box.faultDrops({ drop_big: "1", drop_bad: "2", tx_fail: "3", queue_full: "4", drop_foreign: "5", drop_filter: "99" }), 15);
eq(box.faultDrops({}), 0);

eq(box.setCommand("group", "4"), "set group 4");
eq(box.setCommand("group", "0"), null);
eq(box.setCommand("group", "10"), null);
eq(box.setCommand("group", "1.5"), null);
eq(box.setCommand("group", ""), null);
eq(box.setCommand("offset", "-12"), "set offset -12");
eq(box.setCommand("offset", "101"), null);
eq(box.setCommand("quantum", "16"), "set quantum 16");
eq(box.setCommand("name", "front of house"), "set name front of house");
eq(box.setCommand("name", ""), null);
eq(box.setCommand("name", "x\nreboot"), null);
eq(box.setCommand("name", "café"), null);
eq(box.setCommand("filter", "all"), "set filter all");

console.log("page: " + checks + " checks passed");
