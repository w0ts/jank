"""Speed of one training step: bench.py BATCH T THREADS"""
import sys
import time
from pathlib import Path

import torch

sys.path.insert(0, str(Path(__file__).resolve().parent))


def main() -> None:
    from peeplstm import BeatNet
    b, t, th = map(int, sys.argv[1:4])
    torch.set_num_threads(th)
    m = BeatNet()
    opt = torch.optim.Adam(m.parameters())
    x = torch.rand(b, t, 162)
    y = (torch.rand(b, t) < 0.04).float()
    for i in range(3):
        t0 = time.time()
        loss = torch.nn.functional.binary_cross_entropy_with_logits(m.logits(x)[0], y)
        opt.zero_grad()
        loss.backward()
        opt.step()
        print(f"step {i}: {time.time() - t0:.2f} s", flush=True)


if __name__ == "__main__":
    main()
