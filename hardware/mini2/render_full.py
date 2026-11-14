# SPDX-License-Identifier: GPL-3.0-only
"""The board with every part the user fits (stage 2, the user's "show it with everything on"): render-3d-full.png
(top, angled) and render-bottom-full.png (from below). The routed board is copied with the user-soldered parts' DNP
cleared (the three PJ-307C jacks, the two DINs, the two PH2.0 sockets, the screen's header, the power switch: their JLC
3D models), plus one generated model (lib/3d/USER_EXTRAS.wrl, simple boxes: NOT the parts' own shapes): the 0.96"
screen module on its header (27.3 x 27.8 mm board, its glass), the FPC antenna upright at the bottom edge and its lead
from the u.FL, the two cells on edge under the board (66 x 18 x 6.7 mm, the case's worst) and the power toggle (an
MTS-102 body, bushing and lever at the left edge where the case's wall holds it, its two wires to J11). The copy is
deleted after.
  .../python3.9 render_full.py        (KiCad's Python and kicad-cli)
"""
import os
import subprocess

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
KC = "/opt/homebrew/bin/kicad-cli"
USER = ("J2", "J3", "J5", "J6", "J7", "J8", "J9", "J10", "J13", "J14")
BT = 1.6
U = 2.54                                  # (KiCad's VRML unit: 0.1 inch)
MODULE_W, MODULE_H, MODULE_Z = 35.4, 33.5, 2.5   # (72 x 72: the 1.3 in, the smallest screen kept)   # (make_board.MODULE: the module's outline, its underside over the board)


def box(x0, y0, z0, x1, y1, z1, rgb):
    """an IndexedFaceSet box, board mm (x right, y down, z up from the board's top) -> VRML units (y up)"""
    pts = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    coords = ", ".join(f"{x / U:.4f} {-y / U:.4f} {z / U:.4f}" for x, y, z in pts)
    faces = "0,1,3,2,-1, 4,6,7,5,-1, 0,4,5,1,-1, 2,3,7,6,-1, 0,2,6,4,-1, 1,5,7,3,-1"
    return (f"Shape {{ appearance Appearance {{ material Material {{ diffuseColor {rgb} }} }}\n"
            f"  geometry IndexedFaceSet {{ solid FALSE coord Coordinate {{ point [ {coords} ] }} coordIndex [ {faces} ] }} }}\n")


def extras(board):
    fps = {f.GetReference(): f for f in board.GetFootprints()}

    def at(ref):
        p = fps[ref].GetPosition()
        return pcbnew.ToMM(p.x) - 100, pcbnew.ToMM(p.y) - 100
    hx, hy = at("J10")
    jx, jy = at("J4")
    w = pcbnew.ToMM(board.GetBoardEdgesBoundingBox().GetWidth())
    h = pcbnew.ToMM(board.GetBoardEdgesBoundingBox().GetHeight())
    x0, y0 = hx - MODULE_W / 2, hy - 2.0
    s = ["#VRML V2.0 utf8\n"]
    s.append(box(x0, y0, MODULE_Z, x0 + MODULE_W, y0 + MODULE_H, MODULE_Z + 1.2, "0.1 0.25 0.6"))           # the module
    s.append(box(x0 + 0.6, y0 + 5.0, MODULE_Z + 1.2, x0 + MODULE_W - 0.6, y0 + 21.0, MODULE_Z + 2.6, "0.05 0.05 0.05"))  # glass
    s.append(box(3.0, h + 0.2, 0.5, 45.0, h + 0.4, 10.5, "0.7 0.55 0.2"))                                # the FPC antenna
    s.append(box(jx - 0.55, jy, 1.2, jx + 0.55, h + 0.2, 2.3, "0.15 0.15 0.15"))                          # its lead
    px, py = at("J11")
    ty, tz = py + 5.0, 5.4                       # (case/mini_case.scad: pwr_dy 5, pwr_z = bt + 7 over the board's bottom;
    s.append(box(-0.6, ty - 6.4, tz - 3.4, 12.4, ty + 6.4, tz + 3.4, "0.2 0.2 0.25"))      #  72 x 72: the LEFT wall) its body
    s.append(box(-9.2, ty - 3.0, tz - 3.0, -0.6, ty + 3.0, tz + 3.0, "0.75 0.75 0.75"))    # its bushing
    s.append(box(-19.0, ty - 0.8, tz - 0.8, -9.2, ty + 0.8, tz + 0.8, "0.8 0.8 0.8"))      # its lever
    for dx in (-2.0, 2.0):                                                                # the two wires
        s.append(box(px + dx - 0.4, py, 0.0, px + dx + 0.4, ty - 6.4, 0.8, "0.8 0.1 0.1" if dx < 0 else "0.1 0.1 0.1"))
    for yc in (h - 13.25, h - 3.75):                                                                        # the cells
        s.append(box(0.0, yc - 3.35, -BT - 0.3 - 18.0, w, yc + 3.35, -BT - 0.3, "0.35 0.5 0.8"))
    path = os.path.join(HERE, "lib", "3d", "USER_EXTRAS.wrl")
    open(path, "w").write("".join(s))
    f = pcbnew.FOOTPRINT(board)
    f.SetReference("XTRA")
    f.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(100), pcbnew.FromMM(100)))
    m = pcbnew.FP_3DMODEL()
    m.m_Filename = "${KIPRJMOD}/lib/3d/USER_EXTRAS.wrl"
    f.Add3DModel(m)
    f.Reference().SetVisible(False)
    f.SetExcludedFromBOM(True)
    f.SetExcludedFromPosFiles(True)
    board.Add(f)


def main():
    board = pcbnew.LoadBoard(os.path.join(HERE, "mini2.kicad_pcb"))
    for f in board.GetFootprints():
        if f.GetReference() in USER:
            f.SetDNP(False)
    extras(board)
    tmp = os.path.join(HERE, "mini2-full.kicad_pcb")
    board.Save(tmp)
    try:
        for args, out in ((["--perspective", "--rotate", "-40,0,-25", "--zoom", "0.8"], "render-3d-full.png"),
                          (["--side", "bottom", "--perspective", "--rotate", "40,0,25", "--zoom", "0.8"], "render-bottom-full.png")):
            subprocess.run(["nice", "-n", "19", KC, "pcb", "render", "--width", "1600", "--height", "1200", "--quality", "high",
                            *args, "-o", os.path.join(HERE, out), tmp], check=True, capture_output=True)
            print("wrote", out)
    finally:
        for e in (".kicad_pcb", ".kicad_prl", ".kicad_pro"):
            p = os.path.splitext(tmp)[0] + e
            if os.path.exists(p):
                os.remove(p)


if __name__ == "__main__":
    main()
