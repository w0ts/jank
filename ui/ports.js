/* Which MIDI ports are a jank. Pure: names in, names out, so the node tests can pick it apart. The page attaches to a port ONLY
   when its name passes jankPortName: any other device (a synth, an FM-1) must never be sent a SysEx line.
   A jank is "jank" or "jank (USB)": the word jank, not followed by a letter and not
   inside a longer word. "Digitone" or "Jankowski" do not match; "My jank" does (the word is there). */
const JANK_NAME = /(^|[^a-z])jank([^a-z]|$)/i;

function jankPortName(name) {
  return JANK_NAME.test(name || "");
}
/* the jank among the ports, the USB one ("Jank" or, before 2026-11-21, "jank": the plain name, any case) first; never any other port */
function jankPorts(inputs) {
  return inputs.filter((p) => jankPortName(p.name)).sort((a, b) => (String(b.name).toLowerCase() === "jank") - (String(a.name).toLowerCase() === "jank"));
}
function outputFor(input, outputs) {
  return outputs.find((o) => o.name === input.name) || outputs.find((o) => jankPortName(o.name)) || null;
}
/* what the browser lists, for the error line */
function portNames(ports) {
  return ports.length ? ports.map((p) => p.name || "unnamed").join(", ") : "none";
}
