#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Checks that each STL is ONE closed, manifold solid: every edge shared by exactly two triangles, consistently
oriented, and all triangles in one connected body. Reads binary and ASCII STL. Usage: check_stl.py FILE.stl ..."""
import struct
import sys


def read_stl(path):
    data = open(path, "rb").read()
    if len(data) >= 84:
        n = struct.unpack_from("<I", data, 80)[0]
        if 84 + 50 * n == len(data):
            tris = []
            for i in range(n):
                v = struct.unpack_from("<12f", data, 84 + 50 * i)
                tris.append((v[3:6], v[6:9], v[9:12]))
            return tris
    tris, cur = [], []
    for line in data.decode("ascii", "replace").splitlines():
        p = line.split()
        if p and p[0] == "vertex":
            cur.append(tuple(float(x) for x in p[1:4]))
            if len(cur) == 3:
                tris.append(tuple(cur))
                cur = []
    return tris


def check(path):
    tris = read_stl(path)
    index, faces = {}, []
    for t in tris:
        ids = []
        for v in t:
            key = tuple(round(c, 5) for c in v)
            ids.append(index.setdefault(key, len(index)))
        if len(set(ids)) == 3:
            faces.append(ids)
    directed = {}
    for f, (a, b, c) in enumerate(faces):
        for e in ((a, b), (b, c), (c, a)):
            directed.setdefault(e, []).append(f)
    undirected = {}
    for (a, b), fs in directed.items():
        undirected.setdefault((min(a, b), max(a, b)), []).extend(fs)
    open_edges = sum(1 for fs in undirected.values() if len(fs) == 1)
    nonmanifold = sum(1 for fs in undirected.values() if len(fs) > 2)
    misoriented = sum(1 for e, fs in directed.items() if len(fs) > 1 or (e[1], e[0]) not in directed)
    parent = list(range(len(faces)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for fs in undirected.values():
        for f in fs[1:]:
            ra, rb = find(fs[0]), find(f)
            if ra != rb:
                parent[ra] = rb
    bodies = len({find(f) for f in range(len(faces))})
    ok = open_edges == 0 and nonmanifold == 0 and misoriented == 0 and bodies == 1 and faces
    print(f"{path}: {len(faces)} triangles, {open_edges} open edges, {nonmanifold} non-manifold edges, "
          f"{misoriented} misoriented edges, {bodies} bodies: {'OK' if ok else 'FAIL'}")
    return bool(ok)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    results = [check(p) for p in sys.argv[1:]]
    sys.exit(0 if all(results) else 1)
