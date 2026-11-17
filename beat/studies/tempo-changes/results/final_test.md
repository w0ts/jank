### test half, all tracks (one line per chain)

| chain | steady: held | stopped | wrong locks / track | range breaks / track | flips / track | changing: right | stopped | wrong | changes right within 16 beats | beat-less: stopped | running after | abs error after (ms) | half-time: right |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| today | 0.859 | 0.119 | 0.15 | 0.009 | 3.91 | 0.602 | 0.375 | 0.017 | 0.02 (50) | 0.604 | 0.45 | 26 | 0.153 |
| m2i | 0.846 | 0.128 | 0.26 | 0.003 | 4.01 | 0.592 | 0.380 | 0.021 | 0.02 (50) | 0.601 | 0.42 | 35 | 0.147 |
| m2i+rate | 0.832 | 0.129 | 0.31 | 0.003 | 4.60 | 0.587 | 0.380 | 0.022 | 0.02 (50) | 0.597 | 0.42 | 36 | 0.147 |
| m2i+c3 | 0.840 | 0.128 | 0.30 | 0.003 | 4.28 | 0.589 | 0.382 | 0.021 | 0.02 (50) | 0.600 | 0.42 | 35 | 0.147 |
| tuned | 0.859 | 0.102 | 0.33 | 0.000 | 4.43 | 0.618 | 0.330 | 0.043 | 0.06 (50) | 0.356 | 0.62 | 21 | 0.248 |
| tuned-l16 | 0.858 | 0.105 | 0.20 | 0.000 | 3.84 | 0.614 | 0.334 | 0.043 | 0.04 (50) | 0.386 | 0.62 | 18 | 0.233 |
| tuned-hard | 0.849 | 0.114 | 0.15 | 0.000 | 3.79 | 0.615 | 0.345 | 0.032 | 0.02 (50) | 0.388 | 0.62 | 28 | 0.232 |
| tuned-hard-norate | 0.856 | 0.117 | 0.09 | 0.000 | 3.37 | 0.610 | 0.353 | 0.030 | 0.02 (50) | 0.389 | 0.59 | 20 | 0.212 |

### test half, steady tracks

| chain | tracks | stopped | right (1 %) | held (0.5 %) | wrong | wrong locks / track | range breaks / track | flips / track | in phase |
|---|---|---|---|---|---|---|---|---|---|
| today | 351 | 0.119 | 0.864 | 0.859 | 0.013 | 0.15 | 0.01 | 3.91 | 0.779 |
| m2i | 351 | 0.128 | 0.853 | 0.846 | 0.015 | 0.26 | 0.00 | 4.01 | 0.772 |
| m2i+rate | 351 | 0.129 | 0.849 | 0.832 | 0.016 | 0.31 | 0.00 | 4.60 | 0.768 |
| m2i+c3 | 351 | 0.128 | 0.851 | 0.840 | 0.015 | 0.30 | 0.00 | 4.28 | 0.770 |
| tuned | 351 | 0.102 | 0.872 | 0.859 | 0.020 | 0.33 | 0.00 | 4.43 | 0.787 |
| tuned-l16 | 351 | 0.105 | 0.870 | 0.858 | 0.018 | 0.20 | 0.00 | 3.84 | 0.785 |
| tuned-hard | 351 | 0.114 | 0.864 | 0.849 | 0.016 | 0.15 | 0.00 | 3.79 | 0.781 |
| tuned-hard-norate | 351 | 0.117 | 0.865 | 0.856 | 0.014 | 0.09 | 0.00 | 3.37 | 0.783 |

| chain | real changes followable | median delay (beats) | median delay (s) | within 8 beats | within 16 beats | not within 30 s |
|---|---|---|---|---|---|---|

| chain | beat-less passages | stopped in them | running at the first strong beat after | median abs phase error there (ms) | within 50 ms | median of the first 4 strong beats (ms) | half-time sections: right |
|---|---|---|---|---|---|---|---|
| today | 107 | 0.530 | 0.53 | 26 | 0.28 | 26 | 0.198 (17 tracks) |
| m2i | 107 | 0.529 | 0.50 | 35 | 0.26 | 35 | 0.190 (17 tracks) |
| m2i+rate | 107 | 0.524 | 0.49 | 36 | 0.25 | 36 | 0.190 (17 tracks) |
| m2i+c3 | 107 | 0.529 | 0.50 | 35 | 0.26 | 35 | 0.190 (17 tracks) |
| tuned | 107 | 0.269 | 0.74 | 21 | 0.44 | 21 | 0.320 (17 tracks) |
| tuned-l16 | 107 | 0.302 | 0.73 | 18 | 0.48 | 15 | 0.301 (17 tracks) |
| tuned-hard | 107 | 0.305 | 0.73 | 28 | 0.46 | 24 | 0.300 (17 tracks) |
| tuned-hard-norate | 107 | 0.307 | 0.71 | 23 | 0.47 | 15 | 0.274 (17 tracks) |

### test half, changing tracks

| chain | tracks | stopped | right (1 %) | held (0.5 %) | wrong | wrong locks / track | range breaks / track | flips / track | in phase |
|---|---|---|---|---|---|---|---|---|---|
| today | 8 | 0.600 | 0.386 | 0.378 | 0.011 | 0.38 | 0.00 | 1.62 | 0.372 |
| m2i | 8 | 0.593 | 0.380 | 0.358 | 0.020 | 0.88 | 0.00 | 2.38 | 0.363 |
| m2i+rate | 8 | 0.592 | 0.377 | 0.352 | 0.022 | 0.88 | 0.00 | 2.88 | 0.360 |
| m2i+c3 | 8 | 0.592 | 0.376 | 0.353 | 0.022 | 0.88 | 0.00 | 3.00 | 0.360 |
| tuned | 8 | 0.552 | 0.402 | 0.380 | 0.038 | 1.25 | 0.00 | 3.38 | 0.385 |
| tuned-l16 | 8 | 0.583 | 0.390 | 0.371 | 0.022 | 1.00 | 0.00 | 3.00 | 0.371 |
| tuned-hard | 8 | 0.598 | 0.377 | 0.359 | 0.019 | 0.75 | 0.00 | 3.12 | 0.360 |
| tuned-hard-norate | 8 | 0.594 | 0.389 | 0.373 | 0.014 | 0.50 | 0.00 | 2.00 | 0.372 |

| chain | real changes followable | median delay (beats) | median delay (s) | within 8 beats | within 16 beats | not within 30 s |
|---|---|---|---|---|---|---|

| chain | beat-less passages | stopped in them | running at the first strong beat after | median abs phase error there (ms) | within 50 ms | median of the first 4 strong beats (ms) | half-time sections: right |
|---|---|---|---|---|---|---|---|
| today | 6 | 0.919 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| m2i | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| m2i+rate | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| m2i+c3 | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| tuned | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| tuned-l16 | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| tuned-hard | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |
| tuned-hard-norate | 6 | 0.833 | 0.00 | nan | 0.00 | nan | 0.000 (1 tracks) |

### test half, probable tracks

| chain | tracks | stopped | right (1 %) | held (0.5 %) | wrong | wrong locks / track | range breaks / track | flips / track | in phase |
|---|---|---|---|---|---|---|---|---|---|
| today | 24 | 0.300 | 0.674 | 0.666 | 0.018 | 0.25 | 0.00 | 4.08 | 0.583 |
| m2i | 24 | 0.309 | 0.663 | 0.653 | 0.021 | 0.33 | 0.00 | 4.62 | 0.580 |
| m2i+rate | 24 | 0.310 | 0.657 | 0.639 | 0.022 | 0.38 | 0.00 | 5.25 | 0.576 |
| m2i+c3 | 24 | 0.312 | 0.659 | 0.648 | 0.021 | 0.33 | 0.00 | 4.79 | 0.578 |
| tuned | 24 | 0.256 | 0.690 | 0.678 | 0.044 | 0.38 | 0.04 | 4.67 | 0.600 |
| tuned-l16 | 24 | 0.252 | 0.689 | 0.677 | 0.050 | 0.46 | 0.04 | 4.71 | 0.597 |
| tuned-hard | 24 | 0.260 | 0.694 | 0.683 | 0.037 | 0.17 | 0.00 | 4.54 | 0.602 |
| tuned-hard-norate | 24 | 0.272 | 0.684 | 0.677 | 0.036 | 0.17 | 0.00 | 4.17 | 0.597 |

| chain | real changes followable | median delay (beats) | median delay (s) | within 8 beats | within 16 beats | not within 30 s |
|---|---|---|---|---|---|---|
| today | 6 (of 36) | nan | nan | 0.00 | 0.00 | 1.00 |
| m2i | 6 (of 36) | nan | nan | 0.00 | 0.00 | 1.00 |
| m2i+rate | 6 (of 36) | nan | nan | 0.00 | 0.00 | 1.00 |
| m2i+c3 | 6 (of 36) | nan | nan | 0.00 | 0.00 | 1.00 |
| tuned | 7 (of 36) | 25.1 | 10.8 | 0.00 | 0.00 | 0.86 |
| tuned-l16 | 7 (of 36) | 25.1 | 10.8 | 0.00 | 0.00 | 0.86 |
| tuned-hard | 7 (of 36) | 25.1 | 10.8 | 0.00 | 0.00 | 0.86 |
| tuned-hard-norate | 7 (of 36) | 25.1 | 10.8 | 0.00 | 0.00 | 0.86 |

| chain | beat-less passages | stopped in them | running at the first strong beat after | median abs phase error there (ms) | within 50 ms | median of the first 4 strong beats (ms) | half-time sections: right |
|---|---|---|---|---|---|---|---|
| today | 13 | 0.712 | 0.23 | 8 | 0.15 | 8 | 0.000 (1 tracks) |
| m2i | 13 | 0.705 | 0.15 | 6 | 0.15 | 3 | 0.000 (1 tracks) |
| m2i+rate | 13 | 0.705 | 0.15 | 6 | 0.15 | 4 | 0.000 (1 tracks) |
| m2i+c3 | 13 | 0.705 | 0.15 | 6 | 0.15 | 3 | 0.000 (1 tracks) |
| tuned | 13 | 0.434 | 0.31 | 8 | 0.31 | 3 | 0.000 (1 tracks) |
| tuned-l16 | 13 | 0.462 | 0.31 | 8 | 0.31 | 3 | 0.000 (1 tracks) |
| tuned-hard | 13 | 0.462 | 0.31 | 7 | 0.31 | 4 | 0.000 (1 tracks) |
| tuned-hard-norate | 13 | 0.462 | 0.31 | 9 | 0.31 | 4 | 0.000 (1 tracks) |

