# SPDX-License-Identifier: GPL-3.0-only
"""Stage 2 (2026-11-13) hand edits on the board Freerouting left (README "Routing, mini 2"): they are NOT part of make_fab.sh.
Applied once to the routed board, in this order, with KiCad's Python: C20 turned (its +3V3 pad east, its GND pad was boxed in by
MIDI2_TX), MIDI2_TX and LINE_IN re-laid beside it, one LIN_OUT via nudged for the hole-to-hole rule. ROUTE=1 re-routes and loses them."""
import pcbnew
P="mini2.kicad_pcb"
b=pcbnew.LoadBoard(P)
M=pcbnew.FromMM; ox=100.0
def near(a,x,y): return abs(pcbnew.ToMM(a.x)-ox-x)<0.02 and abs(pcbnew.ToMM(a.y)-ox-y)<0.02
def has(t,p,q):
    s,e=t.GetStart(),t.GetEnd()
    return (near(s,*p) and near(e,*q)) or (near(s,*q) and near(e,*p))
rm=[]
for t in b.GetTracks():
    if t.GetClass()!="PCB_TRACK": continue
    if t.GetNetname()=="+3V3" and any(has(t,p,q) for p,q in [((30.58,38.45),(30.41,38.62)),((30.41,38.62),(30.1,38.62)),((30.1,38.62),(30.1,39.49))]): rm.append(t)
    if t.GetNetname()=="MIDI2_TX" and any(has(t,p,q) for p,q in [((32.1,38.25),(32.1,39.49)),((31.57,37.72),(32.1,38.25))]): rm.append(t)
print("removing",len(rm))
for t in rm: b.Remove(t)
c=[f for f in b.GetFootprints() if f.GetReference()=="C20"][0]
c.SetOrientationDegrees(180); c.SetPosition(pcbnew.VECTOR2I(M(ox+31.0),M(ox+38.3)))
def tr(net,x0,y0,x1,y1,w=0.2):
    t=pcbnew.PCB_TRACK(b); t.SetStart(pcbnew.VECTOR2I(M(ox+x0),M(ox+y0))); t.SetEnd(pcbnew.VECTOR2I(M(ox+x1),M(ox+y1)))
    t.SetWidth(M(w)); t.SetLayer(pcbnew.F_Cu); t.SetNet(b.FindNet(net)); b.Add(t)
for s in [(31.42,38.3,31.42,38.9),(31.42,38.9,30.1,38.9),(30.1,38.9,30.1,39.49)]: tr("+3V3",*s)
for s in [(32.1,39.49,32.1,38.0),(32.1,38.0,31.82,37.72),(31.82,37.72,31.57,37.72)]: tr("MIDI2_TX",*s)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(P)
# LINE_IN: replace the router's kink by a cleaner path 0.03 mm further from MIDI2_TX
b=pcbnew.LoadBoard(P)
rm=[]
for t in b.GetTracks():
    if t.GetClass()=="PCB_TRACK" and t.GetNetname()=="LINE_IN" and t.GetLayer()==pcbnew.F_Cu:
        s,e=t.GetStart(),t.GetEnd()
        if pcbnew.ToMM(max(s.x,e.x))-ox<32.9 and pcbnew.ToMM(min(s.y,e.y))-ox>37.0 and pcbnew.ToMM(max(s.y,e.y))-ox<37.9 or has(t,(32.91,37.82),(32.39,37.82)) or has(t,(32.12,37.05),(31.93,37.05)) or has(t,(32.27,37.2),(32.12,37.05)): rm.append((t.GetStart(),t.GetEnd()))
        else: pass
print("LINE_IN segs to drop",len(rm))
for t in list(b.GetTracks()):
    if t.GetClass()=="PCB_TRACK" and t.GetNetname()=="LINE_IN" and (t.GetStart(),t.GetEnd()) in rm: b.Remove(t)
for s in [(32.91,37.82,32.55,37.82),(32.55,37.82,32.3,37.57),(32.3,37.57,32.3,37.25),(32.3,37.25,32.1,37.05),(32.1,37.05,31.93,37.05)]: tr("LINE_IN",*s)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(P)
# --- the LIN_OUT via at (134.85, 135.89) moved 0.03 mm up, with its track ends (hole to hole 0.4987 -> 0.5 and more) ---
b = pcbnew.LoadBoard(P)
old = pcbnew.VECTOR2I(M(134.85), M(135.89)); new = pcbnew.VECTOR2I(M(134.85), M(135.86))
def close(a): return abs(a.x - old.x) < 20000 and abs(a.y - old.y) < 20000
for t in b.GetTracks():
    if t.GetNetname() != "LIN_OUT": continue
    if t.GetClass() == "PCB_VIA":
        if close(t.GetPosition()): t.SetPosition(new)
    else:
        if close(t.GetStart()): t.SetStart(new)
        if close(t.GetEnd()): t.SetEnd(new)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(P)
