# The confidence: how graded is it, and what predicts a right output

## fw: ticks with the output running (4557764 ticks, 5144724 playing)

| value | 0-20 | 20-40 | 40-60 | 60-80 | 80-100 | share < 0.05 | share > 0.95 | steps > 0.3 | mean step / s |
|---|---|---|---|---|---|---|---|---|---|
| conf | 0 % | 0 % | 0 % | 0 % | 100 % | 0 % | 97 % | 0.0 % | 0.001 |
| strict | 0 % | 0 % | 0 % | 1 % | 98 % | 0 % | 92 % | 0.0 % | 0.100 |
| loose | 0 % | 0 % | 0 % | 0 % | 100 % | 0 % | 97 % | 0.0 % | 0.001 |
| soft | 0 % | 0 % | 0 % | 2 % | 97 % | 0 % | 62 % | 0.0 % | 0.065 |
| jit | 7 % | 0 % | 0 % | 2 % | 91 % | 7 % | 16 % | 0.1 % | 0.011 |
| pk | 0 % | 0 % | 0 % | 0 % | 100 % | 0 % | 100 % | 0.0 % | 0.000 |

AUC for right vs wrong (running, playing, known reference), per split; higher = the value tells right from wrong.

| value | dev AUC | test AUC |
|---|---|---|
| conf | 0.577 | 0.616 |
| strict | 0.768 | 0.875 |
| loose | 0.577 | 0.615 |
| tempo | - | - |
| phase | - | - |
| dbn | - | - |
| soft | 0.874 | 0.936 |
| jit | 0.840 | 0.934 |
| pk | 0.503 | 0.540 |

## all-d-f60: ticks with the output running (4665668 ticks, 5144724 playing)

| value | 0-20 | 20-40 | 40-60 | 60-80 | 80-100 | share < 0.05 | share > 0.95 | steps > 0.3 | mean step / s |
|---|---|---|---|---|---|---|---|---|---|
| conf | 0 % | 0 % | 0 % | 3 % | 97 % | 0 % | 92 % | 0.0 % | 0.007 |
| strict | 1 % | 1 % | 1 % | 3 % | 95 % | 0 % | 89 % | 0.0 % | 0.099 |
| loose | 0 % | 0 % | 1 % | 1 % | 97 % | 0 % | 94 % | 0.0 % | 0.004 |
| tempo | 0 % | 0 % | 0 % | 1 % | 98 % | 0 % | 94 % | 0.0 % | 0.005 |
| phase | 0 % | 0 % | 1 % | 1 % | 97 % | 0 % | 96 % | 0.0 % | 0.003 |
| dbn | 1 % | 0 % | 0 % | 1 % | 98 % | 0 % | 89 % | 0.1 % | 0.020 |
| soft | 0 % | 1 % | 1 % | 3 % | 95 % | 0 % | 61 % | 0.0 % | 0.065 |
| jit | 8 % | 1 % | 0 % | 2 % | 88 % | 8 % | 16 % | 0.1 % | 0.011 |
| pk | 0 % | 0 % | 0 % | 0 % | 100 % | 0 % | 100 % | 0.0 % | 0.000 |

AUC for right vs wrong (running, playing, known reference), per split; higher = the value tells right from wrong.

| value | dev AUC | test AUC |
|---|---|---|
| conf | 0.789 | 0.898 |
| strict | 0.803 | 0.874 |
| loose | 0.623 | 0.675 |
| tempo | 0.732 | 0.730 |
| phase | 0.607 | 0.679 |
| dbn | 0.563 | 0.664 |
| soft | 0.880 | 0.916 |
| jit | 0.844 | 0.908 |
| pk | 0.505 | 0.552 |

