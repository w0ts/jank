"""The site's pictures of the jank mini 3, rendered in Blender (Cycles) from photo.scad's STL pieces.

The case is white PLA, its inlaid labels and the cap's symbols are the site's purple (ui/box3.css #A06CF0, the device
drawn on the site), the cap is the drawing's grey, and the screen shows a real frame of the beat page
(page/img/screen-beat.png) on the LCD-2's active area.

blender -b -P photo.py -- <stl dir> <screen png> <out dir> [view ...]
views: hero (About page), assembly, exploded, top, cap (Build page). make_photo.sh runs it.
"""
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector

MM = 0.001
PURPLE = "#A06CF0"           # ui/box3.css: the labels and symbols of the device drawn on the site
CAP_GREY = "#5f5f5f"         # ui/box3.css .cap
CASE_WHITE = "#f2f1ec"       # white PLA, a touch warm so it is not read as a blank
# box2.scad: the LCD-2's active area (AA) and the screen's front (lcd_front); checked against lcd2.stl in screen_rect()
AA_SIZE = (40.8, 30.6)
AA_FAR = 2.3                 # from the backlight's right edge
EXPLODE = 40.0               # box2.scad assembly(40): top +e, cap +1.5e, bottom -0.8e

# view: (width, height, camera kind, eye mm, target mm, lens mm or ortho scale mm, exploded)
VIEWS = {
    "hero":     (1600, 1000, "persp", (-75, -200, 150), (48, -41, -6), 40, False),
    "assembly": (1600, 1200, "persp", (-75, -200, 150), (48, -41, -6), 38, False),
    "exploded": (1600, 1200, "persp", (-170, -370, 215), (48, -41, 6), 50, True),
    "top":      (1600, 1300, "ortho", (48, -41, 300), (48, -41, 0), 112, False),
    "cap":      (1200, 1200, "ortho", (64.75, -41, 300), (64.75, -41, 0), 34, False),
}


def srgb(hex_):
    h = hex_.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    lin = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return (*lin, 1.0)


def material(name, color, rough=0.5, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = srgb(color) if isinstance(color, str) else color
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    return m


def screen_material(png):
    m = bpy.data.materials.new("screen")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(str(png))
    tex.interpolation = "Closest"           # the LCD's pixels stay sharp
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Strength"].default_value = 3.4
    glass = nt.nodes.new("ShaderNodeBsdfPrincipled")
    glass.inputs["Base Color"].default_value = (0, 0, 0, 1)
    glass.inputs["Roughness"].default_value = 0.25
    glass.inputs["Specular IOR Level"].default_value = 0.12
    add = nt.nodes.new("ShaderNodeAddShader")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tex.outputs["Color"], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], add.inputs[0])
    nt.links.new(glass.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], out.inputs["Surface"])
    return m


def import_stl(path, mat, dz=0.0):
    bpy.ops.wm.stl_import(filepath=str(path), global_scale=MM)
    ob = bpy.context.selected_objects[0]
    ob.data.materials.append(mat)
    ob.location.z += dz * MM
    return ob


def screen_rect(lcd2):
    """The active area from the LCD-2's backlight block (the only box of lcd2.stl that reaches lcd_front)."""
    pts = world_points(lcd2)           # (the importer puts the mm -> m scale on the object, not the vertices)
    ztop = max(p.z for p in pts)
    top = [p for p in pts if abs(p.z - ztop) < 1e-7]
    x1 = max(p.x for p in top) / MM
    ymid = (min(p.y for p in top) + max(p.y for p in top)) / 2 / MM
    x0 = x1 - AA_FAR - AA_SIZE[0]
    if x0 < min(p.x for p in top) / MM:
        raise SystemExit("photo.py: the active area falls outside the LCD-2's backlight: box2.scad changed?")
    return x0, x1, ymid, ztop / MM


def world_points(ob):
    return [ob.matrix_world @ v.co for v in ob.data.vertices]


def add_screen(lcd2, png, dz):
    x0, x1, ymid, z = screen_rect(lcd2)
    bpy.ops.mesh.primitive_plane_add(size=1)
    sc = bpy.context.active_object
    sc.scale = (AA_SIZE[0] * MM, AA_SIZE[1] * MM, 1)
    sc.location = ((x0 + x1) / 2 * MM, ymid * MM, (z + 0.02 + dz) * MM)
    sc.data.materials.append(screen_material(png))
    return sc


def build_scene(stl, png, exploded):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    e = EXPLODE if exploded else 0.0
    white = material("case", CASE_WHITE, rough=0.6)
    purple = material("labels", PURPLE, rough=0.55)
    cap = material("cap", CAP_GREY, rough=0.55)
    import_stl(stl / "top.stl", white, e)
    import_stl(stl / "labels.stl", purple, e + 0.01)       # (flush with the face: lifted a hair so it wins)
    import_stl(stl / "cap.stl", cap, 1.5 * e)
    import_stl(stl / "cap_symbols.stl", purple, 1.5 * e + 0.01)
    import_stl(stl / "bottom.stl", white, -0.8 * e)
    import_stl(stl / "switch.stl", material("switch", "#222222", 0.5), -0.8 * e)
    import_stl(stl / "pcb.stl", material("pcb", "#1f3b2a", 0.45))
    import_stl(stl / "parts_top.stl", material("metal", "#b8bcc2", 0.3, 1.0))
    import_stl(stl / "parts_sw.stl", material("switches", "#2a2a2a", 0.5))
    import_stl(stl / "parts_bottom.stl", material("parts", "#2b2d30", 0.45))
    lcd2 = import_stl(stl / "lcd2.stl", material("lcd", "#0b0c0d", 0.15))
    bpy.context.view_layer.update()
    zmin = min(p.z for ob in bpy.data.objects if ob.type == "MESH" for p in world_points(ob))
    add_screen(lcd2, png, 0.0)
    floor(zmin)
    lights()


def floor(z):
    bpy.ops.mesh.primitive_plane_add(size=2, location=(0.05, -0.04, z - 0.0001))
    fl = bpy.context.active_object
    fl.is_shadow_catcher = True


def lights():
    world = bpy.data.worlds.new("w")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (1, 1, 1, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25
    for name, loc, energy, size in (("key", (-0.15, -0.25, 0.35), 6, 0.25),
                                    ("fill", (0.30, -0.10, 0.20), 2, 0.35),
                                    ("rim", (0.10, 0.25, 0.25), 3, 0.2)):
        bpy.ops.object.light_add(type="AREA", location=loc)
        li = bpy.context.active_object
        li.name = name
        li.data.energy = energy
        li.data.size = size
        point_at(li, (0.048, -0.041, 0.0))


def point_at(ob, target):
    d = Vector(target) - ob.location
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def camera(kind, eye, target, lens):
    bpy.ops.object.camera_add(location=Vector(eye) * MM)
    cam = bpy.context.active_object
    if kind == "ortho":
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = lens * MM
        cam.rotation_euler = (0, 0, 0)
    else:
        cam.data.lens = lens
        point_at(cam, Vector(target) * MM)
    cam.data.clip_start = 0.001
    cam.data.clip_end = 10
    bpy.context.scene.camera = cam


def render(out, w, h, threads):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = int(os.environ.get("SAMPLES", "128"))
    sc.cycles.use_denoising = True
    sc.render.threads_mode = "FIXED"
    sc.render.threads = threads
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.film_transparent = True
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.exposure = -1.8
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.render.filepath = str(out)
    bpy.ops.render.render(write_still=True)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    stl, png, out = Path(args[0]), Path(args[1]), Path(args[2])
    views = args[3:] or list(VIEWS)
    threads = int(os.environ.get("THREADS", "6"))
    for v in views:
        w, h, kind, eye, target, lens, exploded = VIEWS[v]
        build_scene(stl, png, exploded)
        camera(kind, eye, target, lens)
        render(out / f"{v}.png", w, h, threads)


main()
