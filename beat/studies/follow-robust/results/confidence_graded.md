# A graded confidence (all-d-f60)

Fit on the dev half, judged on the test half: running ticks, right (within 1 %) against wrong tempo. Balanced classes in the fit.

| value | dev AUC | test AUC | test: share of ticks in 0-20 / 20-40 / 40-60 / 60-80 / 80-100 % |
|---|---|---|---|
| gate (agreement mean) | 0.789 | 0.898 | 0 / 0 / 0 / 2 / 98 |
| soft | 0.880 | 0.916 | 2 / 2 / 22 / 74 / 0 |
| soft+jit | 0.857 | 0.923 | 8 / 1 / 1 / 90 / 0 |
| soft+jit+tempo+phase | 0.865 | 0.929 | 6 / 2 / 1 / 90 / 0 |
| soft+jit+tempo+phase+dbn | 0.868 | 0.936 | 6 / 2 / 2 / 90 / 0 |
| all | 0.869 | 0.934 | 9 / 2 / 86 / 3 / 0 |

## Calibration of soft+jit+tempo+phase+dbn on the test half (the share right per band of the value)

- 0-20 %:   46419 ticks,  91 % right
- 20-40 %:   13390 ticks,  95 % right
- 40-60 %:   11924 ticks,  97 % right
- 60-80 %:  678941 ticks, 100 % right
- 80-100 %:    3210 ticks, 100 % right

Weights (p = 1 / (1 + exp(-(b + sum w_i x_i)))), x in 0..1:

- b = -4.1474
- w_soft = 0.5981
- w_jit = 2.9058
- w_tempo = 2.3333
- w_phase = -1.4201
- w_dbn = 1.1417
