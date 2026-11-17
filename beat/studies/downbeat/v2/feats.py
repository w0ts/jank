"""Beat-synchronous descriptors from the tracker's own 81-band log spectrogram (no extra FFT on the device).

Each tracked beat j opens a SEGMENT: the frames from beat j to beat j+1 (both known once beat j+1 is tracked), so every
descriptor of beat j is ready at beat j+1. Per beat:

    E_g   mean log level of band group g over the segment (8 groups)                         8
    O_g   onset at the beat: max over [-50, +100] ms of the positive flux summed over group g  8
    dE_g  E_g(j) - E_g(j-1): the level change into this beat                                 8
    cc1   chroma change, segment j against segment j-1 (1 - cosine)                          1
    cc2   chroma change, segments j..j+1 against j-2..j-1                                     1
    nov2  level change summed over groups, segments j..j+1 against j-2..j-1                   1
    tr    the tracker's local timing: (gap to the next beat) / (median of the last 8 gaps)    1

The chroma: each band from 250 Hz to 4.2 kHz adds its linear level to the pitch class of its centre frequency.
cc2 and nov2 use two segments after the beat, so they are ready at beat j+2: the model below waits for them.
"""
from __future__ import annotations

import numpy as np
from numba import njit

FPS = 100
# band groups of the 81 bands (indexes, by centre): 43-86 Hz, 108-172, 194-345, 366-689, 732-1400, 1.5-3k, 3.1-5.9k, 6.3-16k
GROUPS = ((0, 3), (3, 7), (7, 15), (15, 27), (27, 39), (39, 52), (52, 64), (64, 81))
CENTRES = np.array([43, 65, 86, 108, 129, 151, 172, 194, 215, 237, 258, 280, 301, 323, 345, 366, 388, 409, 431, 452, 495,
                    517, 538, 581, 624, 646, 689, 732, 775, 840, 883, 926, 991, 1055, 1098, 1184, 1249, 1314, 1400, 1486,
                    1572, 1658, 1766, 1873, 1981, 2089, 2218, 2347, 2498, 2627, 2799, 2950, 3144, 3316, 3510, 3725, 3941,
                    4177, 4436, 4694, 4974, 5276, 5577, 5922, 6266, 6654, 7041, 7450, 7903, 8376, 8872, 9388, 9948, 10551,
                    11176, 11843, 12554, 13286, 14083, 14923, 15805], dtype=float)
CHROMA_BANDS = np.flatnonzero((CENTRES >= 250) & (CENTRES <= 4200))
CHROMA_PC = (np.round(12 * np.log2(CENTRES[CHROMA_BANDS] / 440.0)).astype(int) % 12)
NG = len(GROUPS)
NAMES = (*(f"E{g}" for g in range(NG)), *(f"O{g}" for g in range(NG)), *(f"dE{g}" for g in range(NG)),
         "cc1", "cc2", "nov2", "tr")
ONSET_WIN = (-5, 10)          # frames around the beat


def _group_means(L: np.ndarray) -> np.ndarray:
    return np.stack([L[:, a:b].mean(1) for a, b in GROUPS], axis=1)


def _seg_means(X: np.ndarray, k0: np.ndarray, k1: np.ndarray) -> np.ndarray:
    """Mean of X's rows over [k0, k1) per beat (k1 > k0 forced)."""
    cs = np.vstack([np.zeros((1, X.shape[1])), np.cumsum(X, axis=0, dtype=np.float64)])
    k1 = np.maximum(k1, k0 + 1)
    return (cs[k1] - cs[k0]) / (k1 - k0)[:, None]


def _cos_change(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    na, nb = np.linalg.norm(a, axis=1), np.linalg.norm(b, axis=1)
    return 1.0 - (a * b).sum(1) / np.maximum(na * nb, 1e-9)


def periods(t: np.ndarray) -> np.ndarray:
    """The median of the last 8 gaps at each beat (the gap before it included); 0.5 s before the second beat."""
    g = np.diff(t)
    out = np.full(len(t), 0.5)
    for i in range(1, len(t)):
        out[i] = np.median(g[max(0, i - 8):i])
    return out


def describe(L: np.ndarray, t: np.ndarray) -> np.ndarray:
    """(len(t), len(NAMES)) float32 descriptors; rows whose segment runs past the audio are filled from what exists."""
    L = np.asarray(L, dtype=np.float32)
    n = len(L)
    m = len(t)
    out = np.zeros((m, len(NAMES)), np.float32)
    if m < 3:
        return out
    k = np.clip(np.round(t * FPS).astype(int), 0, n - 1)
    per = periods(t)
    k_next = np.concatenate([k[1:], [min(n, k[-1] + int(round(per[-1] * FPS)))]])
    G = _group_means(L)
    E = _seg_means(G, k, k_next)
    d = np.vstack([np.zeros((1, L.shape[1]), np.float32), np.maximum(np.diff(L, axis=0), 0.0)])
    flux = np.stack([d[:, a:b].sum(1) for a, b in GROUPS], axis=1)
    Ons = np.zeros((m, NG), np.float32)
    for j in range(m):
        a, b = max(0, k[j] + ONSET_WIN[0]), min(n, k[j] + ONSET_WIN[1] + 1)
        if b > a:
            Ons[j] = flux[a:b].max(0)
    lin = 10.0 ** L[:, CHROMA_BANDS] - 1.0
    chroma_f = np.zeros((n, 12), np.float32)
    np.add.at(chroma_f.T, CHROMA_PC, lin.T)
    C = _seg_means(chroma_f, k, k_next)
    dE = np.vstack([np.zeros((1, NG)), np.diff(E, axis=0)])
    Cp = np.vstack([C[:1], C[:-1]])
    cc1 = _cos_change(C, Cp)
    # two segments after against two before: j, j+1 vs j-2, j-1 (edges repeat the nearest segment)
    idx = np.arange(m)

    def at(X, off):
        return X[np.clip(idx + off, 0, m - 1)]
    cc2 = _cos_change(at(C, 0) + at(C, 1), at(C, -2) + at(C, -1))
    nov2 = np.abs((at(E, 0) + at(E, 1)) - (at(E, -2) + at(E, -1))).sum(1) / 2
    gaps = np.concatenate([np.diff(t), [per[-1]]])
    tr = gaps / np.maximum(per, 1e-3)
    out[:] = np.column_stack([E, Ons, dE, cc1, cc2, nov2, tr])
    return out


def running_norm(D: np.ndarray, tau: float = 64.0) -> np.ndarray:
    """Causal per-feature z-score with exponential forgetting (time constant tau beats): beat j is normalised with the
    mean and variance of beats <= j. On the device: 2 floats per feature."""
    return _running_norm(np.asarray(D, np.float64), 1.0 / tau).astype(np.float32)


@njit(cache=True)
def _running_norm(D, a):
    mu = D[0].copy()
    var = np.ones(D.shape[1])
    out = np.empty_like(D)
    for j in range(len(D)):
        x = D[j]
        mu += a * (x - mu)
        var += a * ((x - mu) ** 2 - var)
        out[j] = (x - mu) / np.sqrt(var + 1e-6)
    return out
