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

// the control line in SysEx (the S3's USB MIDI port): the same frame as core/jank_sysex.c
eq(box.textToSysex("get\n"), [[0xf0, 0x7d, 0x4a, 0x4b, 0x67, 0x65, 0x74, 0x0a, 0xf7]]);
eq(box.textToSysex("x".repeat(31)).map((f) => f.length), [35, 6]);
eq(box.textToSysex("\u00e9")[0][4], 0x3f);
eq(box.sysexToText([0xf0, 0x7d, 0x4a, 0x4b, 0x6f, 0x6b, 0x0a, 0xf7]), "ok\n");
eq(box.sysexToText([0xf0, 0x7d, 0x41, 0x42, 0x6f, 0xf7]), null);
eq(box.sysexToText([0xf8]), null);
eq(box.isJankPort({ name: "jank", state: "connected" }), true);
eq(box.isJankPort({ name: "JANK MIDI", state: "connected" }), true);
eq(box.isJankPort({ name: "jank", state: "disconnected" }), false);
eq(box.isJankPort({ name: "FM1-E040", state: "connected" }), false);

// the Network form
eq(box.networkCommands("jank", "", "", false), ["set net jank", "reboot"]);
eq(box.networkCommands("wifi", "Home Net", "correct horse", false), ["set ssid Home Net", "set pass correct horse", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "Home", "", true), ["set ssid Home", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "Open", "", false), ["set ssid Open", "set pass", "set net wifi", "reboot"]);
eq(box.networkCommands("wifi", "", "correct horse", false), null);
eq(box.networkCommands("wifi", "Home", "short", false), null);
eq(box.networkCommands("wifi", "H\u00f6me", "correct horse", false), null);
eq(box.networkCommands("wifi", "x".repeat(33), "correct horse", false), null);
eq(box.networkLine({ net: "jank", group: "2" }), "MODE JANK, group 2");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "0" }), "Looking for Home");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "0.0.0.0" }), "On Home, waiting for an address");
eq(box.networkLine({ net: "wifi", ssid: "Home", wifi: "1", wifi_ip: "192.168.1.5", wifi_rssi: "-50", wifi_channel: "6" }), "On Home, 192.168.1.5, -50 dBm, channel 6");

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
