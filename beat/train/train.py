"""Train the beat-activation net (peepholes, causal) on the corrected Traktor labels; resumable across 20-minute jobs.

    train.py FEAT_DIR LABELS.json RUN_DIR [--steps N] [--budget-s S] [--pos-weight W] [--crop T] [--batch B]

Targets (Böck-style): 1 at the beat's frame (frame i is centred on i / 100 s), 0.5 on the two neighbours. Loss: BCE with
the beat frames weighted by --pos-weight. Each step: B random crops of T frames, zero initial state. Checkpoints
(model, optimizer, step, rng) go to RUN_DIR/ckpt.pt every CKPT_EVERY steps and on exit; a rerun resumes from it.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parent))

FPS = 100
CKPT_EVERY = 100
VAL_EVERY = 500


def targets(n_frames: int, beats: list[float]) -> np.ndarray:
    t = np.zeros(n_frames, np.float32)
    idx = np.round(np.asarray(beats) * FPS).astype(int)
    idx = idx[(idx >= 0) & (idx < n_frames)]
    for off in (-1, 1):
        j = idx + off
        j = j[(j >= 0) & (j < n_frames)]
        t[j] = np.maximum(t[j], 0.5)
    t[idx] = 1.0
    return t


def load_split(feat_dir: Path, labels: list[dict], split: str, speeds: tuple[float, ...] = ()):
    """(name, features, targets) per track; for each speed in `speeds` also the track played that much faster
    (prep_features --speed: tempo and pitch change, beat times divide by the speed)."""
    out = []
    for r in labels:
        if r["split"] != split:
            continue
        x = np.load(feat_dir / f"{r['name']}.npy", mmap_mode="r")
        out.append((r["name"], x, targets(len(x), r["beats"])))
        for sp in speeds:
            f = feat_dir / f"{r['name']}@{sp:g}.npy"
            if f.exists():
                xs = np.load(f, mmap_mode="r")
                out.append((f"{r['name']}@{sp:g}", xs, targets(len(xs), [b / sp for b in r["beats"]])))
    return out


def feature_stats(train, n=60, seed=0) -> tuple[np.ndarray, np.ndarray]:
    rng = np.random.default_rng(seed)
    pick = rng.choice(len(train), min(n, len(train)), replace=False)
    x = np.concatenate([np.asarray(train[i][1][::4]) for i in pick])
    return x.mean(0), np.maximum(x.std(0), 1e-3)


def batch(train, weights, rng, bsz: int, crop: int):
    xs, ys = [], []
    for i in rng.choice(len(train), bsz, p=weights):
        _, x, y = train[i]
        s = int(rng.integers(0, max(len(x) - crop, 1)))
        xs.append(np.asarray(x[s:s + crop]))
        ys.append(y[s:s + crop])
    return torch.from_numpy(np.stack(xs)), torch.from_numpy(np.stack(ys))


def loss_fn(logits, y, pos_weight: float):
    w = 1.0 + (pos_weight - 1.0) * (y > 0).float()
    return (torch.nn.functional.binary_cross_entropy_with_logits(logits, y, reduction="none") * w).mean()


@torch.no_grad()
def validate(model, val, seg: int = 6000, pos_weight: float = 1.0) -> float:
    xs, ys = [], []
    for _, x, y in val:
        s = max((len(x) - seg) // 2, 0)
        xs.append(np.asarray(x[s:s + seg]))
        ys.append(y[s:s + seg])
    n = min(len(a) for a in xs)
    xb = torch.from_numpy(np.stack([a[:n] for a in xs]))
    yb = torch.from_numpy(np.stack([a[:n] for a in ys]))
    logits, _ = model.logits(xb)
    return float(loss_fn(logits, yb, pos_weight))


LOG: Path | None = None


def log(msg: str) -> None:
    print(msg, flush=True)
    if LOG is not None:
        with LOG.open('a') as f:
            f.write(msg + '\n')


def main() -> None:
    global LOG
    from peeplstm import BeatNet
    ap = argparse.ArgumentParser()
    ap.add_argument("feat_dir")
    ap.add_argument("labels")
    ap.add_argument("run_dir")
    ap.add_argument("--steps", type=int, default=4000)
    ap.add_argument("--budget-s", type=float, default=1050)
    ap.add_argument("--pos-weight", type=float, default=3.0)
    ap.add_argument("--crop", type=int, default=1000)
    ap.add_argument("--batch", type=int, default=64)
    ap.add_argument("--lr", type=float, default=3e-3)
    ap.add_argument("--threads", type=int, default=12)
    ap.add_argument("--speeds", default="", help="comma list of augmentation speeds with prepared features")
    a = ap.parse_args()
    t_end = time.time() + a.budget_s
    torch.set_num_threads(a.threads)
    run = Path(a.run_dir)
    run.mkdir(parents=True, exist_ok=True)
    LOG = run / 'log.txt'
    labels = json.loads(Path(a.labels).read_text())
    feat_dir = Path(a.feat_dir)
    speeds = tuple(float(v) for v in a.speeds.split(",") if v)
    train, val = load_split(feat_dir, labels, "train", speeds), load_split(feat_dir, labels, "val")
    weights = np.array([len(x) for _, x, _ in train], float)
    weights /= weights.sum()
    model = BeatNet()
    opt = torch.optim.Adam(model.parameters(), lr=a.lr)
    step, ck = 0, run / "ckpt.pt"
    rng = np.random.default_rng(int(time.time()))
    if ck.exists():
        s = torch.load(ck, weights_only=False)
        model.load_state_dict(s["model"])
        opt.load_state_dict(s["opt"])
        step = s["step"]
    else:
        mean, std = feature_stats(train)
        model.mean.copy_(torch.from_numpy(mean))
        model.std.copy_(torch.from_numpy(std))
    log(f"train {len(train)} tracks, val {len(val)}; resume at step {step}")

    def save():
        torch.save({"model": model.state_dict(), "opt": opt.state_dict(), "step": step}, str(ck) + ".tmp")
        Path(str(ck) + ".tmp").rename(ck)

    t0, acc, n_acc = time.time(), 0.0, 0
    while step < a.steps and time.time() < t_end:
        lr = a.lr * (0.5 * (1 + np.cos(np.pi * step / a.steps)) * 0.97 + 0.03)
        for g in opt.param_groups:
            g["lr"] = lr
        x, y = batch(train, weights, rng, a.batch, a.crop)
        logits, _ = model.logits(x)
        loss = loss_fn(logits, y, a.pos_weight)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        step += 1
        acc, n_acc = acc + float(loss), n_acc + 1
        if step % 20 == 0:
            log(f"step {step} loss {acc / n_acc:.4f} lr {lr:.5f} {(time.time() - t0) / n_acc:.2f} s/step")
            acc, n_acc, t0 = 0.0, 0, time.time()
        if step % CKPT_EVERY == 0:
            save()
        if step % VAL_EVERY == 0:
            log(f"step {step} VAL loss {validate(model, val, pos_weight=a.pos_weight):.4f}")
    save()
    log(f"exit at step {step}")
    (run / "done").write_text("1") if step >= a.steps else None


if __name__ == "__main__":
    main()
